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
 *   epc_publisher_run ();
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
  PROP_SERVICE_NAME,
  PROP_SERVICE_DOMAIN,
  PROP_APPLICATION,

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
  GString *buffer;
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
};

/**
 * EpcPublisherPrivate:
 *
 * Private fields of the #EpcPublisher class.
 */
struct _EpcPublisherPrivate
{
  EpcDispatcher         *dispatcher;

  EpcResource           *default_resource;
  GHashTable            *resources;

  gboolean               server_started;
  GMainLoop             *server_loop;
  SoupServerAuthContext  server_auth;
  SoupServer            *server;

  EpcProtocol            protocol;
  gchar                 *service_name;
  gchar                 *service_domain;
  gchar                 *application;

  gchar                 *certificate_file;
  gchar                 *private_key_file;
};

extern gboolean _epc_debug;

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

  if (self->destroy_data)
    self->destroy_data (self->user_data);

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
      if (G_UNLIKELY (_epc_debug))
        g_debug ("%s: writing %d bytes", G_STRLOC, length);

      soup_message_add_chunk (message, SOUP_BUFFER_USER_OWNED, chunk, length);
    }
  else
    {
      if (G_UNLIKELY (_epc_debug))
        g_debug ("%s: done", G_STRLOC);

      soup_message_add_final_chunk (message);
    }
}

static void
epc_publisher_handle_get_path (SoupServerContext *context,
                               SoupMessage       *message,
                               gpointer           data)
{
  EpcPublisher *self = EPC_PUBLISHER (data);
  EpcResource *resource = NULL;
  EpcContents *contents = NULL;
  const gchar *key = NULL;

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: method=%s, path=%s", G_STRFUNC, message->method, context->path);

  if (SOUP_METHOD_ID_GET != context->method_id)
    {
      soup_message_set_status (message, SOUP_STATUS_METHOD_NOT_ALLOWED);
      return;
    }

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
}

static void
epc_publisher_list_resource_item_xml (gpointer key,
                                      gpointer value G_GNUC_UNUSED,
                                      gpointer data)
{
  EpcListContext *context = data;

  if (NULL == context->pattern ||
      g_pattern_match_string (context->pattern, key))
    {
      gchar *markup = g_markup_escape_text (key, -1);

      g_string_append (context->buffer, "<item><name>");
      g_string_append (context->buffer, markup);
      g_string_append (context->buffer, "</name></item>");

      g_free (markup);
    }
}

static void
epc_publisher_handle_list_path (SoupServerContext *context,
                                SoupMessage       *message,
                                gpointer           data)
{
  EpcListContext list_context;
  EpcPublisher *self = data;

  list_context.buffer = g_string_new ("<list>");
  list_context.pattern = NULL;

  if (g_str_has_prefix (context->path, "/list/") && '\0' != context->path[6])
    list_context.pattern = g_pattern_spec_new (context->path + 6);

  g_hash_table_foreach (self->priv->resources,
                        epc_publisher_list_resource_item_xml,
                        &list_context);

  g_string_append (list_context.buffer, "</list>");

  soup_message_set_response (message, "text/xml",
                             SOUP_BUFFER_USER_OWNED,
                             list_context.buffer->str,
                             list_context.buffer->len);
  soup_message_set_status (message, SOUP_STATUS_OK);

  g_signal_connect_swapped (message, "finished",
                            G_CALLBACK (g_free),
                            list_context.buffer->str);

  if (list_context.pattern)
    g_pattern_spec_free (list_context.pattern);

  g_string_free (list_context.buffer, FALSE);
}

static void
epc_publisher_list_resource_item_html (gpointer key,
                                       gpointer value G_GNUC_UNUSED,
                                       gpointer data)
{
  gchar *markup = g_markup_escape_text (key, -1);
  GString *contents = data;

  g_string_append (contents, "<li><a href=\"/get/");
  g_string_append (contents, markup);
  g_string_append (contents, "\">");
  g_string_append (contents, markup);
  g_string_append (contents, "</a></li>");

  g_free (markup);
}

