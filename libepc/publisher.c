#include "publisher.h"
#include "dispatcher.h"

#include <libsoup/soup-address.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-server.h>
#include <libsoup/soup-socket.h>

#include <string.h>

/**
 * SECTION:publisher
 * @short_description: this object is used to publish values
 * @see_also: #EpcConsumer
 * @include: libepc/publish.h
 *
 * The #EpcPublisher starts a HTTP server to publish information.
 * To allow #EpcConsumer to find it, it automatically publishes
 * its contact information (host name, TCP/IP port) per DNS-SD.
 *
 * Currently neither encryption or autentication are implemented,
 * but it is planed to change this in the future.
 *
 * Also there are some ideas on using DNS-DS to notify #EpcConsumer on changes.
 */

typedef struct _EpcRecord EpcRecord;

enum
{
  PROP_NONE,
  PROP_NAME,
  PROP_DOMAIN,
  PROP_SERVICE
};

struct _EpcRecord
{
  gchar *mime_type;
  gchar *contents;
  gsize  length;
};

struct _EpcPublisherPrivate
{
  EpcDispatcher *dispatcher;
  GHashTable *records;
  SoupServer *server;

  gchar *name;
  gchar *domain;
  gchar *service;
};

static EpcRecord*
epc_record_new (const gchar *mime_type,
                gchar       *contents,
                gsize        length)
{
  EpcRecord *record = g_slice_new0 (EpcRecord);

  record->mime_type = g_strdup (mime_type);
  record->contents = contents;
  record->length = length;

  return record;
}

static EpcRecord*
epc_record_dup (const gchar *mime_type,
                const gchar *contents,
                gsize        length)
{
  gchar *copy = g_malloc (length);
  memcpy (copy, contents, length);

  return epc_record_new (mime_type, copy, length);
}

static void
epc_record_free (EpcRecord* record)
{
  g_free (record->contents);
  g_free (record->mime_type);

  g_slice_free (EpcRecord, record);
}

G_DEFINE_TYPE (EpcPublisher, epc_publisher, G_TYPE_OBJECT);

static void
epc_publisher_get_handler (SoupServerContext *context,
                           SoupMessage       *message,
                           gpointer           data)
{
  EpcPublisher *self = data;
  EpcRecord *record = NULL;
  const gchar *key = NULL;

  if ('/' == *context->path)
    key = context->path + 1;
  if (key)
    key = strchr (key, '/');
  if (key)
    key += 1;

  if (key)
    record = g_hash_table_lookup (self->priv->records, key);

  if (record)
    {
      const char *mime_type = record->mime_type;

      if (!mime_type)
        mime_type = "application/octet-stream";

      soup_message_set_response (message,
                                 mime_type, SOUP_BUFFER_STATIC,
                                 record->contents, record->length);
      soup_message_set_status (message, SOUP_STATUS_OK);
    }
  else
    soup_message_set_status (message, SOUP_STATUS_NOT_FOUND);
}

static void
epc_publisher_init (EpcPublisher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_PUBLISHER, EpcPublisherPrivate);

  self->priv->server = soup_server_new (SOUP_SERVER_PORT, SOUP_ADDRESS_ANY_PORT, NULL);
  soup_server_add_handler (self->priv->server, "/get", NULL, epc_publisher_get_handler, NULL, self);

  self->priv->records = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                               (GDestroyNotify)epc_record_free);
}

