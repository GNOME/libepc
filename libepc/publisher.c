/* Easy Publish and Consume Library
 * Copyright (C) 2007  Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *      Mathias Hasselmann
 */

#include "publisher.h"
#include "dispatcher.h"
#include "enums.h"
#include "shell.h"
#include "tls.h"

#include <glibconfig.h>
#include <glib/gi18n-lib.h>

#include <libsoup/soup-address.h>
#include <libsoup/soup-server.h>
#include <libsoup/soup-server-auth.h>
#include <libsoup/soup-server-message.h>
#include <libsoup/soup-socket.h>

#include <string.h>

/**
 * SECTION:auth-context
 * @short_description: manage authentication
 * @see_also: #EpcPublisher
 * @include: libepc/publish.h
 * @stability: Unstable
 *
 * With each request the #EpcPublisher verifies access authorization by calling
 * the #EpcAuthHandler registered for the key in question, if any. Information about
 * that process is stored in the #EpcAuthContext structure.
 *
 * To register an authentication handler call #epc_publisher_set_auth_handler:
 *
 * <example id="register-auth-handler">
 *  <title>Register an authentication handler</title>
 *  <programlisting>
 *   epc_publisher_set_auth_handler (publisher, "sensitive-key",
 *                                   my_auth_handler, my_object);
 *  </programlisting>
 * </example>
 *
 * To verify that the user password provided password matches
 * the expected one use #epc_auth_context_check_password:
 *
 * <example id="check-password">
 *  <title>Verify a password</title>
 *  <programlisting>
 *   static gboolean
 *   my_auth_handler (EpcAuthContext *context,
 *                    const gchar    *username,
 *                    gpointer        user_data)
 *   {
 *     MyObject *self = user_data;
 *     const gchar *expected_password;
 *     const gchar *requested_key;
 *
 *     requested_key = epc_auth_context_get_key (context);
 *     expected_password = lookup_password (self, requested_key);
 *
 *     return epc_auth_context_check_password (context, expected_password);
 *   }
 *  </programlisting>
 * </example>
 */

/**
 * SECTION:publisher
 * @short_description: easily publish values
 * @see_also: #EpcConsumer, #EpcAuthContext, #EpcContentsHandler
 * @include: libepc/publish.h
 * @stability: Unstable
 *
 * The #EpcPublisher starts a HTTP server to publish information.
 * To allow #EpcConsumer to find the publisher it automatically publishes
 * its contact information (host name, TCP/IP port) per DNS-SD.
 *
 * In future it might use DNS-DS to notify #EpcConsumer of changes.
 *
 * <example id="publish-value">
 *  <title>Publish a value</title>
 *  <programlisting>
 *   publisher = epc_publisher_new ("Easy Publisher Example", NULL, NULL);
 *
 *   epc_publisher_add (publisher, "maman", "bar", -1);
 *   epc_publisher_add_file (publisher, "source-code", __FILE__);
 *
 *   epc_publisher_run (NULL);
 *  </programlisting>
 * </example>
 *
 * #EpcPublisher doesn't provide a way to explicitly publish %NULL values, as
 * publishing %NULL values doesn't seem very valueable in our scenario: Usually
 * you want to "publish" %NULL values to express, that your application doesn't
 * have any meaningful information for the requested identifier. By "publishing"
 * a %NULL value essentially you say "this information does not exist". So
 * publishing %NULL values is not different from not publishing any value at
 * all or rejected access to some values. Without explicitly inspecting the
 * details for not receiving a value, a consumer calling #epc_consumer_lookup
 * has no chance to distinguish between the cases "never published", "network
 * problem", "authorization rejected", "no meaningful value available".
 *
 * So if  feel like publishing a %NULL value, just remove the key in question
 * from the #EpcPublisher by calling #epc_publisher_remove. When using a
 * custom #EpcContentsHandler an alternate approach is returning %NULL from
 * that handler. In that case the #EpcPublisher will behave exactly the same,
 * as if the value has been removed.
 */

typedef struct _EpcListContext EpcListContext;
typedef struct _EpcResource    EpcResource;

enum
{
  PROP_NONE,
  PROP_PROTOCOL,
  PROP_APPLICATION,
  PROP_SERVICE_NAME,
  PROP_SERVICE_DOMAIN,
  PROP_SERVICE_COOKIE,
  PROP_COLLISION_HANDLING,

  PROP_AUTH_FLAGS,
  PROP_CONTENTS_PATH,
  PROP_CERTIFICATE_FILE,
  PROP_PRIVATE_KEY_FILE,
};

/**
 * EpcAuthContext:
 *
 * This data structure describes a pending authentication request
 * which shall be verified by an #EpcAuthHandler installed by
 * #epc_publisher_set_auth_handler.
 *
 * <note><para>
 *  There is no way to retrieve the password from the #EpcAuthContext, as
 *  the network protocol transfers just a hash code, not the actual password.
 * </para></note>
 */
struct _EpcAuthContext
{
  /*< private >*/
  SoupServerAuth *auth;
  EpcPublisher   *publisher;
  const gchar    *key;

  /*< public >*/
};

struct _EpcListContext
{
  GPatternSpec *pattern;
  GList *matches;
};

struct _EpcResource
{
  EpcContentsHandler handler;
  gpointer           user_data;
  GDestroyNotify     destroy_data;

  EpcAuthHandler     auth_handler;
  gpointer           auth_user_data;
  GDestroyNotify     auth_destroy_data;

  EpcDispatcher     *dispatcher;
};

/**
 * EpcPublisherPrivate:
 *
 * Private fields of the #EpcPublisher class.
 */
struct _EpcPublisherPrivate
{
  EpcDispatcher         *dispatcher;

  GHashTable            *resources;
  EpcResource           *default_resource;
  gchar                 *default_bookmark;

  gboolean               server_started;
  GMainLoop             *server_loop;
  SoupServerAuthContext  server_auth;
  SoupServer            *server;

  GHashTable            *clients;

  EpcProtocol            protocol;
  gchar                 *application;
  gchar                 *service_name;
  gchar                 *service_domain;
  gchar                 *service_cookie;

  EpcAuthFlags           auth_flags;
  EpcCollisionHandling   collisions;
  gchar                 *contents_path;
  gchar                 *certificate_file;
  gchar                 *private_key_file;
};

static GStaticRecMutex epc_publisher_lock;

G_DEFINE_TYPE (EpcPublisher, epc_publisher, G_TYPE_OBJECT);

static EpcResource*
epc_resource_new (EpcContentsHandler handler,
                  gpointer           user_data,
                  GDestroyNotify     destroy_data)
{
  EpcResource *self = g_slice_new0 (EpcResource);

  self->handler = handler;
  self->user_data = user_data;
  self->destroy_data = destroy_data;

  return self;
}

static void
epc_resource_free (gpointer data)
{
  EpcResource *self = data;

  if (self->dispatcher)
    g_object_unref (self->dispatcher);
  if (self->destroy_data)
    self->destroy_data (self->user_data);
  if (self->auth_destroy_data)
    self->auth_destroy_data (self->auth_user_data);

  g_slice_free (EpcResource, self);
}

static void
epc_resource_set_auth_handler (EpcResource    *self,
                               EpcAuthHandler  handler,
                               gpointer        user_data,
                               GDestroyNotify  destroy_data)

{
  if (self->auth_destroy_data)
    self->auth_destroy_data (self->auth_user_data);

  self->auth_handler = handler;
  self->auth_user_data = user_data;
  self->auth_destroy_data = destroy_data;
}

static void
epc_resource_announce (EpcResource *self,
                       const gchar *label)
{
  if (!self->dispatcher)
    {
      GError *error = NULL;

      self->dispatcher = epc_dispatcher_new (label);

      /* TODO: real error reporting */
      if (!epc_dispatcher_run (self->dispatcher, &error))
        {
          g_warning ("%s: %s", G_STRFUNC, error->message);
          g_clear_error (&error);
        }
    }
  else
    epc_dispatcher_set_name (self->dispatcher, label);
}

static EpcContents*
epc_publisher_handle_static (EpcPublisher *publisher G_GNUC_UNUSED,
                             const gchar  *key G_GNUC_UNUSED,
                             gpointer      user_data)
{
  return epc_contents_ref (user_data);
}

static EpcContents*
epc_publisher_handle_file (EpcPublisher *publisher G_GNUC_UNUSED,
                           const gchar  *key G_GNUC_UNUSED,
                           gpointer      user_data)
{
  const gchar *filename = user_data;
  EpcContents *contents = NULL;
  gchar *data = NULL;
  gsize length = 0;

  /* TODO: use gio to determinate mime-type */
  if (g_file_get_contents (filename, &data, &length, NULL))
    contents = epc_contents_new (NULL, data, length, g_free);

  return contents;
}