static void
epc_publisher_handle_root (SoupServerContext *context,
                           SoupMessage       *message,
                           gpointer           data)
{
  EpcPublisher *self = data;

  if (g_str_equal (context->path, "/"))
    {
      GString *contents;
      gchar *title;

      title = g_markup_escape_text (self->priv->service_name, -1);
      contents = g_string_new (NULL);

      g_string_append (contents, "<html><head><title>");
      g_string_append (contents, title);
      g_string_append (contents, "</title></head><body><h1>");
      g_string_append (contents, title);
      g_string_append (contents, "</h1><ul>");

      g_hash_table_foreach (self->priv->resources,
                            epc_publisher_list_resource_item_html,
                            contents);

      g_string_append (contents, "</ul></body></html>");

      soup_message_set_response (message, "text/html",
                                 SOUP_BUFFER_USER_OWNED,
                                 contents->str, contents->len);
      soup_message_set_status (message, SOUP_STATUS_OK);

      g_signal_connect_swapped (message, "finished",
                                G_CALLBACK (g_free),
                                contents->str);

      g_string_free (contents, FALSE);
      g_free (title);
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

  context.auth = auth;
  context.publisher = EPC_PUBLISHER (data);
  context.key = epc_publisher_get_key (uri->path);

  if (NULL != auth)
    user = soup_server_auth_get_user (auth);
  if (NULL == user)
    auth_ctx->digest_info.realm = context.publisher->priv->service_name;
  if (NULL != context.key)
    resource = g_hash_table_lookup (context.publisher->priv->resources, context.key);
  if (NULL == resource)
    resource = context.publisher->priv->default_resource;

  if (resource && resource->auth_handler)
    authorized = resource->auth_handler (&context, user, resource->auth_user_data);

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: path=%s, resource=%p, auth_handler=%p, authorized=%d", G_STRLOC,
             uri->path, resource, resource ? resource->auth_handler : NULL, authorized);

  return authorized;
}

static void
epc_publisher_init (EpcPublisher *self)
{
  epc_shell_ref ();

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_PUBLISHER, EpcPublisherPrivate);

  self->priv->protocol = EPC_PROTOCOL_HTTPS;
  self->priv->server_auth.types = SOUP_AUTH_TYPE_DIGEST;
  self->priv->server_auth.callback = epc_publisher_server_auth_cb;
  self->priv->server_auth.user_data = self;

  /* TODO: Figure out why force_integrity doesn't work. */
  self->priv->server_auth.digest_info.force_integrity = FALSE;
  self->priv->server_auth.digest_info.allow_algorithms = SOUP_ALGORITHM_MD5;

  self->priv->resources = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, epc_resource_free);
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

static void
epc_publisher_announce (EpcPublisher  *self)
{
  const gchar *host;
  struct sockaddr *sockaddr;
  gint addrlen;

  gchar *service;
  gint port;

  g_return_if_fail (SOUP_IS_SERVER (self->priv->server));

  service = epc_service_type_new (self->priv->protocol, self->priv->application);

  host = epc_publisher_get_host (self, &sockaddr, &addrlen);
  port = epc_publisher_get_port (self);

  epc_dispatcher_reset (self->priv->dispatcher);

  epc_dispatcher_add_service (self->priv->dispatcher, sockaddr->sa_family,
                              epc_protocol_get_service_type (self->priv->protocol),
                              self->priv->service_domain, host, port, NULL);

  epc_dispatcher_add_service_subtype (self->priv->dispatcher,
                                      epc_protocol_get_service_type (self->priv->protocol),
                                      service);

  g_free (service);

  if (!host)
    host = epc_dispatcher_get_host_name (self->priv->dispatcher);

  service = epc_protocol_build_uri (self->priv->protocol, host, port, NULL);
  g_print ("%s: listening on %s\n", G_STRFUNC, service);
  g_free (service);
}

static gboolean
epc_publisher_is_server_created (EpcPublisher *self)
{
  return (NULL != self->priv->server);
}

static G_CONST_RETURN gchar*
epc_publisher_find_name (EpcPublisher *self)
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

