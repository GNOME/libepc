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

/**
 * SECTION:publisher
 * @short_description: easily publish values
 * @see_also: #EpcConsumer
 * @include: libepc/publish.h
 *
 * The #EpcPublisher starts a HTTP server to publish information.
 * To allow #EpcConsumer to find it, it automatically publishes
 * its contact information (host name, TCP/IP port) per DNS-SD.
 *
 * Also there are some ideas on using DNS-DS to notify #EpcConsumer on changes.
 *
 * <example>
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
 */

#include "publisher.h"
#include "avahi-shell.h"
#include "dispatcher.h"
#include "service-names.h"

#include <libsoup/soup-address.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-server.h>
#include <libsoup/soup-server-auth.h>
#include <libsoup/soup-socket.h>

#include <string.h>

typedef struct _EpcPublication        EpcPublication;

enum
{
  PROP_NONE,
  PROP_NAME,
  PROP_DOMAIN,
  PROP_SERVICE
};

/**
 * EpcAuthContext:
 *
 * This data structure describes a pending authentication request
 * which shall be verified by an #EpcAuthHandler installed by
 * #epc_publisher_set_auth_handler.
 */
struct _EpcAuthContext
{
  SoupServerAuth *auth;
  EpcPublisher   *publisher;
  const gchar    *key;
};

/**
 * EpcContent:
 *
 * A reference counted buffer for storing content to deliver by the
 * #EpcPublisher. Use #epc_content_new to create instances of this buffer.
 */
struct _EpcContent
{
  volatile gint ref_count;
  gsize         length;
  gpointer      data;
  gchar        *type;
};

struct _EpcPublication
{
  EpcContentHandler handler;
  gpointer          user_data;
  GDestroyNotify    destroy_data;

  EpcAuthHandler    auth_handler;
  gpointer          auth_user_data;
  GDestroyNotify    auth_destroy_data;
};

/**
 * EpcPublisherPrivate:
 *
 * Private fields of the #EpcPublisher class.
 */
struct _EpcPublisherPrivate
{
  EpcDispatcher         *dispatcher;
  GHashTable            *publications;
  EpcPublication        *default_publication;
  SoupServerAuthContext  server_auth;
  SoupServer            *server;
  gboolean               running;

  gchar *name;
  gchar *domain;
  gchar *service;
};

/**
 * epc_content_new:
 * @type: the MIME type of this content, or %NULL
 * @data: the contents for this buffer
 * @length: the content length in bytes
 *
 * Creates a new #EpcContent buffer.
 * Passing %NULL for @type is equivalent to passing "application/octet-stream".
 *
 * Returns: The newly created #EpcContent buffer.
 */
EpcContent*
epc_content_new (const gchar *type,
                 gpointer     data,
                 gsize        length)
{
  EpcContent *content = g_slice_new0 (EpcContent);

  content->ref_count = 1;

  if (type)
    content->type = g_strdup (type);

  content->data = data;
  content->length = length;

  return content;
}

/**
 * epc_content_ref:
 * @content: a EpcContent buffer
 *
 * Increases the reference count of @content.
 *
 * Returns: the same @content buffer.
 */
EpcContent*
epc_content_ref (EpcContent *content)
{
  g_return_val_if_fail (NULL != content, NULL);
  g_atomic_int_inc (&content->ref_count);
  return content;
}

/**
 * epc_content_unref:
 * @content: a EpcContent buffer
 *
 * Decreases the reference count of @content.
 * When its reference count drops to 0, the buffer is released
 * (i.e. its memory is freed).
 */
void
epc_content_unref (EpcContent *content)
{
  g_return_if_fail (NULL != content);

  if (g_atomic_int_dec_and_test (&content->ref_count))
    g_slice_free (EpcContent, content);
}

static EpcPublication*
epc_publication_new (EpcContentHandler handler,
                     gpointer          user_data,
                     GDestroyNotify    destroy_data)
{
  EpcPublication *self = g_slice_new0 (EpcPublication);

  self->handler = handler;
  self->user_data = user_data;
  self->destroy_data = destroy_data;

  return self;
}

static void
epc_publication_free (gpointer data)
{
  EpcPublication *self = data;

  if (self->destroy_data)
    self->destroy_data (self->user_data);

  g_slice_free (EpcPublication, self);
}

