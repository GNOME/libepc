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
 * SECTION:consumer
 * @short_description: lookup published values
 * @see_also: #EpcPublisher
 * @include: libepc/consumer.h
 *
 * The #EpcConsumer object is used to lookup values published by an
 * #EpcPublisher service. Currently HTTP is used for communication.
 * To find a publisher, use DNS-SD (also known as ZeroConf) to
 * list #EPC_PUBLISHER_SERVICE_TYPE services.
 *
 * <example>
 *  <title>Lookup a value</title>
 *  <programlisting>
 *   your_app_discover_server (&host, &port);
 *   consumer = epc_consumer_new (host, port);
 *
 *   value = epc_consumer_lookup (consumer, "database.glom", NULL);
 *   g_object_unref (consumer);
 *
 *   your_app_consume_value (value);
 *   g_free (value);
 *  </programlisting>
 * </example>
 */

#include "consumer.h"
#include "marshal.h"
#include "shell.h"

#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <libsoup/soup-session-async.h>
#include <string.h>

enum
{
  PROP_NONE,
  PROP_HOST,
  PROP_PORT,
  PROP_NAME,
  PROP_SERVICE,
  PROP_DOMAIN
};

enum
{
  SIGNAL_AUTHENTICATE,
  SIGNAL_REAUTHENTICATE,
  SIGNAL_LAST
};

/**
 * EpcConsumerPrivate:
 *
 * Private fields of the #EpcConsumer class.
 */
struct _EpcConsumerPrivate
{
  AvahiClient         *service_client;
  AvahiServiceBrowser *service_browser;
  SoupSession         *session;
  GMainLoop           *loop;

  guint16 port;
  gchar  *host;

  gchar *service_name;
  gchar *service_type;
  gchar *service_domain;
};

static guint signals[SIGNAL_LAST];
extern gboolean _epc_debug;

G_DEFINE_TYPE (EpcConsumer, epc_consumer, G_TYPE_OBJECT);

static void
epc_consumer_authenticate_cb (SoupSession  *session G_GNUC_UNUSED,
                              SoupMessage  *message G_GNUC_UNUSED,
                              gchar        *auth_type G_GNUC_UNUSED,
                              gchar        *auth_realm,
                              gchar       **username,
                              gchar       **password,
                              gpointer      data)
{
  EpcConsumer *self = EPC_CONSUMER (data);

  g_signal_emit (self, signals[SIGNAL_AUTHENTICATE], 0,
                 auth_realm, username, password);
}

static void
epc_consumer_reauthenticate_cb (SoupSession  *session G_GNUC_UNUSED,
                                SoupMessage  *message G_GNUC_UNUSED,
                                gchar        *auth_type G_GNUC_UNUSED,
                                gchar        *auth_realm,
                                gchar       **username,
                                gchar       **password,
                                gpointer      data)
{
  EpcConsumer *self = EPC_CONSUMER (data);

  g_signal_emit (self, signals[SIGNAL_REAUTHENTICATE], 0,
                 auth_realm, username, password);
}

static void
epc_consumer_init (EpcConsumer *self)
{
  epc_shell_ref ();

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_CONSUMER, EpcConsumerPrivate);
  self->priv->loop = g_main_loop_new (NULL, FALSE);
  self->priv->session = soup_session_async_new ();

  g_signal_connect (self->priv->session, "authenticate",
                    G_CALLBACK (epc_consumer_authenticate_cb), self);
  g_signal_connect (self->priv->session, "reauthenticate",
                    G_CALLBACK (epc_consumer_reauthenticate_cb), self);
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
        break;

      case PROP_NAME:
        g_assert (NULL == self->priv->service_name);
        self->priv->service_name = g_value_dup_string (value);
        break;

      case PROP_SERVICE:
        g_assert (NULL == self->priv->service_type);
        self->priv->service_type = g_value_dup_string (value);
        break;

      case PROP_DOMAIN:
        g_assert (NULL == self->priv->service_domain);
        self->priv->service_domain = g_value_dup_string (value);
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

      case PROP_NAME:
        g_value_set_string (value, self->priv->service_name);
        break;

      case PROP_SERVICE:
        g_value_set_string (value, self->priv->service_type);
        break;

      case PROP_DOMAIN:
        g_value_set_string (value, self->priv->service_domain);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_consumer_service_resolver_cb (AvahiServiceResolver   *resolver,
                                  AvahiIfIndex            interface G_GNUC_UNUSED,
                                  AvahiProtocol           protocol G_GNUC_UNUSED,
                                  AvahiResolverEvent      event G_GNUC_UNUSED,
                                  const char             *name G_GNUC_UNUSED,
                                  const char             *type G_GNUC_UNUSED,
                                  const char             *domain G_GNUC_UNUSED,
                                  const char             *hostname,
                                  const AvahiAddress     *a G_GNUC_UNUSED,
                                  uint16_t                port,
                                  AvahiStringList        *txt G_GNUC_UNUSED,
                                  AvahiLookupResultFlags  flags G_GNUC_UNUSED,
                                  void                   *data G_GNUC_UNUSED)
{
  EpcConsumer *self = data;

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: service resolved: host-name=%s, port=%d",
             G_STRLOC, hostname, port);

  g_main_loop_quit (self->priv->loop);

  g_free (self->priv->host);
  self->priv->host = g_strdup (hostname);
  self->priv->port = port;

  avahi_service_resolver_free (resolver);
}