static G_CONST_RETURN gchar*
epc_publisher_get_key (const gchar *path)
{
  const gchar *key;

  g_return_val_if_fail (NULL != path, NULL);
  g_return_val_if_fail ('/' == *path, NULL);

  key = strchr (path + 1, '/');

  if (key)
    key += 1;

  return key;
}

static void
epc_publisher_chunk_cb (SoupMessage *message,
                        gpointer     data)
{
  EpcContents *contents = data;
  gconstpointer chunk;
  gsize length;

  chunk = epc_contents_stream_read (contents, &length);

  if (chunk && length)
    {
      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: writing %d bytes", G_STRLOC, length);

      soup_message_add_chunk (message, SOUP_BUFFER_USER_OWNED, chunk, length);
    }
  else
    {
      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: done", G_STRLOC);

      soup_message_add_final_chunk (message);
    }
}

static void
epc_publisher_trace_client (const gchar *strfunc,
                            const gchar *message,
                            SoupSocket  *socket)
{
  SoupAddress *addr = soup_socket_get_remote_address (socket);

  g_debug ("%s: %s: %s:%d", strfunc, message,
           soup_address_get_physical (addr),
           soup_address_get_port (addr));
}

static gboolean
epc_publisher_check_client (EpcPublisher      *self,
                            SoupServerContext *context)
{
  if (context->server == self->priv->server)
    return TRUE;

  if (EPC_DEBUG_LEVEL (1))
    epc_publisher_trace_client (G_STRFUNC, "stale client", context->sock);

  soup_socket_disconnect (context->sock);
  return FALSE;
}

static G_GNUC_WARN_UNUSED_RESULT gboolean
epc_publisher_track_client (EpcPublisher      *self,
                            SoupServerContext *context)
{
  g_static_rec_mutex_lock (&epc_publisher_lock);

  if (epc_publisher_check_client (self, context))
    {
      gpointer tag;

      tag = g_hash_table_lookup (self->priv->clients, context->sock);
      tag = GINT_TO_POINTER (GPOINTER_TO_INT (tag) + 1);

      g_object_ref (context->sock);
      g_hash_table_replace (self->priv->clients, context->sock, tag);

      return TRUE;
    }
  else
    g_static_rec_mutex_unlock (&epc_publisher_lock);

  return FALSE;
}

static void
epc_publisher_untrack_client (EpcPublisher      *self,
                              SoupServerContext *context)
{
  if (epc_publisher_check_client (self, context))
    {
      gpointer tag;

      tag = g_hash_table_lookup (self->priv->clients, context->sock);
      tag = GINT_TO_POINTER (GPOINTER_TO_INT (tag) - 1);

      g_object_ref (context->sock);
      g_hash_table_replace (self->priv->clients, context->sock, tag);
    }

  g_static_rec_mutex_unlock (&epc_publisher_lock);
}

static void
epc_publisher_handle_contents (SoupServerContext *context,
                               SoupMessage       *message,
                               gpointer           data)
{
  EpcPublisher *self = EPC_PUBLISHER (data);
  EpcResource *resource = NULL;
  EpcContents *contents = NULL;
  const gchar *key = NULL;

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: method=%s, path=%s", G_STRFUNC, message->method, context->path);

  if (SOUP_METHOD_ID_GET != context->method_id)
    {
      soup_message_set_status (message, SOUP_STATUS_METHOD_NOT_ALLOWED);
      return;
    }

  if (!epc_publisher_track_client (self, context))
    return;

  key = epc_publisher_get_key (context->path);

  if (key)
    resource = g_hash_table_lookup (self->priv->resources, key);
  if (resource && resource->handler)
    contents = resource->handler (self, key, resource->user_data);

  soup_message_set_status (message, SOUP_STATUS_NOT_FOUND);

  if (contents)
    {
      gconstpointer data;
      const gchar *type;
      gsize length = 0;

      data = epc_contents_get_data (contents, &length);
      type = epc_contents_get_mime_type (contents);

      if (data)
        {
          soup_message_set_response (message, type, SOUP_BUFFER_USER_OWNED, (gpointer) data, length);
          soup_message_set_status (message, SOUP_STATUS_OK);
        }
      else if (epc_contents_is_stream (contents))
        {
          g_signal_connect (message, "wrote-chunk", G_CALLBACK (epc_publisher_chunk_cb), contents);
          g_signal_connect (message, "wrote-headers", G_CALLBACK (epc_publisher_chunk_cb), contents);

          soup_server_message_set_encoding (SOUP_SERVER_MESSAGE (message), SOUP_TRANSFER_CHUNKED);
          soup_message_set_status (message, SOUP_STATUS_OK);
        }

      g_signal_connect_swapped (message, "finished", G_CALLBACK (epc_contents_unref), contents);
    }

  epc_publisher_untrack_client (self, context);
}

static void
epc_publisher_handle_list (SoupServerContext *context,
                           SoupMessage       *message,
                           gpointer           data)
{
  const gchar *pattern = NULL;
  EpcPublisher *self = data;
  GList *files = NULL;
  GList *iter;

  GString *contents = g_string_new (NULL);

  if (!epc_publisher_track_client (self, context))
    return;

  if (g_str_has_prefix (context->path, "/list/") && '\0' != context->path[6])
    pattern = context->path + 6;

  files = epc_publisher_list (self, pattern);
  g_string_append (contents, "<list>");

  for (iter = files; iter; iter = iter->next)
    {
      gchar *markup = g_markup_escape_text (iter->data, -1);

      g_string_append (contents, "<item><name>");
      g_string_append (contents, markup);
      g_string_append (contents, "</name></item>");

      g_free (iter->data);
      g_free (markup);
    }

  g_string_append (contents, "</list>");

  soup_message_set_response (message, "text/xml",
                             SOUP_BUFFER_USER_OWNED,
                             contents->str, contents->len);
  soup_message_set_status (message, SOUP_STATUS_OK);

  g_signal_connect_swapped (message, "finished",
                            G_CALLBACK (g_free),
                            contents->str);

  g_string_free (contents, FALSE);
  g_list_free (files);

  epc_publisher_untrack_client (self, context);
}

static void
epc_publisher_handle_root (SoupServerContext *context,
                           SoupMessage       *message,
                           gpointer           data)
{
  EpcPublisher *self = data;

  if (g_str_equal (context->path, "/") &&
      epc_publisher_track_client (self, context))
    {
      GString *contents = g_string_new (NULL);
      gchar *markup;

      GList *files;
      GList *iter;

      files = epc_publisher_list (self, NULL);
      files = g_list_sort (files, (GCompareFunc) g_utf8_collate);

      markup = g_markup_escape_text (self->priv->service_name, -1);

      g_string_append (contents, "<html><head><title>");
      g_string_append (contents, markup);
      g_string_append (contents, "</title></head><body><h1>");
      g_string_append (contents, markup);
      g_string_append (contents, "</h1><h2>");
      g_string_append (contents, _("Table of Contents"));
      g_string_append (contents, "</h2>");

      g_free (markup);

      if (files)
        {
          g_string_append (contents, "<ul id=\"toc\">");

          for (iter = files; iter; iter = iter->next)
            {
              markup = g_markup_escape_text (self->priv->contents_path, -1);

              g_string_append (contents, "<li><a href=\"");
              g_string_append (contents, markup);
              g_string_append (contents, "/");

              g_free (markup);
              markup = g_markup_escape_text (iter->data, -1);

              g_string_append (contents, markup);
              g_string_append (contents, "\">");
              g_string_append (contents, markup);
              g_string_append (contents, "</a></li>");

              g_free (markup);
              g_free (iter->data);
            }

          g_string_append (contents, "</ul>");
        }
      else
        {
          g_string_append (contents, "<p id=\"toc\">");
          g_string_append (contents, _("Sorry, no resources published yet."));
          g_string_append (contents, "</ul>");
        }

      g_string_append (contents, "</ul></body></html>");

      soup_message_set_response (message, "text/html",
                                 SOUP_BUFFER_USER_OWNED,
                                 contents->str, contents->len);
      soup_message_set_status (message, SOUP_STATUS_OK);

      g_signal_connect_swapped (message, "finished",
                                G_CALLBACK (g_free),
                                contents->str);

      g_string_free (contents, FALSE);
      g_list_free (files);

      epc_publisher_untrack_client (self, context);
    }
  else
    soup_message_set_status (message, SOUP_STATUS_NOT_FOUND);
}