static gboolean
epc_publisher_create_server (EpcPublisher  *self,
                             GError       **error)
{
  g_return_val_if_fail (!epc_publisher_is_server_created (self), FALSE);
  g_return_val_if_fail (NULL == self->priv->dispatcher, FALSE);

  self->priv->dispatcher = epc_dispatcher_new (epc_publisher_find_name (self));

  if (!epc_dispatcher_run (self->priv->dispatcher, error))
    return FALSE;

  if (EPC_PROTOCOL_UNKNOWN == self->priv->protocol)
    self->priv->protocol = EPC_PROTOCOL_HTTPS;

  if (EPC_PROTOCOL_HTTPS == self->priv->protocol && (
      NULL == self->priv->certificate_file ||
      NULL == self->priv->private_key_file))
    {
      GError *error = NULL;
      const gchar *host;

      g_free (self->priv->certificate_file);
      g_free (self->priv->private_key_file);

      host = epc_dispatcher_get_host_name (self->priv->dispatcher);

      if (!epc_tls_get_server_credentials (host,
                                           &self->priv->certificate_file,
                                           &self->priv->private_key_file,
                                           &error))
        {
          self->priv->protocol = EPC_PROTOCOL_HTTP;
          g_warning ("%s: Cannot retrieve server credentials, using insecure transport protocol: %s",
                     G_STRFUNC, error ? error->message : "No error details available.");
          g_clear_error (&error);

        }
    }

  self->priv->server =
    soup_server_new (SOUP_SERVER_SSL_CERT_FILE, self->priv->certificate_file,
                     SOUP_SERVER_SSL_KEY_FILE, self->priv->private_key_file,
                     SOUP_SERVER_PORT, SOUP_ADDRESS_ANY_PORT,
                     NULL);

  soup_server_add_handler (self->priv->server, "/get",
                           &self->priv->server_auth,
                           epc_publisher_handle_get_path,
                           NULL, self);
  soup_server_add_handler (self->priv->server, "/list",
                           &self->priv->server_auth,
                           epc_publisher_handle_list_path,
                           NULL, self);
  soup_server_add_handler (self->priv->server, "/",
                           &self->priv->server_auth,
                           epc_publisher_handle_root,
                           NULL, self);

  epc_publisher_announce (self);

  return TRUE;
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

      case PROP_SERVICE_NAME:
        g_free (self->priv->service_name);
        self->priv->service_name = g_value_dup_string (value);

        if (self->priv->dispatcher)
          epc_dispatcher_set_name (self->priv->dispatcher,
                                   epc_publisher_find_name (self));

        break;

      case PROP_SERVICE_DOMAIN:
        g_return_if_fail (!epc_publisher_is_server_created (self));

        g_free (self->priv->service_domain);
        self->priv->service_domain = g_value_dup_string (value);
        break;

      case PROP_APPLICATION:
        g_return_if_fail (!epc_publisher_is_server_created (self));
        self->priv->application = g_value_dup_string (value);
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

      case PROP_SERVICE_NAME:
        g_value_set_string (value, self->priv->service_name);
        break;

      case PROP_SERVICE_DOMAIN:
        g_value_set_string (value, self->priv->service_domain);
        break;

      case PROP_APPLICATION:
        g_value_set_string (value, self->priv->application);
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

  G_OBJECT_CLASS (epc_publisher_parent_class)->dispose (object);
}

static void
epc_publisher_finalize (GObject *object)
{
  G_OBJECT_CLASS (epc_publisher_parent_class)->finalize (object);
  epc_shell_unref ();
}

static void
epc_publisher_class_init (EpcPublisherClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_publisher_set_property;
  oclass->get_property = epc_publisher_get_property;
  oclass->finalize = epc_publisher_finalize;
  oclass->dispose = epc_publisher_dispose;

  g_object_class_install_property (oclass, PROP_PROTOCOL,
                                   g_param_spec_enum ("protocol", "Protocol",
                                                      "The transport protocol the publisher uses",
                                                      EPC_TYPE_PROTOCOL, EPC_PROTOCOL_HTTPS,
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

  g_object_class_install_property (oclass, PROP_APPLICATION,
                                   g_param_spec_string ("application", "Application",
                                                        "Program name for deriving the service type",
                                                        NULL,
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
 * @key for addressing. The publisher delivers the current content
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
 * @key: the key for addressing the content
 * @handler: the #EpcContentsHandler for handling this content
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

  resource = epc_resource_new (handler, user_data, destroy_data);
  g_hash_table_insert (self->priv->resources, g_strdup (key), resource);
}

/**
 * epc_publisher_get_url:
 * @publisher: a #EpcPublisher
 * @key: the resource key to inspect, or %NULL
 *
 * Queries the fully qualified URL for accessing the resource published under
 * @key. This is useful when referencing keys in published resources. When
 * passing %NULL the publisher's base URL is returned.
 *
 * Returns: The fully qualified URL for @key, or the publisher's
 * base URL when passing %NULL for @key.
 */
gchar*
epc_publisher_get_url (EpcPublisher *self,
                       const gchar  *key)
{
  gchar *path = NULL;
  gchar *url = NULL;

  const gchar *host;
  gint port;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);

  host = epc_publisher_get_host (self, NULL, NULL);
  port = epc_publisher_get_port (self);

  if (!host)
    host = epc_dispatcher_get_host_name (self->priv->dispatcher);

  if (key)
    {
      gchar *encoded_key = soup_uri_encode (key, NULL);
      path = g_strconcat ("/get/", encoded_key, NULL);
      g_free (encoded_key);
    }

  url = epc_protocol_build_uri (self->priv->protocol, host, port, path);
  g_free (path);

  return url;
}

/**
 * epc_publisher_remove:
 * @publisher: a #EpcPublisher
 * @key: the key for addressing the content
 *
 * Removes a key and its associated content from a #EpcPublisher.
 *
 * Returns: %TRUE if the key was found and removed from the #EpcPublisher.
 */
gboolean
epc_publisher_remove (EpcPublisher *self,
                      const gchar  *key)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), FALSE);
  g_return_val_if_fail (NULL != key, FALSE);

  return g_hash_table_remove (self->priv->resources, key);
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

  g_hash_table_foreach (self->priv->resources,
                        epc_publisher_list_cb,
                        &context);

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

  if (NULL != key)
    {
      resource = g_hash_table_lookup (self->priv->resources, key);

      if (NULL == resource)
        {
          g_warning ("%s: No resource handler found for key `%s'", G_STRFUNC, key);
          return;
        }
    }
  else
    {
      if (NULL == self->priv->default_resource)
        self->priv->default_resource = epc_resource_new (NULL, NULL, NULL);

      resource = self->priv->default_resource;
    }

  epc_resource_set_auth_handler (resource, handler, user_data, destroy_data);
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
 * Returns: The transport protocol the publisher uses, or #EPC_PROTOCOL_UNKNOWN.
 */
EpcProtocol
epc_publisher_get_protocol (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), EPC_PROTOCOL_UNKNOWN);
  return self->priv->protocol;
}