static void
epc_publication_set_auth_handler (EpcPublication *self,
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

static EpcContent*
epc_publisher_handle_static (EpcPublisher *publisher G_GNUC_UNUSED,
                             const gchar  *key G_GNUC_UNUSED,
                             gpointer      user_data)
{
  return epc_content_ref (user_data);
}

static EpcContent*
epc_publisher_handle_file (EpcPublisher *publisher G_GNUC_UNUSED,
                           const gchar  *key G_GNUC_UNUSED,
                           gpointer      user_data)
{
  const gchar *filename = user_data;
  EpcContent *content = NULL;
  gchar *data = NULL;
  gsize length = 0;

  /* TODO: use gio to determinate mime-type */
  if (g_file_get_contents (filename, &data, &length, NULL))
    content = epc_content_new (NULL, data, length);

  return content;
}

G_DEFINE_TYPE (EpcPublisher, epc_publisher, G_TYPE_OBJECT);

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
epc_publisher_server_get_handler (SoupServerContext *context,
                                  SoupMessage       *message,
                                  gpointer           data)
{
  EpcPublisher *self = data;
  EpcPublication *publication = NULL;
  EpcContent *content = NULL;
  const gchar *key = NULL;

  key = epc_publisher_get_key (context->path);

  if (key)
    publication = g_hash_table_lookup (self->priv->publications, key);
  if (publication && publication->handler)
    content = publication->handler (self, key, publication->user_data);

  if (content)
    {
      const gchar *mime_type = content->type;

      if (!mime_type)
        mime_type = "application/octet-stream";

      soup_message_set_response (message, mime_type,
                                 SOUP_BUFFER_USER_OWNED,
                                 content->data, content->length);
      soup_message_set_status (message, SOUP_STATUS_OK);

      g_signal_connect_swapped (message, "finished",
                                G_CALLBACK (epc_content_unref),
                                content);
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
  EpcPublication *publication = NULL;
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
    auth_ctx->digest_info.realm = context.publisher->priv->name;
  if (NULL != context.key)
    publication = g_hash_table_lookup (context.publisher->priv->publications, context.key);
  if (NULL == publication)
    publication = context.publisher->priv->default_publication;

  if (publication && publication->auth_handler)
    return publication->auth_handler (&context, user, publication->auth_user_data);

  return FALSE;
}

static void
epc_publisher_init (EpcPublisher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_PUBLISHER, EpcPublisherPrivate);

  self->priv->server_auth.types = SOUP_AUTH_TYPE_DIGEST;
  self->priv->server_auth.callback = epc_publisher_server_auth_cb;
  self->priv->server_auth.user_data = self;

  self->priv->server_auth.digest_info.allow_algorithms = SOUP_ALGORITHM_MD5;
  self->priv->server_auth.digest_info.force_integrity = FALSE;
  /* TODO: Figure out why force_integrity doesn't work. */

  self->priv->server = soup_server_new (SOUP_SERVER_PORT, SOUP_ADDRESS_ANY_PORT, NULL);
  g_object_ref (self->priv->server); /* work arround bug #494128. */

  soup_server_add_handler (self->priv->server, "/get", &self->priv->server_auth,
                           epc_publisher_server_get_handler, NULL, self);

  self->priv->publications = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    g_free, epc_publication_free);
}

static void
epc_publisher_constructed (GObject *object)
{
  EpcPublisher *self;

  SoupSocket *listener;
  SoupAddress *address;

  const gchar *name;
  const gchar *host;
  gint port;

  AvahiProtocol protocol;
  struct sockaddr *sockaddr;
  gint addrlen;

  if (G_OBJECT_CLASS (epc_publisher_parent_class)->constructed)
    G_OBJECT_CLASS (epc_publisher_parent_class)->constructed (object);

  self = EPC_PUBLISHER (object);
  name = self->priv->name;

  if (!name)
    name = g_get_application_name ();
  if (!name)
    name = g_get_prgname ();

  if (!name)
    {
      gint hash = g_random_int ();

      name = G_OBJECT_TYPE_NAME (self);
      self->priv->name = g_strdup_printf ("%s-%08x", name, hash);
      name = self->priv->name;

      g_warning ("%s: No service name set - using generated name (`%s'). "
                 "Consider passing a service name to the publisher's "
                 "constructor or call g_set_application_name().",
                 G_STRLOC, name);
    }

  if (!self->priv->name)
    self->priv->name = g_strdup (name);

  if (!self->priv->service)
    self->priv->service = epc_avahi_shell_create_service_type (NULL);

  listener = soup_server_get_listener (self->priv->server);
  port = soup_server_get_port (self->priv->server);

  address = soup_socket_get_local_address (listener);
  sockaddr = soup_address_get_sockaddr (address, &addrlen);
  protocol = avahi_af_to_proto (sockaddr->sa_family);
  host = soup_address_get_name (address);

  g_print ("listening on http://%s:%d/\n", host ? host : "localhost", port);
  self->priv->dispatcher = epc_dispatcher_new (AVAHI_IF_UNSPEC, protocol, name);

  epc_dispatcher_add_service (self->priv->dispatcher, EPC_SERVICE_NAME_HTTP,
                              self->priv->domain, host, port,
                              NULL);

  if (self->priv->service)
    epc_dispatcher_add_service_subtype (self->priv->dispatcher,
                                        EPC_SERVICE_NAME,
                                        self->priv->service);
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
      case PROP_NAME:
        g_free (self->priv->name);
        self->priv->name = g_value_dup_string (value);
        break;

      case PROP_DOMAIN:
        g_return_if_fail (NULL == self->priv->domain);
        self->priv->domain = g_value_dup_string (value);
        break;

      case PROP_SERVICE:
        g_return_if_fail (NULL == self->priv->service);
        self->priv->service = g_value_dup_string (value);
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
      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;

      case PROP_DOMAIN:
        g_value_set_string (value, self->priv->domain);
        break;

      case PROP_SERVICE:
        g_value_set_string (value, self->priv->service);
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

  if (self->priv->dispatcher)
    {
      g_object_unref (self->priv->dispatcher);
      self->priv->dispatcher = NULL;
    }

  if (self->priv->server)
    {
      if (self->priv->running)
        soup_server_quit (self->priv->server);

      g_object_unref (self->priv->server);
      self->priv->server = NULL;
    }

  if (self->priv->publications)
    {
      g_hash_table_unref (self->priv->publications);
      self->priv->publications = NULL;
    }

  if (self->priv->default_publication)
    {
      epc_publication_free (self->priv->default_publication);
      self->priv->default_publication = NULL;
    }

  g_free (self->priv->name);
  self->priv->name = NULL;

  g_free (self->priv->domain);
  self->priv->domain = NULL;

  g_free (self->priv->service);
  self->priv->service = NULL;

  G_OBJECT_CLASS (epc_publisher_parent_class)->dispose (object);
}