static gboolean
epc_publisher_server_auth_cb (SoupServerAuthContext *auth_ctx G_GNUC_UNUSED,
                              SoupServerAuth        *auth,
                              SoupMessage           *message,
                              gpointer               data)
{
  EpcResource *resource = NULL;
  gboolean authorized = TRUE;
  const char *user = NULL;
  EpcAuthContext context;
  const SoupUri *uri;

  uri = soup_message_get_uri (message);

  g_static_rec_mutex_lock (&epc_publisher_lock);

  context.auth = auth;
  context.publisher = EPC_PUBLISHER (data);
  context.key = epc_publisher_get_key (uri->path);

  if (NULL != auth)
    user = soup_server_auth_get_user (auth);
  if (NULL != context.key)
    resource = g_hash_table_lookup (context.publisher->priv->resources, context.key);
  if (NULL == resource)
    resource = context.publisher->priv->default_resource;

  if (resource && resource->auth_handler)
    authorized = resource->auth_handler (&context, user, resource->auth_user_data);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: path=%s, resource=%p, auth_handler=%p, authorized=%d", G_STRLOC,
             uri->path, resource, resource ? resource->auth_handler : NULL, authorized);

  g_static_rec_mutex_unlock (&epc_publisher_lock);

  return authorized;
}

static void
epc_publisher_init (EpcPublisher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_PUBLISHER, EpcPublisherPrivate);

  self->priv->protocol = EPC_PROTOCOL_HTTPS;
  self->priv->server_auth.types = SOUP_AUTH_TYPE_DIGEST;
  self->priv->server_auth.callback = epc_publisher_server_auth_cb;
  self->priv->server_auth.user_data = self;

  self->priv->resources = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, epc_resource_free);
  self->priv->clients = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                               g_object_unref, NULL);
}

static const gchar*
epc_publisher_get_host (EpcPublisher     *self,
                        struct sockaddr **sockaddr,
                        gint             *addrlen)
{
  SoupSocket *listener = soup_server_get_listener (self->priv->server);
  SoupAddress *address = soup_socket_get_local_address (listener);

  if (sockaddr && addrlen)
    *sockaddr = soup_address_get_sockaddr (address, addrlen);

  return soup_address_get_name (address);
}

static gint
epc_publisher_get_port (EpcPublisher *self)
{
  return soup_server_get_port (self->priv->server);
}

static const gchar*
epc_publisher_get_bookmark_type (EpcPublisher *self)
{
  switch (self->priv->protocol)
    {
      case EPC_PROTOCOL_HTTP:
        return "_http._tcp";

      case EPC_PROTOCOL_HTTPS:
        return "_https._tcp";

      case EPC_PROTOCOL_UNKNOWN:
        break;
    }

  return NULL;
}

static void
epc_publisher_find_bookmarks_cb (gpointer key,
                                 gpointer value,
                                 gpointer data)
{
  EpcResource *resource = value;
  GSList **bookmarks = data;

  if (resource->dispatcher)
    {
      *bookmarks = g_slist_prepend (*bookmarks, resource);
      *bookmarks = g_slist_prepend (*bookmarks, key);
    }
}

static EpcResource*
epc_publisher_find_resource (EpcPublisher   *self,
                             const gchar    *key)
{
  if (NULL != key)
    return g_hash_table_lookup (self->priv->resources, key);

  if (NULL == self->priv->default_resource)
    self->priv->default_resource = epc_resource_new (NULL, NULL, NULL);

  return self->priv->default_resource;
}

static void
epc_publisher_announce (EpcPublisher *self)
{
  EpcResource *default_bookmark = NULL;
  GSList *bookmarks = NULL;
  GSList *iter;

  const gchar *bookmark_type;
  const gchar *service_type;
  gchar *service_sub_type;

  const gchar *host;
  struct sockaddr *addr;
  gchar *path_record;
  gint addrlen;
  gint port;

  g_return_if_fail (SOUP_IS_SERVER (self->priv->server));

  /* compute service types */

  service_sub_type = epc_service_type_new (self->priv->protocol,
                                           self->priv->application);
  service_type = epc_protocol_get_service_type (self->priv->protocol);
  bookmark_type = epc_publisher_get_bookmark_type (self);

  /* compute service address */

  host = epc_publisher_get_host (self, &addr, &addrlen);
  port = epc_publisher_get_port (self);

  /* find all bookmark resources */

  g_hash_table_foreach (self->priv->resources,
                        epc_publisher_find_bookmarks_cb,
                        &bookmarks);

  if (self->priv->default_bookmark)
    default_bookmark = epc_publisher_find_resource (self, self->priv->default_bookmark);

  if (default_bookmark)
    {
      bookmarks = g_slist_prepend (bookmarks, default_bookmark);
      bookmarks = g_slist_prepend (bookmarks, self->priv->default_bookmark);
    }

  /* announce the easy-publish service */

  epc_dispatcher_reset (self->priv->dispatcher);

  path_record = g_strconcat ("path=", self->priv->contents_path, NULL);
  epc_dispatcher_add_service (self->priv->dispatcher, addr->sa_family,
                              service_type, self->priv->service_domain,
                              host, port, path_record, NULL);
  g_free (path_record);

  epc_dispatcher_add_service_subtype (self->priv->dispatcher,
                                      service_type, service_sub_type);

  /* announce dynamic bookmarks */

  for (iter = bookmarks; iter; iter = iter->next->next)
    {
      EpcDispatcher *dispatcher = self->priv->dispatcher;
      EpcResource *resource = iter->next->data;
      const gchar *key = iter->data;
      gchar *path;

      if (resource->dispatcher)
        {
          dispatcher = resource->dispatcher;
          epc_dispatcher_reset (dispatcher);
        }

      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: Creating dynamic %s bookmark for %s: %s", G_STRLOC,
                 bookmark_type, key, epc_dispatcher_get_name (dispatcher));

      path = epc_publisher_get_path (self, key);
      path_record = g_strconcat ("path=", path, NULL);

      epc_dispatcher_add_service (dispatcher, addr->sa_family, bookmark_type,
                                  self->priv->service_domain, host, port,
                                  path_record, NULL);

      g_free (path_record);
      g_free (path);
    }

  /* release resources */

  g_free (service_sub_type);
  g_slist_free (bookmarks);

}

static gboolean
epc_publisher_is_server_created (EpcPublisher *self)
{
  return (NULL != self->priv->server);
}

static G_CONST_RETURN gchar*
epc_publisher_compute_name (EpcPublisher *self)
{
  const gchar *name = self->priv->service_name;

  if (!name)
    name = g_get_application_name ();
  if (!name)
    name = g_get_prgname ();

  if (!name)
    {
      gint hash = g_random_int ();

      name = G_OBJECT_TYPE_NAME (self);
      self->priv->service_name = g_strdup_printf ("%s-%08x", name, hash);
      name = self->priv->service_name;

      g_warning ("%s: No service name set - using generated name (`%s'). "
                 "Consider passing a service name to the publisher's "
                 "constructor or call g_set_application_name().",
                 G_STRFUNC, name);
    }

  if (!self->priv->service_name)
    self->priv->service_name = g_strdup (name);

  return name;
}

static void
epc_publisher_remove_handlers (EpcPublisher *self)
{
  memset (&self->priv->server_auth.digest_info, 0,
          sizeof self->priv->server_auth.digest_info);

  soup_server_remove_handler (self->priv->server, self->priv->contents_path);
  soup_server_remove_handler (self->priv->server, "/list");
  soup_server_remove_handler (self->priv->server, "/");
}

static void
epc_publisher_install_handlers (EpcPublisher *self)
{
  memset (&self->priv->server_auth.digest_info, 0,
          sizeof self->priv->server_auth.digest_info);

  switch (self->priv->server_auth.types)
    {
      case SOUP_AUTH_TYPE_BASIC:
        self->priv->server_auth.basic_info.realm = self->priv->service_name;
        break;

      case SOUP_AUTH_TYPE_DIGEST:
        self->priv->server_auth.digest_info.realm = self->priv->service_name;
        self->priv->server_auth.digest_info.allow_algorithms = SOUP_ALGORITHM_MD5;
        self->priv->server_auth.digest_info.force_integrity = FALSE; /* not implemented */
        break;
    }

  soup_server_add_handler (self->priv->server,
                           self->priv->contents_path,
                           &self->priv->server_auth,
                           epc_publisher_handle_contents,
                           NULL, self);
  soup_server_add_handler (self->priv->server, "/list",
                           &self->priv->server_auth,
                           epc_publisher_handle_list,
                           NULL, self);
  soup_server_add_handler (self->priv->server, "/",
                           &self->priv->server_auth,
                           epc_publisher_handle_root,
                           NULL, self);
}