/**
 * epc_publisher_run:
 * @publisher: a #EpcPublisher
 * @error: return location for a #GError, or %NULL
 *
 * Starts the server component of the #EpcPublisher and blocks until it is
 * shutdown using #epc_publisher_quit. If the server was not started, the
 * function returns %FALSE and sets @error. The error domain is
 * #EPC_AVAHI_ERROR. Possible error codes are those of the
 * <citetitle>Avahi</citetitle> library.
 *
 * When starting the publisher in HTTPS mode for the first time, self-signed
 * keys have to be generated. Generating secure keys needs quite some time,
 * therefore it is recommended to call #epc_progress_window_install, or
 * #epc_shell_set_progress_hooks to provide visual feedback during that
 * operation. Key generation takes place in a separate background thread,
 * the calling thread waits in a GMainLoop. Therefore the UI should remain
 * responsive, when generating keys.
 *
 * To start the server without blocking call #epc_publisher_run_async.
 *
 * Returns: %TRUE when the publisher was started successfully,
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
 * server was not started, the function returns %FALSE and sets @error. The
 * error domain is #EPC_AVAHI_ERROR. Possible error codes are those of the
 * <citetitle>Avahi</citetitle> library.
 *
 * To stop the server component call #epc_publisher_quit.
 * See #epc_publisher_run for additional information.
 *
 * Returns: %TRUE when the publisher was started successfully,
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

/**
 * epc_publisher_quit:
 * @publisher: a #EpcPublisher
 *
 * Stops the server component of the #EpcPublisher
 * started with #epc_publisher_run or #epc_publisher_run_async.
 */
void
epc_publisher_quit (EpcPublisher *self)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));

  if (self->priv->server_loop)
    g_main_loop_quit (self->priv->server_loop);
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
epc_auth_context_get_publisher (EpcAuthContext *context)
{
  g_return_val_if_fail (NULL != context, NULL);
  return context->publisher;
}

/**
 * epc_auth_context_get_key:
 * @context: a #EpcAuthContext
 *
 * Queries the resource key assosiated with the authentication @context.
 *
 * Returns: The resource key.
 */
G_CONST_RETURN gchar*
epc_auth_context_get_key (EpcAuthContext *context)
{
  g_return_val_if_fail (NULL != context, NULL);
  return context->key;
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
epc_auth_context_check_password (EpcAuthContext *context,
                                 const gchar    *password)
{
  g_return_val_if_fail (NULL != context, FALSE);
  g_return_val_if_fail (NULL != password, FALSE);

  return
    NULL != context->auth &&
    soup_server_auth_check_passwd (context->auth, (gchar*) password);
    /* TODO: libsoup bug #493686 */
}

/* vim: set sw=2 sta et spl=en spell: */
