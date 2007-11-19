#include "consumer.h"

#include <libsoup/soup-session-sync.h>
#include <string.h>

enum
{
  PROP_NONE,
  PROP_HOST,
  PROP_PORT
};

struct _EpcConsumerPrivate
{
  SoupSession *session;
  guint16 port;
  gchar *host;
};

G_DEFINE_TYPE (EpcConsumer, epc_consumer, G_TYPE_OBJECT);

static void
epc_consumer_init (EpcConsumer *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_CONSUMER, EpcConsumerPrivate);
  self->priv->session = soup_session_sync_new ();
}

static void
epc_consumer_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  switch (prop_id)
    {
      case PROP_HOST:
        g_assert (NULL == self->priv->host);
        self->priv->host = g_value_dup_string (value);
        break;

      case PROP_PORT:
        g_assert (0 == self->priv->port);
        self->priv->port = g_value_get_int (value);
        g_assert (self->priv->port > 0);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_consumer_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  switch (prop_id)
    {
      case PROP_HOST:
        g_value_set_string (value, self->priv->host);
        break;

      case PROP_PORT:
        g_value_set_int (value, self->priv->port);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_consumer_dispose (GObject *object)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  if (self->priv->session)
    {
      g_object_unref (self->priv->session);
      self->priv->session = NULL;
    }

  g_free (self->priv->host);
  self->priv->host = NULL;

  G_OBJECT_CLASS (epc_consumer_parent_class)->dispose (object);
}

static void
epc_consumer_class_init (EpcConsumerClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_consumer_set_property;
  oclass->get_property = epc_consumer_get_property;
  oclass->dispose = epc_consumer_dispose;

  g_object_class_install_property (oclass, PROP_HOST,
                                   g_param_spec_string ("host", "Host",
                                                        "Hostname of the publisher service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_PORT,
                                   g_param_spec_int ("port", "Port",
                                                     "TCP/IP port of the publisher service",
                                                     1, G_MAXUINT16, 80,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcConsumerPrivate));
}

EpcConsumer*
epc_consumer_new (const gchar *host,
                  guint16      port)
{
  g_return_val_if_fail (NULL != host, NULL);
  g_return_val_if_fail (port > 0, NULL);

  return g_object_new (EPC_TYPE_CONSUMER,
                       "host", host, "port", port,
                       NULL);
}

static SoupMessage*
epc_consumer_create_request (EpcConsumer *self,
                             const gchar *path)
{
  SoupMessage *request;
  char *request_uri;

  if (NULL == path)
    path = "/";

  g_assert ('/' == path[0]);

  request_uri = g_strdup_printf ("http://%s:%d/%s", self->priv->host,
                                 self->priv->port, path + 1);
  request = soup_message_new ("GET", request_uri);
  g_free (request_uri);

  return request;
}

gchar*
epc_consumer_lookup (EpcConsumer *self,
                     const gchar *key,
                     gsize       *length)
{
  SoupMessage *request = NULL;
  gchar *contents = NULL;
  gchar *path = NULL;
  gint status = 0;

  g_return_val_if_fail (EPC_IS_CONSUMER (self), NULL);
  g_return_val_if_fail (NULL != key, NULL);

  path = g_strconcat ("/get/", key, NULL);
  request = epc_consumer_create_request (self, path);
  g_free (path);

  status = soup_session_send_message (self->priv->session, request);

  if (SOUP_STATUS_OK == status)
    {
      if (length)
        *length = request->response.length;

      contents = g_malloc (request->response.length);
      memcpy (contents, request->response.body,
              request->response.length);
    }

  g_object_unref (request);
  return contents;
}