static void
epc_publisher_client_disconnected_cb (EpcPublisher *self,
                                      SoupSocket   *socket)
{
  if (EPC_DEBUG_LEVEL (1))
    epc_publisher_trace_client (G_STRFUNC, "disconnected", socket);

  g_hash_table_remove (self->priv->clients, socket);
}

static void
epc_publisher_new_connection_cb (EpcPublisher *self,
                                 SoupSocket   *socket)
{
  if (EPC_DEBUG_LEVEL (1))
    epc_publisher_trace_client (G_STRFUNC, "new client", socket);

  g_object_ref (socket);
  g_hash_table_replace (self->priv->clients, socket, GINT_TO_POINTER (1));

  g_signal_connect_swapped (socket, "disconnected",
                            G_CALLBACK (epc_publisher_client_disconnected_cb),
                            self);
}

static gboolean
epc_publisher_create_server (EpcPublisher  *self,
                             GError       **error)
{
  gchar *base_uri;

  g_return_val_if_fail (!epc_publisher_is_server_created (self), FALSE);
  g_return_val_if_fail (NULL == self->priv->dispatcher, FALSE);

  self->priv->dispatcher = epc_dispatcher_new (epc_publisher_compute_name (self));

  if (self->priv->service_cookie)
    epc_dispatcher_set_cookie (self->priv->dispatcher, self->priv->service_cookie);
  epc_dispatcher_set_collision_handling (self->priv->dispatcher, self->priv->collisions);

  if (!epc_dispatcher_run (self->priv->dispatcher, error))
    return FALSE;

  if (EPC_PROTOCOL_UNKNOWN == self->priv->protocol)
    self->priv->protocol = EPC_PROTOCOL_HTTPS;

  if (EPC_PROTOCOL_HTTPS == self->priv->protocol && (
      NULL == self->priv->certificate_file ||
      NULL == self->priv->private_key_file))
    {
      GError *tls_error = NULL;
      const gchar *host;

      g_free (self->priv->certificate_file);
      g_free (self->priv->private_key_file);

      host = epc_shell_get_host_name (error);

      if (NULL != host &&
          !epc_tls_get_server_credentials (host,
                                           &self->priv->certificate_file,
                                           &self->priv->private_key_file,
                                           &tls_error))
        {
          self->priv->protocol = EPC_PROTOCOL_HTTP;
          g_warning ("%s: Cannot retrieve server credentials, using insecure transport protocol: %s",
                     G_STRFUNC, tls_error ? tls_error->message : "No error details available.");
          g_clear_error (&tls_error);

        }
    }

  self->priv->server =
    soup_server_new (SOUP_SERVER_SSL_CERT_FILE, self->priv->certificate_file,
                     SOUP_SERVER_SSL_KEY_FILE, self->priv->private_key_file,
                     SOUP_SERVER_PORT, SOUP_ADDRESS_ANY_PORT,
                     NULL);

  g_signal_connect_swapped (soup_server_get_listener (self->priv->server), "new-connection",
                            G_CALLBACK (epc_publisher_new_connection_cb), self);

  epc_publisher_install_handlers (self);
  epc_publisher_announce (self);

  base_uri = epc_publisher_get_uri (self, NULL, NULL);
  g_print ("%s: listening on %s\n", G_STRFUNC, base_uri);
  g_free (base_uri);

  return TRUE;
}

static void
epc_publisher_real_set_service_name (EpcPublisher *self,
                                     const GValue *value)
{
  if (self->priv->server)
    epc_publisher_remove_handlers (self);

  g_free (self->priv->service_name);
  self->priv->service_name = g_value_dup_string (value);

  if (self->priv->server)
    epc_publisher_install_handlers (self);

  if (self->priv->dispatcher)
    epc_dispatcher_set_name (self->priv->dispatcher,
                             epc_publisher_compute_name (self));

}

static void
epc_publisher_real_set_auth_flags (EpcPublisher *self,
                                   const GValue *value)
{
  EpcAuthFlags flags = g_value_get_flags (value);

  if (0 != (flags & EPC_AUTH_PASSWORD_TEXT_NEEDED) &&
      EPC_PROTOCOL_HTTPS != self->priv->protocol)
    {
      g_warning ("%s: Basic authentication not allowed for %s",
                 G_STRFUNC, epc_protocol_to_string (self->priv->protocol));
      flags &= ~EPC_AUTH_PASSWORD_TEXT_NEEDED;
    }

  if (self->priv->server)
    epc_publisher_remove_handlers (self);

  self->priv->server_auth.types =
    flags & EPC_AUTH_PASSWORD_TEXT_NEEDED ?
    SOUP_AUTH_TYPE_BASIC : SOUP_AUTH_TYPE_DIGEST;

  if (self->priv->server)
    epc_publisher_install_handlers (self);
}

void
epc_publisher_set_service_cookie (EpcPublisher *self,
                                  const gchar  *cookie)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_object_set (self, "service-cookie", cookie, NULL);
}

void
epc_publisher_set_collision_handling (EpcPublisher         *self,
                                      EpcCollisionHandling  method)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_object_set (self, "collision-handling", method, NULL);
}

static void
epc_publisher_real_set_contents_path (EpcPublisher *self,
                                      const GValue *value)
{
  const gchar *path = g_value_get_string (value);

  g_return_if_fail (NULL != path);
  g_return_if_fail ('/' == path[0]);
  g_return_if_fail ('\0' != path[1]);

  if (NULL == self->priv->contents_path ||
      strcmp (self->priv->contents_path, path))
    {
      if (self->priv->server)
        epc_publisher_remove_handlers (self);

      g_free (self->priv->contents_path);
      self->priv->contents_path = g_value_dup_string (value);

      if (self->priv->server)
        epc_publisher_install_handlers (self);
    }
}

