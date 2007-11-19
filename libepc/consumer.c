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

#include "consumer.h"
#include "enums.h"
#include "marshal.h"
#include "shell.h"

#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup-session-async.h>
#include <string.h>

/**
 * SECTION:consumer
 * @short_description: lookup published values
 * @see_also: #EpcPublisher
 * @include: libepc/consumer.h
 * @stability: Unstable
 *
 * The #EpcConsumer object is used to lookup values published by an
 * #EpcPublisher service. Currently HTTP is used for communication.
 * To find a publisher, use DNS-SD (also known as ZeroConf) to
 * list #EPC_PUBLISHER_SERVICE_TYPE services.
 *
 * <example id="lookup-value">
 *  <title>Lookup a value</title>
 *  <programlisting>
 *   service_name = choose_recently_used_service ();
 *
 *   if (service_name)
 *     consumer = epc_consumer_new_for_name (service_name);
 *   else if (your_app_discover_server (&protocol, &hostname, &port))
 *     consumer = epc_consumer_new (protocol, hostname, port);
 *
 *   value = epc_consumer_lookup (consumer, "glom-settings", NULL, &error);
 *   g_object_unref (consumer);
 *
 *   your_app_consume_value (value);
 *   g_free (value);
 *  </programlisting>
 * </example>
 *
 * <example id="find-publisher">
 *  <title>Find a publisher</title>
 *  <programlisting>
 *   dialog = aui_service_dialog_new ("Choose a Service", main_window,
 *                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                    GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT,
 *                                    NULL);
 *
 *   aui_service_dialog_set_browse_service_types (AUI_SERVICE_DIALOG (dialog),
 *                                                EPC_SERVICE_TYPE_HTTPS,
 *                                                EPC_SERVICE_TYPE_HTTP,
 *                                                NULL);
 *
 *  aui_service_dialog_set_service_type_name (AUI_SERVICE_DIALOG (dialog),
 *                                            EPC_SERVICE_TYPE_HTTPS,
 *                                            "Secure Transport");
 *  aui_service_dialog_set_service_type_name (AUI_SERVICE_DIALOG (dialog),
 *                                            EPC_SERVICE_TYPE_HTTP,
 *                                            "Insecure Transport");
 *
 *  if (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
 *   {
 *      const gchar *transport = aui_service_dialog_get_service_type (AUI_SERVICE_DIALOG (dialog));
 *      const gchar *hostname = aui_service_dialog_get_host_name (AUI_SERVICE_DIALOG (dialog));
 *      const gint port = aui_service_dialog_get_port (AUI_SERVICE_DIALOG (dialog));
 *
 *      consumer = epc_consumer_new (epc_service_type_get_protocol (transport), hostname, port);
 *      ...
 *   }
 *  </programlisting>
 * </example>
 */

typedef struct _EpcListingState EpcListingState;

typedef enum
{
  EPC_LISTING_ELEMENT_NONE,
  EPC_LISTING_ELEMENT_LIST,
  EPC_LISTING_ELEMENT_ITEM,
  EPC_LISTING_ELEMENT_NAME
}
EpcListingElementType;

enum
{
  PROP_NONE,
  PROP_APPLICATION,
  PROP_PROTOCOL,
  PROP_HOSTNAME,
  PROP_PORT,
  PROP_NAME,
  PROP_DOMAIN
};

enum
{
  SIGNAL_AUTHENTICATE,
  SIGNAL_REAUTHENTICATE,
  SIGNAL_PUBLISHER_RESOLVED,
  SIGNAL_LAST
};

/**
 * EpcConsumerPrivate:
 *
 * Private fields of the #EpcConsumer class.
 */
struct _EpcConsumerPrivate
{
  AvahiClient  *client;
  GSList       *browsers;
  SoupSession  *session;
  GMainLoop    *loop;

  gchar        *application;
  EpcProtocol   protocol;
  gchar        *hostname;
  guint16       port;

  gchar        *service_name;
  gchar        *service_domain;
};

struct _EpcListingState
{
  EpcListingElementType element;
  GString              *name;
  GList                *items;
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

  epc_shell_enter ();
  g_signal_emit (self, signals[SIGNAL_AUTHENTICATE], 0,
                 auth_realm, username, password);
  epc_shell_leave ();
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

  epc_shell_enter ();
  g_signal_emit (self, signals[SIGNAL_REAUTHENTICATE], 0,
                 auth_realm, username, password);
  epc_shell_leave ();
}