static void
epc_publisher_class_init (EpcPublisherClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->constructed = epc_publisher_constructed;
  oclass->set_property = epc_publisher_set_property;
  oclass->get_property = epc_publisher_get_property;
  oclass->dispose = epc_publisher_dispose;

  g_object_class_install_property (oclass, PROP_NAME,
                                   g_param_spec_string ("name", "Name",
                                                        "User friendly name for the service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_DOMAIN,
                                   g_param_spec_string ("domain", "Domain",
                                                        "Internet domain for publishing the service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_SERVICE,
                                   g_param_spec_string ("service", "Service",
                                                        "Wellknown DNS-SD name the service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcPublisherPrivate));
}

/**
 * epc_publisher_new:
 * @name: the human friendly service name, or %NULL
 * @service: the DNS-SD service name, or %NULL
 * @domain: the DNS domain, or %NULL
 *
 * Creates a new #EpcPublisher object. The publisher announces its service
 * per DNS-SD to the DNS domain specified by @domain, using @name as service
 * name and @service as service type.
 *
 * You have to call <function>g_set_application_name</function>, when passing
 * %NULL for @name, as the result of <function>g_set_application_name</function>
 * will be used in that case. When %NULL is passed for @service,
 * #EPC_SERVICE_NAME_HTTP is used. Passing %NULL for @domain lets the DNS-DS
 * daemon choose.
 *
 * Returns: The newly created #EpcPublisher object.
 */
EpcPublisher*
epc_publisher_new (const gchar *name,
                   const gchar *service,
                   const gchar *domain)
{
  return g_object_new (EPC_TYPE_PUBLISHER, "name", name,
                       "service", service, "domain", domain, NULL);
}

/**
 * epc_publisher_add:
 * @publisher: a #EpcPublisher
 * @key: the key for addressing the value
 * @value: the value to publish
 * @length: the length of @value in bytes, or -1.
 *
 * Publishes a new @value on the #EpcPublisher using the unique @key for
 * addressing. When -1 is passed for @length, @value is expected to be a
 * null-terminated string and its length is determined automatically
 * using <function>strlen</function>.
 *
 * <note><para>
 *  Values published by the #EpcPublisher can be arbitrary data, possibly
 *  including null characters in the middle. The kind of data associated
 *  with a @key is chosen by the application providing values and has
 *  to be specified separately.
 *
 *  Even though, when publishing plain text it is strongly recommended
 *  to use UTF-8 encoding to avoid internationalization issues.
 * </para></note>
 */
void
epc_publisher_add (EpcPublisher  *self,
                   const gchar   *key,
                   const gchar   *value,
                   gssize         length)
{
  const gchar *type = NULL;
  gchar *data = NULL;

  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != value);
  g_return_if_fail (NULL != key);

  if (-1 == length)
    {
      length = strlen (value);
      type = "text/plain";
    }

  data = g_malloc (length);
  memcpy (data, value, length);

  epc_publisher_add_handler (self, key,
                             epc_publisher_handle_static,
                             epc_content_new (type, data, length),
                             (GDestroyNotify) epc_content_unref);
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
 * @handler: the #EpcContentHandler for handling this content
 * @user_data: data to pass on @handler calls
 * @destroy_data: a function for releasing @user_data
 *
 * Publishes content on the #EpcPublisher which is generated by a custom
 * #EpcContentHandler. This is the most flexible method for publishing
 * information.
 *
 * The @handler is called on every request matching @key.
 * On this call @publisher, @key and @user_data are passed to the @handler.
 * When replacing or deleting the resource referenced by @key, the function
 * described by @destroy_data is called with @user_data as argument.
 */
void
epc_publisher_add_handler (EpcPublisher      *self,
                           const gchar       *key,
                           EpcContentHandler  handler,
                           gpointer           user_data,
                           GDestroyNotify     destroy_data)
{
  EpcPublication *publication;

  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != handler);
  g_return_if_fail (NULL != key);

  publication = epc_publication_new (handler, user_data, destroy_data);
  g_hash_table_insert (self->priv->publications, g_strdup (key), publication);
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
 * When replacing or deleting the resource referenced by @key, the function
 * described by @destroy_data is called with @user_data as argument.
 */
void
epc_publisher_set_auth_handler (EpcPublisher   *self,
                                const gchar    *key,
                                EpcAuthHandler  handler,
                                gpointer        user_data,
                                GDestroyNotify  destroy_data)
{
  EpcPublication *publication;

  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != handler);

  if (NULL != key)
    {
      publication = g_hash_table_lookup (self->priv->publications, key);

      if (NULL == publication)
        {
          g_warning ("%s: No publication found for key `%s'", G_STRLOC, key);
          return;
        }
    }
  else
    {
      if (NULL == self->priv->default_publication)
        self->priv->default_publication = epc_publication_new (NULL, NULL, NULL);

      publication = self->priv->default_publication;
    }

  epc_publication_set_auth_handler (publication, handler, user_data, destroy_data);
}

/**
 * epc_publisher_set_name:
 * @publisher: a #EpcPublisher
 * @name: the new name of this #EpcPublisher
 *
 * Changes the human friendly name this #EpcPublisher uses
 * to announce its service. See #EpcPublisher:name for details.
 */
void
epc_publisher_set_name (EpcPublisher *self,
                        const gchar  *name)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_object_set (self, "name", name, NULL);
}

/**
 * epc_publisher_get_name:
 * @publisher: a #EpcPublisher
 *
 * Queries the human friendly name this #EpcPublisher uses
 * to announce its service. See #EpcPublisher:name for details.
 *
 * Returns: The human friendly name of this #EpcPublisher.
 */
G_CONST_RETURN char*
epc_publisher_get_name (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->name;
}

/**
 * epc_publisher_get_domain:
 * @publisher: a #EpcPublisher
 *
 * Queries the DNS domain for which this #EpcPublisher announces its service.
 * See #EpcPublisher:domain for details.
 *
 * Returns: The DNS-SD domain of this #EpcPublisher, or %NULL.
 */
G_CONST_RETURN char*
epc_publisher_get_domain (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->domain;
}

/**
 * epc_publisher_get_service:
 * @publisher: a #EpcPublisher
 *
 * Queries the DNS-SD service name of this #EpcPublisher.
 * See #EpcPublisher:service for details.
 *
 * Returns: The DNS-SD service name of this #EpcPublisher.
 */
G_CONST_RETURN char*
epc_publisher_get_service (EpcPublisher *self)
{
  g_return_val_if_fail (EPC_IS_PUBLISHER (self), NULL);
  return self->priv->service;
}

/**
 * epc_publisher_run:
 * @publisher: a #EpcPublisher
 *
 * Starts the server component of the #EpcPublisher
 * and blocks until it is shutdown using #epc_publisher_quit.
 *
 * To start the server without blocking call #epc_publisher_run_async.
 */
void
epc_publisher_run (EpcPublisher *self)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  soup_server_run (self->priv->server);
}

/**
 * epc_publisher_run_async:
 * @publisher: a #EpcPublisher
 *
 * Starts the server component of the #EpcPublisher without blocking.
 * To start the server without blocking call #epc_publisher_run_async.
 * To stop the server component call #epc_publisher_quit.
 */
void
epc_publisher_run_async (EpcPublisher *self)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  soup_server_run_async (self->priv->server);
  self->priv->running = TRUE;
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
  soup_server_quit (self->priv->server);
  self->priv->running = FALSE;
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
 * Verifies that the password passed with the authentication request described
 * by @context matches the expected @password. The password cannot be verify
 * directly (i.e. by <function>strcmp</function>) because the authentication
 * mechanism sends hash codes ("fingerprints") instead of the actual password
 * password.
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