static void
epc_publisher_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EpcPublisher *self = EPC_PUBLISHER (object);

  switch (prop_id)
    {
      case PROP_PROTOCOL:
        g_return_if_fail (!epc_publisher_is_server_created (self));
        g_return_if_fail (EPC_PROTOCOL_UNKNOWN != g_value_get_enum (value));
        self->priv->protocol = g_value_get_enum (value);
        break;

      case PROP_APPLICATION:
        g_return_if_fail (!epc_publisher_is_server_created (self));

        g_free (self->priv->application);
        self->priv->application = g_value_dup_string (value);
        break;

      case PROP_SERVICE_NAME:
        epc_publisher_real_set_service_name (self, value);
        break;

      case PROP_SERVICE_DOMAIN:
        g_return_if_fail (!epc_publisher_is_server_created (self));

        g_free (self->priv->service_domain);
        self->priv->service_domain = g_value_dup_string (value);
        break;

      case PROP_SERVICE_COOKIE:
        g_free (self->priv->service_cookie);
        self->priv->service_cookie = g_value_dup_string (value);

        if (self->priv->dispatcher)
          epc_dispatcher_set_cookie (self->priv->dispatcher,
                                     self->priv->service_cookie);
        break;

      case PROP_COLLISION_HANDLING:
        self->priv->collisions = g_value_get_enum (value);

        if (self->priv->dispatcher)
          epc_dispatcher_set_collision_handling (self->priv->dispatcher,
                                                 self->priv->collisions);

        break;

      case PROP_CONTENTS_PATH:
        epc_publisher_real_set_contents_path (self, value);
        break;

      case PROP_AUTH_FLAGS:
        epc_publisher_real_set_auth_flags (self, value);
        break;

      case PROP_CERTIFICATE_FILE:
        g_return_if_fail (!epc_publisher_is_server_created (self));

        g_free (self->priv->certificate_file);
        self->priv->certificate_file = g_value_dup_string (value);
        break;

      case PROP_PRIVATE_KEY_FILE:
        g_return_if_fail (!epc_publisher_is_server_created (self));

        g_free (self->priv->private_key_file);
        self->priv->private_key_file = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_publisher_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EpcPublisher *self = EPC_PUBLISHER (object);

  switch (prop_id)
    {
      case PROP_PROTOCOL:
        g_value_set_enum (value, self->priv->protocol);
        break;

      case PROP_APPLICATION:
        g_value_set_string (value, self->priv->application);
        break;

      case PROP_SERVICE_NAME:
        g_value_set_string (value, self->priv->service_name);
        break;

      case PROP_SERVICE_DOMAIN:
        g_value_set_string (value, self->priv->service_domain);
        break;

      case PROP_SERVICE_COOKIE:
        g_value_set_string (value, self->priv->service_cookie);
        break;

      case PROP_COLLISION_HANDLING:
        g_value_set_enum (value, self->priv->collisions);
        break;

      case PROP_CONTENTS_PATH:
        g_value_set_string (value, self->priv->contents_path);
        break;

      case PROP_AUTH_FLAGS:
        g_value_set_flags (value, self->priv->auth_flags);
        break;

      case PROP_CERTIFICATE_FILE:
        g_value_set_string (value, self->priv->certificate_file);
        break;

      case PROP_PRIVATE_KEY_FILE:
        g_value_set_string (value, self->priv->private_key_file);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_publisher_dispose (GObject *object)
{
  EpcPublisher *self = EPC_PUBLISHER (object);

  epc_publisher_quit (self);

  if (self->priv->clients)
    {
      g_hash_table_unref (self->priv->clients);
      self->priv->clients = NULL;
    }

  if (self->priv->resources)
    {
      g_hash_table_unref (self->priv->resources);
      self->priv->resources = NULL;
    }

  if (self->priv->default_resource)
    {
      epc_resource_free (self->priv->default_resource);
      self->priv->default_resource = NULL;
    }

  g_free (self->priv->certificate_file);
  self->priv->certificate_file = NULL;

  g_free (self->priv->private_key_file);
  self->priv->private_key_file = NULL;

  g_free (self->priv->service_name);
  self->priv->service_name = NULL;

  g_free (self->priv->service_domain);
  self->priv->service_domain = NULL;

  g_free (self->priv->application);
  self->priv->application = NULL;

  g_free (self->priv->contents_path);
  self->priv->contents_path = NULL;

  g_free (self->priv->default_bookmark);
  self->priv->default_bookmark = NULL;

  G_OBJECT_CLASS (epc_publisher_parent_class)->dispose (object);
}

static void
epc_publisher_class_init (EpcPublisherClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_publisher_set_property;
  oclass->get_property = epc_publisher_get_property;
  oclass->dispose = epc_publisher_dispose;

  g_object_class_install_property (oclass, PROP_PROTOCOL,
                                   g_param_spec_enum ("protocol", "Protocol",
                                                      "The transport protocol the publisher uses",
                                                      EPC_TYPE_PROTOCOL, EPC_PROTOCOL_HTTPS,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_APPLICATION,
                                   g_param_spec_string ("application", "Application",
                                                        "Program name for deriving the service type",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_SERVICE_NAME,
                                   g_param_spec_string ("service-name", "Service Name",
                                                        "User friendly name for the service",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_SERVICE_DOMAIN,
                                   g_param_spec_string ("service-domain", "Service Domain",
                                                        "Internet domain for publishing the service",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EpcPublisher:service-cookie:
   *
   * Unique identifier of the service. This cookie is used for implementing
   * #EPC_COLLISION_HANDLING_UNIQUE_SERVICE, and usually is a UUID or the
   * MD5/SHA1/... checksum of a central document.
   *
   * Since: 0.3.1
   */
  g_object_class_install_property (oclass, PROP_SERVICE_COOKIE,
                                   g_param_spec_string ("service-cookie", "Service Cookie",
                                                        "Unique identifier of the service",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EpcPublisher:collision-handling:
   *
   * The collision handling method to use.
   *
   * Since: 0.3.1
   */
  g_object_class_install_property (oclass, PROP_COLLISION_HANDLING,
                                   g_param_spec_enum ("collision-handling", "Collision Handling",
                                                      "The collision handling method to use",
                                                      EPC_TYPE_COLLISION_HANDLING,
                                                      EPC_COLLISION_HANDLING_ALTERNATIVE_NAME,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_CONTENTS_PATH,
                                   g_param_spec_string ("contents-path", "Contents Path",
                                                        "The built-in server path for publishing resources",
                                                        "/contents",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_AUTH_FLAGS,
                                   g_param_spec_flags ("auth-flags", "Authentication Flags",
                                                      "The authentication settings to use",
                                                      EPC_TYPE_AUTH_FLAGS, EPC_AUTH_DEFAULT,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_CERTIFICATE_FILE,
                                   g_param_spec_string ("certificate-file", "Certificate File",
                                                        "File name for the PEM encoded server certificate",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_PRIVATE_KEY_FILE,
                                   g_param_spec_string ("private-key-file", "Private Key File",
                                                        "File name for the PEM encoded private server key",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcPublisherPrivate));
  g_static_rec_mutex_init (&epc_publisher_lock);
}

/**
 * epc_publisher_new:
 * @name: the human friendly service name, or %NULL
 * @application: application name used for DNS-SD service type, or %NULL
 * @domain: the DNS domain for announcing the service, or %NULL
 *
 * Creates a new #EpcPublisher object. The publisher announces its service
 * per DNS-SD to the DNS domain specified by @domain, using @name as service
 * name. The service type is derived from @application. When %NULL is passed
 * for @application the value returned by #g_get_prgname is used. See
 * #epc_service_type_new for details.
 *
 * Returns: The newly created #EpcPublisher object.
 */
EpcPublisher*
epc_publisher_new (const gchar *name,
                   const gchar *application,
                   const gchar *domain)
{
  return g_object_new (EPC_TYPE_PUBLISHER,
                       "service-name", name,
                       "service-domain", domain,
                       "application", application,
                       NULL);
}

/**
 * epc_publisher_add:
 * @publisher: a #EpcPublisher
 * @key: the key for addressing the value
 * @data: the value to publish
 * @length: the length of @data in bytes, or -1 if @data is a null-terminated string.
 *
 * Publishes a new value on the #EpcPublisher using the unique @key for
 * addressing. When -1 is passed for @length, @data is expected to be a
 * null-terminated string and its length in bytes is determined automatically
 * using <function>strlen</function>.
 *
 * <note><para>
 *  Values published by the #EpcPublisher can be arbitrary data, possibly
 *  including null characters in the middle. The kind of data associated
 *  with a @key is chosen by the application providing values and should 
 *  be specified separately.
 *
 *  However, when publishing plain text it is strongly recommended
 *  to use UTF-8 encoding to avoid internationalization issues.
 * </para></note>
 */
void
epc_publisher_add (EpcPublisher  *self,
                   const gchar   *key,
                   gconstpointer  data,
                   gssize         length)
{
  const gchar *type = NULL;

  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != data);
  g_return_if_fail (NULL != key);

  if (-1 == length)
    {
      length = strlen (data);
      type = "text/plain";
    }

  epc_publisher_add_handler (self, key,
                             epc_publisher_handle_static,
                             epc_contents_new_dup (type, data, length),
                             (GDestroyNotify) epc_contents_unref);
}

/**
 * epc_publisher_add_file:
 * @publisher: a #EpcPublisher
 * @key: the key for addressing the file
 * @filename: the name of the file to publish
 *
 * Publishes a local file on the #EpcPublisher using the unique
 * @key for addressing. The publisher delivers the current contents
 * of the file at the time of access.
 */
void
epc_publisher_add_file (EpcPublisher  *self,
                        const gchar   *key,
                        const gchar   *filename)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != filename);
  g_return_if_fail (NULL != key);

  epc_publisher_add_handler (self, key,
                             epc_publisher_handle_file,
                             g_strdup (filename), g_free);
}

/**
 * epc_publisher_add_handler:
 * @publisher: a #EpcPublisher
 * @key: the key for addressing the contents
 * @handler: the #EpcContentsHandler for handling this contents
 * @user_data: data to pass on @handler calls
 * @destroy_data: a function for releasing @user_data
 *
 * Publishes contents on the #EpcPublisher which are generated by a custom
 * #EpcContentsHandler callback. This is the most flexible method for publishing
 * information.
 *
 * The @handler is called on every request matching @key.
 * When called, @publisher, @key and @user_data are passed to the @handler.
 * When replacing or deleting the resource referenced by @key,
 * or when the the Publisher is destroyed, the function
 * described by @destroy_data is called with @user_data as argument.
 */
void
epc_publisher_add_handler (EpcPublisher      *self,
                           const gchar       *key,
                           EpcContentsHandler handler,
                           gpointer           user_data,
                           GDestroyNotify     destroy_data)
{
  EpcResource *resource;

  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != handler);
  g_return_if_fail (NULL != key);

  g_static_rec_mutex_lock (&epc_publisher_lock);

  resource = epc_resource_new (handler, user_data, destroy_data);
  g_hash_table_insert (self->priv->resources, g_strdup (key), resource);

  g_static_rec_mutex_unlock (&epc_publisher_lock);
}

/**
 * epc_publisher_get_path:
 * @publisher: a #EpcPublisher
 * @key: the resource key to inspect, or %NULL.
 *
 * Queries the path component of the URI used to publish the resource
 * associated with @key. This is useful when referencing keys in published
 * resources. Passing %NULL as @key retrieve the path of the root context.
 *
 * Returns: The resource path for @key.
 */
gchar*
epc_publisher_get_path (EpcPublisher *self,
                        const gchar  *key)
{
  gchar *encoded_key = NULL;
  gchar *path = NULL;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);

  if (key)
    {
      encoded_key = soup_uri_encode (key, NULL);
      path = g_strconcat (self->priv->contents_path, "/", encoded_key, NULL);
      g_free (encoded_key);
    }
  else
    path = g_strdup ("/");

  return path;
}

/**
 * epc_publisher_get_uri:
 * @publisher: a #EpcPublisher
 * @key: the resource key to inspect, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Queries the URI used to publish the resource associated with @key.
 * This is useful when referencing keys in published resources. When
 * passing %NULL the publisher's base URI is returned.
 *
 * The function fails if the publisher's host name cannot be retrieved.
 * In that case %NULL is returned and @error is set. The error domain is
 * #EPC_AVAHI_ERROR. Possible error codes are those of the
 * <citetitle>Avahi</citetitle> library.
 *
 * Returns: The fully qualified URI for @key, or %NULL on error.
 */
gchar*
epc_publisher_get_uri (EpcPublisher  *self,
                       const gchar   *key,
                       GError       **error)
{
  gchar *path = NULL;
  gchar *url = NULL;

  const gchar *host;
  gint port;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);

  host = epc_publisher_get_host (self, NULL, NULL);
  port = epc_publisher_get_port (self);

  if (!host)
    host = epc_shell_get_host_name (error);
  if (!host)
    return NULL;

  path = epc_publisher_get_path (self, key);
  url = epc_protocol_build_uri (self->priv->protocol, host, port, path);
  g_free (path);

  return url;
}

/**
 * epc_publisher_remove:
 * @publisher: a #EpcPublisher
 * @key: the key for addressing the contents
 *
 * Removes a key and its associated contents from a #EpcPublisher.
 *
 * Returns: %TRUE if the key was found and removed from the #EpcPublisher.
 */
gboolean
epc_publisher_remove (EpcPublisher *self,
                      const gchar  *key)
{
  gboolean success;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), FALSE);
  g_return_val_if_fail (NULL != key, FALSE);

  g_static_rec_mutex_lock (&epc_publisher_lock);

  if (self->priv->default_bookmark &&
      g_str_equal (key, self->priv->default_bookmark))
    {
      g_free (self->priv->default_bookmark);
      self->priv->default_bookmark = NULL;

      if (self->priv->server)
        epc_publisher_announce (self);
    }

  success = g_hash_table_remove (self->priv->resources, key);
  g_static_rec_mutex_unlock (&epc_publisher_lock);

  return success;
}

/**
 * epc_publisher_lookup:
 * @publisher: a #EcpPublisher
 * @key: the key for addressing contents
 *
 * Looks up the user_data passed to #epc_publisher_add_handler for @key.
 * Returns %NULL if the specified @key doesn't exist or wasn't published
 * with #epc_publisher_add_handler.
 *
 * This function allows to use the publisher as local key/value store,
 * which is useful for instance to prevent accidental key collisions.
 *
 * See also: epc_publisher_has_key.
 *
 * Returns: The user_data associated with @key, or %NULL.
 */
gpointer
epc_publisher_lookup (EpcPublisher *self,
                      const gchar  *key)
{
  EpcResource *resource;
  gpointer data = NULL;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  g_return_val_if_fail (NULL != key, NULL);

  g_static_rec_mutex_lock (&epc_publisher_lock);

  resource = g_hash_table_lookup (self->priv->resources, key);

  if (resource)
    data = resource->user_data;

  g_static_rec_mutex_unlock (&epc_publisher_lock);

  return data;
}

/**
 * epc_publisher_has_key:
 * @publisher: a #EcpPublisher
 * @key: the key for addressing contents
 *
 * Checks if information is published for @key.
 *
 * This function allows to use the publisher as local key/value store,
 * which is useful for instance to prevent accidental key collisions.
 *
 * See also: epc_publisher_lookup.
 *
 * Returns: %TRUE when the publisher has information for @key,
 * and %FALSE otherwise.
 */
gboolean
epc_publisher_has_key (EpcPublisher *self,
                       const gchar  *key)
{
  EpcResource *resource;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), FALSE);
  g_return_val_if_fail (NULL != key, FALSE);

  g_static_rec_mutex_lock (&epc_publisher_lock);
  resource = g_hash_table_lookup (self->priv->resources, key);
  g_static_rec_mutex_unlock (&epc_publisher_lock);

  return (NULL != resource);
}

static void
epc_publisher_list_cb (gpointer key,
                       gpointer value G_GNUC_UNUSED,
                       gpointer data)
{
  EpcListContext *context = data;

  if (NULL == context->pattern || g_pattern_match_string (context->pattern, key))
    context->matches = g_list_prepend (context->matches, g_strdup (key));
}

/**
 * epc_publisher_list:
 * @publisher: a #EpcPublisher
 * @pattern: a glob-style pattern, or %NULL
 *
 * Matches published keys against patterns containing '*' (wildcard) and '?'
 * (joker). Passing %NULL as @pattern is equivalent to passing "*" and returns
 * all published keys. This function is useful for generating dynamic resource
 * listings in other formats than libepc's specific format. See #GPatternSpec
 * for information about glob-style patterns.
 *
 * If the call was successful, a list of keys matching @pattern is returned.
 * If the call was not successful, it returns %NULL.
 *
 * The returned list should be freed when no longer needed:
 *
 * <programlisting>
 *  g_list_foreach (keys, (GFunc) g_free, NULL);
 *  g_list_free (keys);
 * </programlisting>
 *
 * See also #epc_consumer_list for builtin listing capabilities.
 *
 * Returns: A newly allocated list of keys, or %NULL when an error occurred.
 */
GList*
epc_publisher_list (EpcPublisher *self,
                    const gchar  *pattern)
{
  EpcListContext context;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);

  context.matches = NULL;
  context.pattern = NULL;

  if (pattern && *pattern)
    context.pattern = g_pattern_spec_new (pattern);

  g_static_rec_mutex_lock (&epc_publisher_lock);

  g_hash_table_foreach (self->priv->resources,
                        epc_publisher_list_cb,
                        &context);

  g_static_rec_mutex_unlock (&epc_publisher_lock);

  if (context.pattern)
    g_pattern_spec_free (context.pattern);

  return context.matches;
}

/**
 * epc_publisher_set_auth_handler:
 * @publisher: a #EpcPublisher
 * @key: the key of the resource to protect, or %NULL
 * @handler: the #EpcAuthHandler to connect
 * @user_data: data to pass on @handler calls
 * @destroy_data: a function for releasing @user_data
 *
 * Installs an authentication handler for the specified @key.
 * Passing %NULL as @key installs a fallback handler for all resources.
 *
 * The @handler is called on every request matching @key. On this call
 * a temporary #EpcAuthContext and @user_data are passed to the @handler.
 * The #EpcAuthContext references the @publisher and @key passed here.
 * When replacing or deleting the resource referenced by @key, or when
 * the publisher is destroyed, the function
 * described by @destroy_data is called with @user_data as argument.
 *
 * <note><para>
 *  This should be called after adding the resource identified by @key,
 *  not before. For instance, after calling #epc_publisher_add.
 * </para></note>
 *
 * See also #epc_publisher_set_auth_flags.
 */
void
epc_publisher_set_auth_handler (EpcPublisher   *self,
                                const gchar    *key,
                                EpcAuthHandler  handler,
                                gpointer        user_data,
                                GDestroyNotify  destroy_data)
{
  EpcResource *resource;

  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != handler);

  g_static_rec_mutex_lock (&epc_publisher_lock);

  resource = epc_publisher_find_resource (self, key);

  if (resource)
    epc_resource_set_auth_handler (resource, handler, user_data, destroy_data);
  else
    g_warning ("%s: No resource handler found for key `%s'", G_STRFUNC, key);

  g_static_rec_mutex_unlock (&epc_publisher_lock);
}

/**
 * epc_publisher_add_bookmark:
 * @publisher: a #EpcResource
 * @key: the key of the resource to publish, or %NULL
 * @label: the bookmark's label, or %NULL
 *
 * Installs a dynamic HTTP (respectively HTTPS) bookmark for @key.
 * This allows consumption of #EpcPublisher resources by foreign
 * applications that support ZeroConf bookmarks, but not libepc.
 * This is useful for instance for publishing media playlists.
 *
 * Passing %NULL as @key installs a bookmark for the root context of the
 * builtin web server. When passing %NULL as @label the publisher's name
 * is used as bookmark label.
 *
 * <note><para>
 *  Dynamic bookmarks must be unique within the service domain.
 *  Therefore the @label will get modified on name collisions.
 * </para></note>
 *
 * <note><para>
 *  This should be called after adding the resource identified by @key,
 *  not before. For instance, after calling #epc_publisher_add.
 * </para></note>
 */
void
epc_publisher_add_bookmark (EpcPublisher *self,
                            const gchar  *key,
                            const gchar  *description)
{
  EpcResource *resource;

  g_return_if_fail (EPC_IS_PUBLISHER (self));

  g_static_rec_mutex_lock (&epc_publisher_lock);

  resource = epc_publisher_find_resource (self, key);

  if (resource)
    {
      if (description)
        epc_resource_announce (resource, description);
      else
        self->priv->default_bookmark = g_strdup (key);

      if (self->priv->server)
        epc_publisher_announce (self);
    }
  else
    g_warning ("%s: No resource handler found for key `%s'", G_STRFUNC, key);

  g_static_rec_mutex_unlock (&epc_publisher_lock);
}

/**
 * epc_publisher_set_service_name:
 * @publisher: a #EpcPublisher
 * @name: the new name of this #EpcPublisher
 *
 * Changes the human friendly name this #EpcPublisher uses to announce its
 * service. See #EpcPublisher:service-name for details.
 */
void
epc_publisher_set_service_name (EpcPublisher *self,
                                const gchar  *name)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_object_set (self, "service-name", name, NULL);
}