static void
epc_consumer_init (EpcConsumer *self)
{
  epc_shell_ref ();

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_CONSUMER, EpcConsumerPrivate);
  self->priv->loop = g_main_loop_new (NULL, FALSE);
  self->priv->session = soup_session_async_new ();
  self->priv->protocol = EPC_PROTOCOL_HTTPS;

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
      case PROP_APPLICATION:
        g_assert (NULL == self->priv->application);
        self->priv->application = g_value_dup_string (value);
        break;

      case PROP_PROTOCOL:
        self->priv->protocol = g_value_get_enum (value);
        break;

      case PROP_HOSTNAME:
        g_assert (NULL == self->priv->hostname);
        self->priv->hostname = g_value_dup_string (value);
        break;

      case PROP_PORT:
        g_assert (0 == self->priv->port);
        self->priv->port = g_value_get_int (value);
        break;

      case PROP_NAME:
        g_assert (NULL == self->priv->service_name);
        self->priv->service_name = g_value_dup_string (value);
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
      case PROP_APPLICATION:
        g_value_set_string (value, self->priv->application);
        break;

      case PROP_PROTOCOL:
        g_value_set_enum (value, self->priv->protocol);
        break;

      case PROP_HOSTNAME:
        g_value_set_string (value, self->priv->hostname);
        break;

      case PROP_PORT:
        g_value_set_int (value, self->priv->port);
        break;

      case PROP_NAME:
        g_value_set_string (value, self->priv->service_name);
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
                                  const char             *type,
                                  const char             *domain G_GNUC_UNUSED,
                                  const char             *hostname,
                                  const AvahiAddress     *a G_GNUC_UNUSED,
                                  uint16_t                port,
                                  AvahiStringList        *txt G_GNUC_UNUSED,
                                  AvahiLookupResultFlags  flags G_GNUC_UNUSED,
                                  void                   *data G_GNUC_UNUSED)
{
  EpcConsumer *self = data;
  EpcProtocol transport;

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: Service resolved: type='%s', hostname='%s', port=%d",
             G_STRLOC, type, hostname, port);

  g_assert (EPC_PROTOCOL_HTTP > EPC_PROTOCOL_UNKNOWN);
  g_assert (EPC_PROTOCOL_HTTPS > EPC_PROTOCOL_HTTP);

  transport = epc_service_type_get_protocol (type);

  if (transport > self->priv->protocol)
    {
      if (G_UNLIKELY (_epc_debug))
        g_debug ("%s: Upgrading to '%s' protocol", G_STRLOC,
                 epc_protocol_get_service_type (transport));

      g_signal_emit (self, signals[SIGNAL_PUBLISHER_RESOLVED],
                     0, transport, hostname, (guint)port);

      self->priv->protocol = transport;
    }

  g_main_loop_quit (self->priv->loop);

  g_free (self->priv->hostname);
  self->priv->hostname = g_strdup (hostname);
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
        g_warning ("%s: %s (%d)", G_STRFUNC, avahi_strerror (error), error);
        break;

      default:
        if (name && g_str_equal (name, self->priv->service_name))
          avahi_service_resolver_new (self->priv->client, interface,
                                      protocol, name, type, domain, AVAHI_PROTO_UNSPEC,
                                      0, epc_consumer_service_resolver_cb, self);

        break;
    }
}

static void
epc_consumer_create_service_browsers (EpcConsumer *self,
                                      EpcProtocol  protocol,
                                                   ...)
{
  AvahiServiceBrowser *browser;
  va_list args;

  va_start (args, protocol);

  while (protocol > EPC_PROTOCOL_UNKNOWN)
    {
      gchar *service_type;

      service_type = epc_service_type_new (protocol, self->priv->application);
      protocol = va_arg (args, EpcProtocol);

      if (NULL == service_type)
        continue;

      browser = avahi_service_browser_new (self->priv->client,
                                           AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                                           service_type, self->priv->service_domain,
                                           0, epc_consumer_service_brower_cb, self);

      if (NULL == browser)
        {
          gint error = avahi_client_errno (self->priv->client);

          g_warning ("%s: Cannot create browser for service type '%s': %s (%d)",
                     G_STRFUNC, service_type, avahi_strerror (error), error);

          continue;
      }

      self->priv->browsers = g_slist_prepend (self->priv->browsers, browser);
    }

  va_end (args);
}