static void
epc_publisher_constructed (GObject *object)
{
  EpcPublisher *self;

  SoupSocket *listener;
  SoupAddress *address;

  const gchar *name;
  const gchar *domain;
  const gchar *service;
  const gchar *host;
  gint port;

  AvahiProtocol protocol;
  struct sockaddr *sockaddr;
  gint addrlen;

  if (G_OBJECT_CLASS (epc_publisher_parent_class)->constructed)
    G_OBJECT_CLASS (epc_publisher_parent_class)->constructed (object);

  self = EPC_PUBLISHER (object);
  name = self->priv->name;
  domain = self->priv->domain;
  service = self->priv->service;

  if (!name)
    name = g_get_application_name ();
  if (!name)
    name = g_get_prgname ();
  if (!service)
    service = EPC_PUBLISHER_SERVICE_NAME;

  listener = soup_server_get_listener (self->priv->server);
  port = soup_server_get_port (self->priv->server);

  address = soup_socket_get_local_address (listener);
  sockaddr = soup_address_get_sockaddr (address, &addrlen);
  protocol = avahi_af_to_proto (sockaddr->sa_family);
  host = soup_address_get_name (address);

  g_print ("listening on http://%s:%d/\n", host ? host : "localhost", port);
  self->priv->dispatcher = epc_dispatcher_new (AVAHI_IF_UNSPEC, protocol, name);

  epc_dispatcher_add_service (self->priv->dispatcher, service,
                              self->priv->domain, host, port,
                              NULL);
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
        g_return_if_fail (NULL != self->priv->domain);
        self->priv->domain = g_value_dup_string (value);
        break;

      case PROP_SERVICE:
        g_return_if_fail (NULL != self->priv->service);
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
      soup_server_quit (self->priv->server);
      g_object_unref (self->priv->server);
      self->priv->server = NULL;
    }

  if (self->priv->records)
    {
      g_hash_table_unref (self->priv->records);
      self->priv->records = NULL;
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
 * @name: the humany friendly service name
 * @service: the DNS-SD service name, or %NULL
 * @domain: the DNS domain, or %NULL
 *
 * Creates a new #EpcPublisher object. The publisher announces its service
 * per DNS-SD to the DNS domain specified by @domain, using @name as service
 * name and @service as service type.
 *
 * When %NULL is passed for @service or @domain reasonable
 * default values are used.
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
 * @publisher: the publisher
 * @key: the key for addressing the value
 * @value: the value to publish
 * @length: the length of @value in bytes, or -1.
 *
 * Publishes a new @value on the #EpcPublisher using the unique @key for
 * addressing. When -1 is passed or @length, @value is expected to be a
 * null-terminated string and its length is determinated automatically
 * using #strlen.
 */
void
epc_publisher_add (EpcPublisher  *self,
                   const gchar   *key,
                   const gchar   *value,
                   gssize         length)
{
  EpcRecord *record;

  g_return_if_fail (EPC_IS_PUBLISHER (self));
  g_return_if_fail (NULL != value);
  g_return_if_fail (NULL != key);

  if (-1 == length)
    record = epc_record_dup ("text/plain", value, strlen (value));
  else
    record = epc_record_dup (NULL, value, length);

  g_hash_table_insert (self->priv->records, g_strdup (key), record);
}

/**
 * epc_publisher_add_file:
 * @publisher: the publisher
 * @key: the key for addressing the file
 * @filename: the name of the file to publish
 * @error: return location for a GError, or NULL
 *
 * Publishes the current content of a local file a new @value on the
 * #EpcPublisher using the unique @key for addressing.
 *
 * Returns: %TRUE on success, or %FALSE when the file cannot be read.
 */
gboolean
epc_publisher_add_file (EpcPublisher  *self,
                        const gchar   *key,
                        const gchar   *filename,
                        GError       **error)
{
  gchar *contents = NULL;
  gsize length = 0;

  g_return_val_if_fail (EPC_IS_PUBLISHER (self), FALSE);
  g_return_val_if_fail (NULL != filename, FALSE);
  g_return_val_if_fail (NULL != key, FALSE);

  /* TODO: use gio to determinate mime-type */

  if (g_file_get_contents (filename, &contents, &length, error))
    g_hash_table_insert (self->priv->records, g_strdup (key),
                         epc_record_new (NULL, contents, length));

  return (NULL != contents);
}

/**
 * epc_publisher_set_name:
 * @publisher: the publisher
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
 * @publisher: the publisher
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
 * @publisher: the publisher
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
 * @publisher: the publisher
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
 * @publisher: the publisher
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
 * @publisher: the publisher
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
}

/**
 * epc_publisher_quit:
 * @publisher: the publisher
 *
 * Stops the server component of the #EpcPublisher
 * started with #epc_publisher_run or #epc_publisher_run_async.
 */
void
epc_publisher_quit (EpcPublisher *self)
{
  g_return_if_fail (EPC_IS_PUBLISHER (self));
  soup_server_quit (self->priv->server);
}