/**
 * epc_publisher_set_credentials:
 * @publisher: a #EpcPublisher
 * @certfile: file name of the server certificate
 * @keyfile: file name of the private key
 *
 * Changes the file names of the PEM encoded TLS credentials the publisher use
 * for its services, when the transport #EpcPublisher:protocol is
 * #EPC_PROTOCOL_HTTPS.
 *
 * See #EpcPublisher:certificate-file and
 * #EpcPublisher:private-key-file for details.
 */
void
epc_publisher_set_credentials (EpcPublisher *self,
                               const gchar  *certfile,
                               const gchar  *keyfile)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));

  g_object_set (self, "certificate-file", certfile,
                      "private-key-file", keyfile,
                      NULL);
}

/**
 * epc_publisher_set_protocol:
 * @publisher: a #EpcPublisher
 * @protocol: the transport protocol
 *
 * Changes the transport protocol the publisher uses.
 * See #EpcPublisher:protocol for details.
 */
void
epc_publisher_set_protocol (EpcPublisher *self,
                            EpcProtocol   protocol)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_object_set (self, "protocol", protocol, NULL);
}

/**
 * epc_publisher_set_contents_path:
 * @publisher: a #EpcPublisher
 * @path: the new contents path
 *
 * Changes the server path used for publishing contents.
 * See #EpcPublisher:contents-path for details.
 */