static void
epc_consumer_constructed (GObject *object)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  if (G_OBJECT_CLASS (epc_consumer_parent_class)->constructed)
    G_OBJECT_CLASS (epc_consumer_parent_class)->constructed (object);

  if (!self->priv->hostname)
    {
      GError *error = NULL;

      self->priv->client = epc_shell_create_avahi_client (AVAHI_CLIENT_NO_FAIL,
                                                          NULL, NULL, &error);

      if (NULL == self->priv->client)
        {
          g_warning ("%s: %s", G_STRFUNC, error->message);
          g_error_free (error);
          return;
        }

      if (EPC_PROTOCOL_UNKNOWN != self->priv->protocol)
        epc_consumer_create_service_browsers (self, self->priv->protocol, 0);
      else
        epc_consumer_create_service_browsers (self, EPC_PROTOCOL_HTTP, EPC_PROTOCOL_HTTPS, 0);
    }
}

static void
epc_consumer_dispose (GObject *object)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  while (self->priv->browsers)
    {
      avahi_service_browser_free (self->priv->browsers->data);
      self->priv->browsers = g_slist_delete_link (self->priv->browsers, self->priv->browsers);
    }

  if (self->priv->client)
    {
      avahi_client_free (self->priv->client);
      self->priv->client = NULL;
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

  g_free (self->priv->hostname);
  self->priv->hostname = NULL;

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

  g_object_class_install_property (oclass, PROP_APPLICATION,
                                   g_param_spec_string ("application", "Application",
                                                        "Program name the publisher to use", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_PROTOCOL,
                                   g_param_spec_enum ("protocol", "Protocol",
                                                      "The transport protocol to use for contacting the publisher",
                                                      EPC_TYPE_PROTOCOL, EPC_PROTOCOL_UNKNOWN,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_HOSTNAME,
                                   g_param_spec_string ("hostname", "Host Name",
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
   * Emitted when the #EpcConsumer requires authentication. The signal
   * handler should provide these credentials, which may come from the
   * user or from cached information. If no credentials
   * are available then the signal handler should leave @username and @password unchanged.
   *
   * If the provided credentials fail then the reauthenticate signal
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
   * If your application only uses cached passwords it should only connect
   * to authenticate but not to reauthenticate.
   *
   * If your application always prompts the user for a password, and never
   * uses cached information, then you can connect the same handler to
   * authenticate and reauthenticate.
   *
   * To get ideal behaviour return either cached information or user-provided
   * credentials (whichever are available) from the authenticate handler, but
   * return only user-provided information from the reauthenticate handler.
   *
   * The consumer takes ownership of the credential strings.
   */
  signals[SIGNAL_REAUTHENTICATE] = g_signal_new ("reauthenticate", EPC_TYPE_CONSUMER, G_SIGNAL_RUN_FIRST,
                                                 G_STRUCT_OFFSET (EpcConsumerClass, reauthenticate), NULL, NULL,
                                                 _epc_marshal_VOID__STRING_POINTER_POINTER, G_TYPE_NONE,
                                                 3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * EpcConsumer::publisher-resolved:
   * @consumer: the #EpcConsumer emitting the signal
   * @protocol: the publisher's transport protocol
   * @hostname: the publisher's host name
   * @port:  the publisher's TCP/IP port
   *
   * This signal is emitted when a #EpcConsumer created with
   * #epc_consumer_new_for_name or #epc_consumer_new_for_name_full
   * has found its #EpcPublisher.
   */
  signals[SIGNAL_PUBLISHER_RESOLVED] = g_signal_new ("publisher-resolved", EPC_TYPE_CONSUMER, G_SIGNAL_RUN_FIRST,
                                                     G_STRUCT_OFFSET (EpcConsumerClass, publisher_resolved), NULL, NULL,
                                                     _epc_marshal_VOID__ENUM_STRING_UINT, G_TYPE_NONE,
                                                     3, EPC_TYPE_PROTOCOL, G_TYPE_STRING, G_TYPE_UINT);

  g_type_class_add_private (cls, sizeof (EpcConsumerPrivate));
}

/**
 * epc_consumer_new:
 * @protocol: the transport protocol to use
 * @hostname: the publisher's host name
 * @port:  the publisher's TCP/IP port
 *
 * Creates a new #EpcConsumer object and associates it with a known
 * #EpcPublisher. Values for @protocol, @hostname and @port can be retrieved,
 * for instance, by using the service selection dialog of avahi-ui
 * (#AuiServiceDialog). Call #epc_service_type_get_protocol to convert
 * the service-type provided by that dialog to an #EpcProtocol value.
 *
 * The connection is not established until #epc_consumer_lookup
 * is called to retrieve values.
 *
 * Returns: The newly created #EpcConsumer object
 */
EpcConsumer*
epc_consumer_new (EpcProtocol  protocol,
                  const gchar *hostname,
                  guint16      port)
{
  g_return_val_if_fail (EPC_PROTOCOL_UNKNOWN != protocol, NULL);
  g_return_val_if_fail (NULL != hostname, NULL);
  g_return_val_if_fail (port > 0, NULL);

  return g_object_new (EPC_TYPE_CONSUMER, "protocol", protocol,
                       "hostname", hostname, "port", port, NULL);
}

/**
 * epc_consumer_new_for_name:
 * @name: the service name of an #EpcPublisher
 *
 * Creates a new #EpcConsumer object and associates it with the #EpcPublisher
 * announcing itself with @name on the local network. The DNS-SD service name
 * used for searching the #EpcPublisher is derived from the application's
 * program name as returned by #g_get_prgname.
 *
 * <note><para>
 *  The connection is not established until #epc_consumer_lookup is called
 *  to retrieve values. Use #epc_consumer_resolve_publisher or connect to the
 *  #EpcConsumer::publisher-resolved signal to wait until the #EpcPublisher
 *  has been found.
 * </para></note>
 *
 * For more control have a look at #epc_consumer_new_for_name_full.
 *
 * Returns: The newly created #EpcConsumer object
 */
EpcConsumer*
epc_consumer_new_for_name (const gchar *name)
{
  return epc_consumer_new_for_name_full (name, NULL, NULL);
}

/**
 * epc_consumer_new_for_name_full:
 * @name: the service name of an #EpcPublisher
 * @application: the publisher's program name
 * @domain: the DNS domain of the #EpcPublisher
 *
 * Creates a new #EpcConsumer object and associates it with the #EpcPublisher
 * announcing itself with @name on @domain. The DNS-SD service of the
 * #EpcPublisher is derived from @application using #epc_service_type_new.
 *
 * <note><para>
 *  The connection is not established until #epc_consumer_lookup is called
 *  to retrieve values. Use #epc_consumer_resolve_publisher or connect to the
 *  #EpcConsumer::publisher-resolved signal to wait until the #EpcPublisher
 *  has been found.
 * </para></note>
 *
 * Returns: The newly created #EpcConsumer object
 */
EpcConsumer*
epc_consumer_new_for_name_full (const gchar *name,
                                const gchar *application,
                                const gchar *domain)
{
  g_return_val_if_fail (NULL != name, NULL);

  return g_object_new (EPC_TYPE_CONSUMER,
                       "application", application,
                       "domain", domain,
                       "name", name, NULL);
}

/**
 * epc_consumer_set_protocol:
 * @consumer: a #EpcConsumer
 * @protocol: the new transport protocol
 *
 * Changes the transport protocol to use for contacting the publisher.
 * See #EpcConsumer:protocol.
 */
void
epc_consumer_set_protocol (EpcConsumer *self,
                           EpcProtocol  protocol)
{
  g_return_if_fail (EPC_IS_CONSUMER (self));
  g_object_set (self, "protocol", protocol, NULL);
}

/**
 * epc_consumer_get_protocol:
 * @consumer: a #EpcConsumer
 *
 * Queries the transport protocol to use for contacting the publisher.
 * See #EpcConsumer:protocol.
 *
 * Returns: The transport protocol this consumer uses.
 */
EpcProtocol
epc_consumer_get_protocol (EpcConsumer *self)
{
  g_return_val_if_fail (EPC_IS_CONSUMER (self), EPC_PROTOCOL_UNKNOWN);
  return self->priv->protocol;
}

static gboolean
epc_consumer_wait_cb (gpointer data)
{
  EpcConsumer *self = data;

  g_warning ("%s: Timeout reached when waiting for publisher", G_STRFUNC);
  g_main_loop_quit (self->priv->loop);

  return FALSE;
}

/**
 * epc_consumer_resolve_publisher:
 * @consumer: a #EpcConsumer
 * @timeout: the amount of milliseconds to wait
 *
 * Waits until the @consumer has found its #EpcPublisher.
 * A @timeout of 0 requests infinite waiting.
 *
 * See also: #EpcConsumer::publisher-resolved
 *
 * Returns: %TRUE when a publisher has been found, %FALSE otherwise.
 */
gboolean
epc_consumer_resolve_publisher (EpcConsumer *self,
                                guint        timeout)
{
  g_return_val_if_fail (EPC_IS_CONSUMER (self), FALSE);

  if (NULL == self->priv->hostname)
    {
      if (timeout > 0)
        g_timeout_add (timeout, epc_consumer_wait_cb, self);

      g_main_loop_run (self->priv->loop);
    }

  return (NULL != self->priv->hostname);
}

static SoupMessage*
epc_consumer_create_request (EpcConsumer *self,
                             const gchar *path)
{
  SoupMessage *request = NULL;

  if (NULL == path)
    path = "/";

  g_assert ('/' == path[0]);

  if (epc_consumer_resolve_publisher (self, EPC_CONSUMER_DEFAULT_TIMEOUT))
    {
      char *request_uri;

      g_return_val_if_fail (NULL != self->priv->hostname, NULL);
      g_return_val_if_fail (self->priv->port > 0, NULL);

      request_uri = epc_protocol_build_uri (self->priv->protocol,
                                            self->priv->hostname,
                                            self->priv->port,
                                            path);

      g_return_val_if_fail (NULL != request_uri, NULL);

      if (G_UNLIKELY (_epc_debug))
        g_debug ("%s: Connecting to `%s'", G_STRLOC, request_uri);

      request = soup_message_new ("GET", request_uri);
      g_free (request_uri);
    }

  return request;
}

static void
epc_consumer_set_http_error (GError     **error,
                             SoupMessage *request,
                             guint        status)
{
  const gchar *details = NULL;

  if (request)
    details = request->reason_phrase;
  if (!details)
    details = soup_status_get_phrase (status);

  g_set_error (error, EPC_HTTP_ERROR, status,
               "HTTP library error %d: %s.",
               status, details);
}

/**
 * epc_consumer_lookup:
 * @consumer: the consumer
 * @key: unique key of the value
 * @length: location to store length in bytes of the contents, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * If the call was successful, this returns a newly allocated buffer containing
 * the value the publisher provides for @key. If the call was not
 * successful it returns %NULL and sets @error. The error domain is
 * #EPC_HTTP_ERROR. Error codes are taken from the #SoupKnownStatusCode
 * enumeration.
 *
 * The returned buffer should be freed when no longer needed.
 *
 * Returns: A copy of the publisher's value for the the requested @key,
 * or %NULL when an error occurred.
 */
gchar*
epc_consumer_lookup (EpcConsumer  *self,
                     const gchar  *key,
                     gsize        *length,
                     GError      **error)
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

  if (request)
    {
      epc_shell_leave ();
      status = soup_session_send_message (self->priv->session, request);
      epc_shell_enter ();
    }
  else
    status = SOUP_STATUS_CANT_RESOLVE;

  if (SOUP_STATUS_IS_SUCCESSFUL (status))
    {
      if (length)
        *length = request->response.length;

      contents = g_malloc (request->response.length + 1);
      contents[request->response.length] = '\0';

      memcpy (contents,
              request->response.body,
              request->response.length);
    }
  else
    epc_consumer_set_http_error (error, request, status);

  g_object_unref (request);
  return contents;
}

static void
epc_consumer_list_parser_start_element (GMarkupParseContext *context G_GNUC_UNUSED,
                                        const gchar         *element_name,
                                        const gchar        **attribute_names G_GNUC_UNUSED,
                                        const gchar        **attribute_values G_GNUC_UNUSED,
                                        gpointer             data,
                                        GError             **error)
{
  EpcListingElementType element = EPC_LISTING_ELEMENT_NONE;
  EpcListingState *state = data;

  switch (state->element)
    {
      case EPC_LISTING_ELEMENT_NONE:
        if (g_str_equal (element_name, "list"))
          element = EPC_LISTING_ELEMENT_LIST;

        break;

      case EPC_LISTING_ELEMENT_LIST:
        if (g_str_equal (element_name, "item"))
          element = EPC_LISTING_ELEMENT_ITEM;

        break;

      case EPC_LISTING_ELEMENT_ITEM:
        if (g_str_equal (element_name, "name"))
          element = EPC_LISTING_ELEMENT_NAME;

        break;

      case EPC_LISTING_ELEMENT_NAME:
        break;
    }

  if (element)
    state->element = element;
  else
    g_set_error (error, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_INVALID_CONTENT,
                 _("Unexpected element: `%s'"),
                element_name);
}

static void
epc_consumer_list_parser_end_element (GMarkupParseContext *context G_GNUC_UNUSED,
                                      const gchar         *element_name G_GNUC_UNUSED,
                                      gpointer             data,
                                      GError             **error G_GNUC_UNUSED)
{
  EpcListingState *state = data;

  switch (state->element)
    {
      case EPC_LISTING_ELEMENT_NAME:
        state->element = EPC_LISTING_ELEMENT_ITEM;
        break;

      case EPC_LISTING_ELEMENT_ITEM:
        state->element = EPC_LISTING_ELEMENT_LIST;
        state->items = g_list_prepend (state->items, g_string_free (state->name, FALSE));
        state->name = NULL;
        break;

      case EPC_LISTING_ELEMENT_LIST:
        state->element = EPC_LISTING_ELEMENT_NONE;
        break;

      case EPC_LISTING_ELEMENT_NONE:
        break;
    }
}

static void
epc_consumer_list_parser_text (GMarkupParseContext *context G_GNUC_UNUSED,
                               const gchar         *text,
                               gsize                text_len,
                               gpointer             data,
                               GError             **error G_GNUC_UNUSED)
{
  EpcListingState *state = data;

  switch (state->element)
    {
      case EPC_LISTING_ELEMENT_NAME:
        if (!state->name)
          state->name = g_string_new (NULL);

        g_string_append_len (state->name, text, text_len);
        break;

      case EPC_LISTING_ELEMENT_ITEM:
      case EPC_LISTING_ELEMENT_LIST:
      case EPC_LISTING_ELEMENT_NONE:
        break;
    }
}

/**
 * epc_consumer_list:
 * @consumer: a #EpcConsumer
 * @pattern: a file globbing pattern
 * @error: return location for a #GError, or %NULL
 *
 * If the call was successful, a list of keys matching @pattern is returned.
 * If the call was not successful, it returns %NULL and sets @error. The error
 * domain is #EPC_HTTP_ERROR. Error codes are taken from the
 * #SoupKnownStatusCode enumeration.
 *
 * The returned list should be freed when no longer needed:
 *
 * <programlisting>
 *  g_list_foreach (keys, (GFunc) g_free, NULL);
 *  g_list_free (keys);
 * </programlisting>
 *
 * Returns: A newly allocated list of keys, or %NULL when an error occurred.
 */
GList*
epc_consumer_list (EpcConsumer  *self,
                   const gchar  *pattern G_GNUC_UNUSED,
                   GError      **error G_GNUC_UNUSED)
{
  EpcListingState state;
  SoupMessage *request = NULL;
  gchar *path = NULL;
  gint status = 0;

  g_return_val_if_fail (EPC_IS_CONSUMER (self), NULL);
  g_return_val_if_fail (NULL == pattern || *pattern, NULL);

  path = g_strconcat ("/list/", pattern, NULL);
  request = epc_consumer_create_request (self, path);
  g_free (path);

  if (request)
    {
      epc_shell_leave ();
      status = soup_session_send_message (self->priv->session, request);
      epc_shell_enter ();
    }
  else
    status = SOUP_STATUS_CANT_RESOLVE;

  memset (&state, 0, sizeof state);

  if (SOUP_STATUS_IS_SUCCESSFUL (status))
    {
      GMarkupParseContext *context;
      GMarkupParser parser;

      memset (&parser, 0, sizeof parser);

      parser.start_element = epc_consumer_list_parser_start_element;
      parser.end_element = epc_consumer_list_parser_end_element;
      parser.text = epc_consumer_list_parser_text;

      context = g_markup_parse_context_new (&parser,
                                            G_MARKUP_TREAT_CDATA_AS_TEXT,
                                            &state, NULL);

      g_markup_parse_context_parse (context,
                                    request->response.body,
                                    request->response.length,
                                    error);

      g_markup_parse_context_free (context);
    }
  else
    epc_consumer_set_http_error (error, request, status);

  return state.items;
}

GQuark
epc_http_error_quark (void)
{
  return g_quark_from_static_string ("epc-http-error-quark");
}

/* vim: set sw=2 sta et spl=en spell: */