static void
epc_consumer_service_brower_cb (AvahiServiceBrowser    *browser,
                                AvahiIfIndex            interface,
                                AvahiProtocol           protocol,
                                AvahiBrowserEvent       event,
                                const char             *name,
                                const char             *type,
                                const char             *domain,
                                AvahiLookupResultFlags  flags G_GNUC_UNUSED,
                                void                   *data)
{
  EpcConsumer *self = data;
  gint error;

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: event=%d, name=`%s', type=`%s', domain=`%s'",
             G_STRLOC, event, name, type, domain);

  switch (event)
    {
      case AVAHI_BROWSER_REMOVE:
        break;

      case AVAHI_BROWSER_FAILURE:
        error = avahi_client_errno (avahi_service_browser_get_client (browser));
        g_warning ("%s: %s (%d)", G_STRLOC, avahi_strerror (error), error);
        break;

      default:
        if (name && g_str_equal (name, self->priv->service_name))
          avahi_service_resolver_new (self->priv->service_client, interface,
                                      protocol, name, type, domain, AVAHI_PROTO_UNSPEC,
                                      0, epc_consumer_service_resolver_cb, self);

        break;
    }
}

static void
epc_consumer_constructed (GObject *object)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  if (G_OBJECT_CLASS (epc_consumer_parent_class)->constructed)
    G_OBJECT_CLASS (epc_consumer_parent_class)->constructed (object);

  if (!self->priv->service_type)
    self->priv->service_type = epc_shell_create_service_type (NULL);

  if (!self->priv->host && self->priv->service_type)
    {
      self->priv->service_client =
        epc_shell_create_avahi_client (AVAHI_CLIENT_NO_FAIL, NULL, NULL);

      g_return_if_fail (NULL != self->priv->service_client);

      self->priv->service_browser =
        avahi_service_browser_new (self->priv->service_client,
                                   AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                                   self->priv->service_type,
                                   self->priv->service_domain, 0,
                                   epc_consumer_service_brower_cb, self);

      g_return_if_fail (NULL != self->priv->service_browser);
    }
}

static void
epc_consumer_dispose (GObject *object)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  if (self->priv->service_browser)
    {
      avahi_service_browser_free (self->priv->service_browser);
      self->priv->service_browser = NULL;
    }

  if (self->priv->service_client)
    {
      avahi_client_free (self->priv->service_client);
      self->priv->service_client = NULL;
    }

  if (self->priv->session)
    {
      g_object_unref (self->priv->session);
      self->priv->session = NULL;
    }

  if (self->priv->loop)
    {
      g_main_loop_unref (self->priv->loop);
      self->priv->loop = NULL;
    }

  g_free (self->priv->host);
  self->priv->host = NULL;

  G_OBJECT_CLASS (epc_consumer_parent_class)->dispose (object);
}

static void
epc_consumer_finalize (GObject *object)
{
  epc_shell_unref ();
  G_OBJECT_CLASS (epc_consumer_parent_class)->finalize (object);
}