void
epc_publisher_set_contents_path (EpcPublisher *self,
                                 const gchar  *path)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_object_set (self, "contents-path", path, NULL);
}

/**
 * epc_publisher_set_auth_flags:
 * @publisher: a #EpcPublisher
 * @flags: new authentication settings
 *
 * Changes the authentication settings the publisher uses 
 * when #epc_publisher_set_auth_handler is used.
 * See #EpcPublisher:auth-flags for details.
 */
void
epc_publisher_set_auth_flags (EpcPublisher *self,
                              EpcAuthFlags  flags)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_object_set (self, "auth-flags", flags, NULL);
}

/**
 * epc_publisher_get_service_name:
 * @publisher: a #EpcPublisher
 *
 * Queries the human friendly name this #EpcPublisher uses
 * to announce its service. See #EpcPublisher:name for details.
 *
 * Returns: The human friendly name of this #EpcPublisher.
 */
G_CONST_RETURN gchar*
epc_publisher_get_service_name (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->service_name;
}

/**
 * epc_publisher_get_service_domain:
 * @publisher: a #EpcPublisher
 *
 * Queries the DNS domain for which this #EpcPublisher announces its service.
 * See #EpcPublisher:domain for details.
 *
 * Returns: The DNS-SD domain of this #EpcPublisher, or %NULL.
 */
G_CONST_RETURN gchar*
epc_publisher_get_service_domain (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->service_domain;
}

/**
 * epc_publisher_get_certificate_file:
 * @publisher: a #EpcPublisher
 *
 * Queries the file name of the PEM encoded server certificate.
 * See #EpcPublisher:certificate-file for details.
 *
 * Returns: The certificate's file name, or %NULL.
 */
G_CONST_RETURN gchar*
epc_publisher_get_certificate_file (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->certificate_file;
}

/**
 * epc_publisher_get_private_key_file:
 * @publisher: a #EpcPublisher
 *
 * Queries the file name of the PEM encoded private server key.
 * See #EpcPublisher:private-key-file for details.
 *
 * Returns: The private key's file name, or %NULL.
 */
G_CONST_RETURN gchar*
epc_publisher_get_private_key_file (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->private_key_file;
}

/**
 * epc_publisher_get_protocol:
 * @publisher: a #EpcPublisher
 *
 * Queries the transport protocol the publisher uses.
 * See #EpcPublisher:protocol for details.
 *
 * Returns: The transport protocol the publisher uses,
 * or #EPC_PROTOCOL_UNKNOWN on error.
 */
EpcProtocol
epc_publisher_get_protocol (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), EPC_PROTOCOL_UNKNOWN);
  return self->priv->protocol;
}

/**
 * epc_publisher_get_contents_path:
 * @publisher: a #EpcPublisher
 *
 * Queries the server path used for publishing contents.
 * See #EpcPublisher:contents-path for details.
 *
 * Returns: The server's contents path.
 */
G_CONST_RETURN gchar*
epc_publisher_get_contents_path (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->contents_path;
}

/**
 * epc_publisher_get_auth_flags:
 * @publisher: a #EpcPublisher
 *
 * Queries the current authentication settings of the publisher.
 * See #EpcPublisher:auth-flags for details.
 *
 * Returns: The authentication settings of the publisher,
 * or #EPC_AUTH_DEFAULT on error.
 */
EpcAuthFlags
epc_publisher_get_auth_flags (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), EPC_AUTH_DEFAULT);
  return self->priv->auth_flags;
}

G_CONST_RETURN gchar*
epc_publisher_get_service_cookie (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->service_cookie;
}

EpcCollisionHandling
epc_publisher_get_collision_handling (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self),
                        EPC_COLLISION_HANDLING_NONE);

  return self->priv->collisions;
}

/**
 * epc_publisher_run:
 * @publisher: a #EpcPublisher
 * @error: return location for a #GError, or %NULL
 *
 * Starts the server component of the #EpcPublisher and blocks until it is
 * shutdown using #epc_publisher_quit. If the server could not be started, the
 * function returns %FALSE and sets @error. The error domain is
 * #EPC_AVAHI_ERROR. Possible error codes are those of the
 * <citetitle>Avahi</citetitle> library.
 *
 * When starting the publisher in HTTPS mode for the first time self-signed
 * keys must be generated. Generating secure keys needs some time,
 * so it is recommended to call #epc_progress_window_install, or
 * #epc_shell_set_progress_hooks to provide visual feedback during that
 * operation. Key generation takes place in a separate background thread and
 * the calling thread waits in a GMainLoop. Therefore the UI can remain
 * responsive when generating keys.
 *
 * To start the server without blocking call #epc_publisher_run_async.
 *
 * Returns: %TRUE when the publisher was successfully started,
 * %FALSE if an error occurred.
 */
gboolean
epc_publisher_run (EpcPublisher  *self,
                   GError       **error)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), FALSE);

  if (!epc_publisher_run_async (self, error))
    return FALSE;

  if (NULL == self->priv->server_loop)
    {
      self->priv->server_loop = g_main_loop_new (NULL, FALSE);

      g_main_loop_run (self->priv->server_loop);

      g_main_loop_unref (self->priv->server_loop);
      self->priv->server_loop = NULL;
    }

  return TRUE;
}

/**
 * epc_publisher_run_async:
 * @publisher: a #EpcPublisher
 * @error: return location for a #GError, or %NULL
 *
 * Starts the server component of the #EpcPublisher without blocking. If the
 * server could not be started then the function returns %FALSE and sets @error. The
 * error domain is #EPC_AVAHI_ERROR. Possible error codes are those of the
 * <citetitle>Avahi</citetitle> library.
 *
 * To stop the server component call #epc_publisher_quit.
 * See #epc_publisher_run for additional information.
 *
 * Returns: %TRUE when the publisher was successfully started,
 * %FALSE if an error occurred.
 */