static void
epc_consumer_class_init (EpcConsumerClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_consumer_set_property;
  oclass->get_property = epc_consumer_get_property;
  oclass->constructed = epc_consumer_constructed;
  oclass->finalize = epc_consumer_finalize;
  oclass->dispose = epc_consumer_dispose;

  g_object_class_install_property (oclass, PROP_HOST,
                                   g_param_spec_string ("host", "Host",
                                                        "Host name of the publisher to use", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_PORT,
                                   g_param_spec_int ("port", "Port",
                                                     "TCP/IP port of the publisher to use",
                                                     0, G_MAXUINT16, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_NAME,
                                   g_param_spec_string ("name", "Name",
                                                        "Service name of the publisher to use", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_SERVICE,
                                   g_param_spec_string ("service", "Service",
                                                        "Service type of the publisher to use", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_DOMAIN,
                                   g_param_spec_string ("domain", "Domain",
                                                        "DNS domain of the publisher to use", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EpcConsumer::authenticate:
   * @consumer: the #EpcConsumer emitting the signal
   * @realm: the realm being authenticated to
   * @username: return location for the username (gchar*)
   * @password: return location for the password (gchar*)
   *
   * Emitted when the #EpcConsumer requires authentication. The credentials
   * may come for the user, or from cached information. If no credentials
   * are available leave @username and @password unchanged.
   *
   * If the provided credentials fail, the reauthenticate signal
   * will be emmitted.
   *
   * The consumer takes ownership of the credential strings.
   */
  signals[SIGNAL_AUTHENTICATE] = g_signal_new ("authenticate", EPC_TYPE_CONSUMER, G_SIGNAL_RUN_FIRST,
                                               G_STRUCT_OFFSET (EpcConsumerClass, authenticate), NULL, NULL,
                                               _epc_marshal_VOID__STRING_POINTER_POINTER, G_TYPE_NONE,
                                               3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * EpcConsumer::reauthenticate:
   * @consumer: the #EpcConsumer emitting the signal
   * @realm: the realm being authenticated to
   * @username: return location for the username (gchar*)
   * @password: return location for the password (gchar*)
   *
   * Emitted when the credentials provided by the application to the #
   * authenticate signal have failed. This gives the application a second
   * chance to provide authentication credentials. If the new credentials
   * also fail, #EpcConsumer will emit reauthenticate again, and will
   * continue doing so until the provided credentials work, or the signal
   * emission "fails" because the handler left @username and @password
   * unchanged.
   *
   * If your application only uses cached passwords, it should only connect
   * to authenticate, but not to reauthenticate.
   *
   * If your application always prompts the user for a password, and never
   * uses cached information, then you can connect the same handler to
   * authenticate and reauthenticate.
   *
   * To get common behaviour, return either cached information or user-provided
   * credentials (whichever is available) from the authenticate handler, but
   * return only user-provided information from the authenticate handler.
   *
   * The consumer takes ownership of the credential strings.
   */
  signals[SIGNAL_REAUTHENTICATE] = g_signal_new ("reauthenticate", EPC_TYPE_CONSUMER, G_SIGNAL_RUN_FIRST,
                                                 G_STRUCT_OFFSET (EpcConsumerClass, authenticate), NULL, NULL,
                                                 _epc_marshal_VOID__STRING_POINTER_POINTER, G_TYPE_NONE,
                                                 3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);

  g_type_class_add_private (cls, sizeof (EpcConsumerPrivate));
}

/**
 * epc_consumer_new:
 * @host: the host name of the publisher
 * @port: the TCP/IP port number of the publisher
 *
 * Creates a new #EpcConsumer object. To find host name and port
 * of a publisher service use DNS-SD (also known as ZeroConf) to
 * list #EPC_PUBLISHER_SERVICE_TYPE services. To lookup published
 * values, use #epc_consumer_lookup.
 *
 * Returns: The newly created #EpcConsumer object
 */
EpcConsumer*
epc_consumer_new (const gchar *host,
                  guint16      port)
{
  g_return_val_if_fail (NULL != host, NULL);
  g_return_val_if_fail (port > 0, NULL);

  return g_object_new (EPC_TYPE_CONSUMER, "host", host, "port", port, NULL);
}

EpcConsumer*
epc_consumer_new_for_name (const gchar *name)
{
  g_return_val_if_fail (NULL != name, NULL);

  return g_object_new (EPC_TYPE_CONSUMER, "name", name, NULL);
}

EpcConsumer*
epc_consumer_new_for_name_full (const gchar *name,
                                const gchar *service,
                                const gchar *domain)
{
  g_return_val_if_fail (NULL != name, NULL);

  return g_object_new (EPC_TYPE_CONSUMER, "name", name,
		       "service", service, "domain", domain, NULL);
}

gboolean
epc_consumer_wait_cb (gpointer data)
{
  EpcConsumer *self = data;

  g_warning ("%s: Timeout reached when waiting for publisher", G_STRLOC);
  g_main_loop_quit (self->priv->loop);

  return FALSE;
}

static void
epc_consumer_wait (EpcConsumer *self)
{
  g_timeout_add_seconds (5, epc_consumer_wait_cb, self);
  g_main_loop_run (self->priv->loop);
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

  if (NULL == self->priv->host)
    epc_consumer_wait (self);

  g_return_val_if_fail (NULL != self->priv->host, NULL);
  g_return_val_if_fail (self->priv->port > 0, NULL);

  request_uri = g_strdup_printf ("http://%s:%d/%s", self->priv->host,
                                 self->priv->port, path + 1);

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: connecting to `%s'", G_STRLOC, request_uri);

  request = soup_message_new ("GET", request_uri);
  g_free (request_uri);

  return request;
}

/**
 * epc_consumer_lookup:
 * @consumer: the consumer
 * @key: unique key of the value
 * @length: location to store length in bytes of the contents, or %NULL
 *
 * Looks up information at the publisher the consumer is associated with.
 * %NULL is returned when the publisher cannot be contacted or doesn't
 * contain information for the requested key.
 *
 * Returns: The contents of the requested value,
 * or %NULL when the key cannot be resolved
 */
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

  g_return_val_if_fail (NULL != request, NULL);

  epc_shell_leave ();
  status = soup_session_send_message (self->priv->session, request);
  epc_shell_enter ();

  if (SOUP_STATUS_OK == status)
    {
      if (length)
        *length = request->response.length;

      contents = g_malloc (request->response.length + 1);
      contents[request->response.length] = '\0';

      memcpy (contents, request->response.body,
              request->response.length);
    }

  g_object_unref (request);
  return contents;
}

/* vim: set sw=2 sta et spl=en spell: */