gboolean
epc_publisher_run_async (EpcPublisher  *self,
                         GError       **error)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), FALSE);

  if (!epc_publisher_is_server_created (self) &&
      !epc_publisher_create_server (self, error))
    return FALSE;

  if (!self->priv->server_started)
    {
      soup_server_run_async (self->priv->server);
      g_object_unref (self->priv->server); /* work arround bug #494128 */
      self->priv->server_started = TRUE;
    }

  return TRUE;
}

static void
epc_publisher_disconnect_idle_cb (gpointer key,
                                  gpointer value,
                                  gpointer data)
{
  SoupSocket *socket = key;
  GSList **clients = data;

  if (1 >= GPOINTER_TO_INT (value))
    {
      if (EPC_DEBUG_LEVEL (1))
        epc_publisher_trace_client (G_STRFUNC, "idle client", socket);

      *clients = g_slist_prepend (*clients, socket);
    }
}

/**
 * epc_publisher_quit:
 * @publisher: a #EpcPublisher
 *
 * Stops the server component of the #EpcPublisher started with
 * #epc_publisher_run or #epc_publisher_run_async. The functions
 * returns %TRUE when the built-in server was running and had to
 * be stopped. If the server wasn't running the function returns
 * %FALSE.
 *
 * Returns: %TRUE when the server had to be stopped, and %FALSE otherwise.
 */
gboolean
epc_publisher_quit (EpcPublisher *self)
{
  GSList *idle_clients = NULL;
  gboolean was_running;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), FALSE);

  was_running = self->priv->server_started;

  if (self->priv->server_loop)
    g_main_loop_quit (self->priv->server_loop);

  g_static_rec_mutex_lock (&epc_publisher_lock);

  if (self->priv->clients)
    g_hash_table_foreach (self->priv->clients,
                          epc_publisher_disconnect_idle_cb,
                          &idle_clients);

  g_slist_foreach (idle_clients, (GFunc) soup_socket_disconnect, NULL);
  g_slist_free (idle_clients);

  g_static_rec_mutex_unlock (&epc_publisher_lock);

  if (self->priv->dispatcher)
    {
      g_object_unref (self->priv->dispatcher);
      self->priv->dispatcher = NULL;
    }

  if (self->priv->server)
    {
      g_object_unref (self->priv->server);
      self->priv->server = NULL;
    }

  self->priv->server_started = FALSE;

  return was_running;
}

static gchar*
epc_utf8_strtitle (const gchar *str,
                   gssize       len)
{
    gunichar     first_chr;
    gchar        first_str[7];
    gint         first_len;

    const gchar *tail_str;
    gsize        tail_len;

    gchar       *lower_str;
    gsize        lower_len;

    gchar       *title_str;

    g_return_val_if_fail (NULL != str, NULL);

    if (-1 == len)
      len = strlen (str);

    first_chr = g_utf8_get_char_validated (str, len);

    if ((gint) first_chr < 0)
      return NULL;

    first_chr = g_unichar_totitle (first_chr);
    first_len = g_unichar_to_utf8 (first_chr, first_str);

    tail_str = g_utf8_next_char (str);
    tail_len = len - (tail_str - str);

    lower_str = g_utf8_strdown (tail_str, tail_len);
    lower_len = strlen (lower_str);

    len = first_len + lower_len;
    title_str = g_new (gchar, len + 1);
    title_str[len] = '\0';

    memcpy (title_str, first_str, first_len);
    memcpy (title_str + first_len, lower_str, lower_len);

    g_free (lower_str);

    return title_str;
}

/**
 * epc_publisher_expand_name:
 * @name: a service name with placeholders
 * @error: return location for a #GError, or %NULL
 *
 * Expands all known placeholders in @name. Supported placeholders are:
 *
 * <itemizedlist>
 *  <listitem>%%a: the program name as returned by #g_get_application_name</listitem>
 *  <listitem>%%h: the machine's host name in title case</listitem>
 *  <listitem>%%u: the user's login name in title case</listitem>
 *  <listitem>%%U: the user's real name</listitem>
 *  <listitem>%%: the percent sign</listitem>
 * </itemizedlist>
 *
 * The function fails when the host name cannot looked up. In that case %NULL
 * is returned and @error is set. The error domain is #EPC_AVAHI_ERROR.
 * Possible error codes are those of the <citetitle>Avahi</citetitle> library.
 *
 * Returns: The @name with all known placeholders expanded, or %NULL on error.
 */
gchar*
epc_publisher_expand_name (const gchar  *name,
                           GError      **error)
{
  gchar *tcase_host = NULL;
  const gchar *host = NULL;
  const gchar *tail = NULL;

  GString *expand = NULL;

  if (NULL == name)
    name = _("%a of %u on %h");

  host = epc_shell_get_host_name (error);

  if (NULL == host)
    return NULL;

  expand = g_string_new (NULL);

  while (NULL != (tail = strchr (name, '%')))
    {
      const gchar *subst = NULL;
      gchar *temp_str1 = NULL;
      gchar *temp_str2 = NULL;
      gsize  temp_len;

      g_string_append_len (expand, name, tail - name);

      switch (tail[1])
        {
          case 'u':
            temp_str1 = g_filename_to_utf8 (g_get_user_name (), -1, NULL, &temp_len, NULL);
            temp_str2 = epc_utf8_strtitle (temp_str1, temp_len);
            subst = temp_str2;
            break;

          case 'U':
            temp_str1 = g_filename_to_utf8 (g_get_real_name (), -1, NULL, NULL, NULL);
            subst = temp_str1;
            break;
            break;

          case 'a':
            subst = g_get_application_name ();
            break;

          case 'h':
            if (!tcase_host)
              tcase_host = epc_utf8_strtitle (host, -1);

            subst = tcase_host;
            break;

          case '%':
            subst = "%";
            break;
        }

      if (subst)
        {
          g_string_append (expand, subst);
          name = tail + 2;
        }
      else
        {
          g_string_append_c (expand, *tail);
          name = tail + 1;
        }

      g_free (temp_str2);
      g_free (temp_str1);
    }

  g_string_append (expand, name);
  g_free (tcase_host);

  return g_string_free (expand, FALSE);
}

/**
 * epc_auth_context_get_publisher:
 * @context: a #EpcAuthContext
 *
 * Queries the #EpcPublisher owning the authentication @context.
 *
 * Returns: The owning #EpcPublisher.
 */
EpcPublisher*
epc_auth_context_get_publisher (const EpcAuthContext *context)
{
  g_return_val_if_fail (NULL != context, NULL);
  return context->publisher;
}

/**
 * epc_auth_context_get_key:
 * @context: a #EpcAuthContext
 *
 * Queries the resource key associated with the authentication @context.
 *
 * Returns: The resource key.
 */
G_CONST_RETURN gchar*
epc_auth_context_get_key (const EpcAuthContext *context)
{
  g_return_val_if_fail (NULL != context, NULL);
  return context->key;
}

/**
 * epc_auth_context_get_password:
 * @context: a #EpcAuthContext
 *
 * Queries the password sent for the authentication @context when Basic
 * authentication was allowed for the @context, and %NULL otherwise.
 *
 * See also: #EPC_AUTH_PASSWORD_TEXT_NEEDED
 *
 * Returns: The password sent, or %NULL.
 */
G_CONST_RETURN gchar*
epc_auth_context_get_password (const EpcAuthContext *context)
{
  g_return_val_if_fail (NULL != context, NULL);

  if (NULL != context->auth &&
      SOUP_AUTH_TYPE_BASIC == context->auth->type)
    return context->auth->basic.passwd;

  return NULL;
}

/**
 * epc_auth_context_check_password:
 * @context: a #EpcAuthContext
 * @password: the expected password
 *
 * Verifies that the password supplied with the network request matches
 * the @password the application expects. There is no way to retrieve the
 * password from the #EpcAuthContext, as the network protocol transfers
 * just a hash code, not the actual password.
 *
 * Returns: %TRUE when the sent password matches, or %FALSE otherwise.
 */
gboolean
epc_auth_context_check_password (const EpcAuthContext *context,
                                 const gchar          *password)
{
  g_return_val_if_fail (NULL != context, FALSE);
  g_return_val_if_fail (NULL != password, FALSE);

  return
    NULL != context->auth &&
    soup_server_auth_check_passwd (context->auth, (gchar*) password);
    /* TODO: libsoup bug #493686 */
}

/* vim: set sw=2 sta et spl=en spell: */
