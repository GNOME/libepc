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
#include "service-monitor.h"
#include "shell.h"

#include <glib/gi18n-lib.h>
#include <libsoup/soup-session-async.h>
#include <libsoup/soup-uri.h>
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
 *   else
 *     consumer = epc_consumer_new (your_app_find_service ());
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
 *      EpcServiceInfo *service =
 *        epc_service_info_new (aui_service_dialog_get_service_type (AUI_SERVICE_DIALOG (dialog)),
 *                              aui_service_dialog_get_host_name    (AUI_SERVICE_DIALOG (dialog)),
 *                              aui_service_dialog_get_port         (AUI_SERVICE_DIALOG (dialog)),
 *                              aui_service_dialog_get_txt_data     (AUI_SERVICE_DIALOG (dialog)));
 *
 *      consumer = epc_consumer_new (service);
 *      epc_service_info_unref (service);
 *      ...
 *   }
 *  </programlisting>
 * </example>
 */

#define EPC_CONSUMER_DEFAULT_TIMEOUT 5000

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
  PROP_NAME,
  PROP_DOMAIN,
  PROP_APPLICATION,
  PROP_PROTOCOL,
  PROP_HOSTNAME,
  PROP_PORT,
  PROP_PATH,
  PROP_USERNAME,
  PROP_PASSWORD
};

enum
{
  SIGNAL_AUTHENTICATE,
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
  /* supportive objects */

  EpcServiceMonitor *service_monitor;
  SoupSession       *session;
  GMainLoop         *loop;

  /* search parameters */

  gchar       *application;
  EpcProtocol  protocol;

  /* service credentials */

  gchar       *username;
  gchar       *password;

  /* service description */

  gchar       *name;
  gchar       *domain;
  gchar       *hostname;
  gchar       *path;
  guint16      port;
};

struct _EpcListingState
{
  EpcListingElementType element;
  GString              *name;
  GList                *items;
};

static guint signals[SIGNAL_LAST];

G_DEFINE_TYPE (EpcConsumer, epc_consumer, G_TYPE_OBJECT);

static void
epc_consumer_authenticate_cb (SoupSession  *session G_GNUC_UNUSED,
                              SoupMessage  *message,
                              gchar        *auth_type G_GNUC_UNUSED,
                              gchar        *auth_realm,
                              gchar       **username,
                              gchar       **password,
                              gpointer      data)
{
  EpcConsumer *self = EPC_CONSUMER (data);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: path=%s, realm=%s, username=%s, password=%s",
             G_STRLOC, soup_message_get_uri (message)->path,
             auth_realm, *username, *password);

  g_free (*username);
  g_free (*password);

  *username = g_strdup (self->priv->username ? self->priv->username : "");
  *password = g_strdup (self->priv->password ? self->priv->password : "");

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: path=%s, realm=%s, username=%s, password=%s",
             G_STRLOC, soup_message_get_uri (message)->path,
             auth_realm, *username, *password);
}

static void
epc_consumer_reauthenticate_cb (SoupSession  *session,
                                SoupMessage  *message,
                                gchar        *auth_type,
                                gchar        *auth_realm,
                                gchar       **username,
                                gchar       **password,
                                gpointer      data)
{
  EpcConsumer *self = EPC_CONSUMER (data);
  gboolean handled = FALSE;

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: path=%s, realm=%s, username=%s, password=%s, handled=%d",
             G_STRLOC, soup_message_get_uri (message)->path,
             auth_realm, *username, *password, handled);

  g_signal_emit (self, signals[SIGNAL_AUTHENTICATE],
                 0, auth_realm, &handled);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: path=%s, realm=%s, username=%s, password=%s, handled=%d",
             G_STRLOC, soup_message_get_uri (message)->path,
             auth_realm, *username, *password, handled);

  if (handled)
    epc_consumer_authenticate_cb (session, message, auth_realm,
                                  auth_type, username, password, data);
}

static void
epc_consumer_init (EpcConsumer *self)
{
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
      case PROP_NAME:
        g_assert (NULL == self->priv->name);
        self->priv->name = g_value_dup_string (value);
        break;

      case PROP_DOMAIN:
        g_assert (NULL == self->priv->domain);
        self->priv->domain = g_value_dup_string (value);
        break;

      case PROP_APPLICATION:
        g_assert (NULL == self->priv->application);
        self->priv->application = g_value_dup_string (value);
        break;

      case PROP_PROTOCOL:
        g_return_if_fail (NULL == self->priv->service_monitor &&
                          NULL == self->priv->hostname);
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

      case PROP_PATH:
        g_assert (NULL == self->priv->path);
        self->priv->path = g_value_dup_string (value);
        break;

      case PROP_USERNAME:
        g_free (self->priv->username);
        self->priv->username = g_value_dup_string (value);
        break;

      case PROP_PASSWORD:
        g_free (self->priv->password);
        self->priv->password = g_value_dup_string (value);
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
      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;

      case PROP_DOMAIN:
        g_value_set_string (value, self->priv->domain);
        break;

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

      case PROP_PATH:
        g_value_set_string (value, self->priv->path);
        break;

      case PROP_USERNAME:
        g_value_set_string (value, self->priv->username);
        break;

      case PROP_PASSWORD:
        g_value_set_string (value, self->priv->password);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_consumer_service_found_cb (EpcConsumer    *self,
                               const gchar    *name,
                               EpcServiceInfo *info)
{
  const gchar *type = epc_service_info_get_service_type (info);
  EpcProtocol transport = epc_service_type_get_protocol (type);

  const gchar *path = epc_service_info_get_detail (info, "path");
  const gchar *host = epc_service_info_get_host (info);
  guint port = epc_service_info_get_port (info);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: Service resolved: type='%s', host='%s', port=%d, path='%s'", 
             G_STRLOC, type, host, port, path);

  if (name && strcmp (name, self->priv->name))
    return;

  g_assert (EPC_PROTOCOL_HTTPS > EPC_PROTOCOL_HTTP);

  if (transport > self->priv->protocol)
    {
      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: Upgrading to %s protocol", G_STRLOC, epc_protocol_get_service_type (transport));

      g_signal_emit (self, signals[SIGNAL_PUBLISHER_RESOLVED], 0, transport, host, port);
      self->priv->protocol = transport;
    }

  g_main_loop_quit (self->priv->loop);

  g_free (self->priv->path);
  g_free (self->priv->hostname);

  /* Use /get path as fallback for libepc-0.2 publishers */
  self->priv->path = g_strdup (path ? path : "/get");
  self->priv->hostname = g_strdup (host);
  self->priv->port = port;
}

static void
epc_consumer_constructed (GObject *object)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  if (G_OBJECT_CLASS (epc_consumer_parent_class)->constructed)
    G_OBJECT_CLASS (epc_consumer_parent_class)->constructed (object);

  if (!self->priv->hostname)
    {
      self->priv->service_monitor = epc_service_monitor_new (self->priv->application,
                                                             self->priv->domain,
                                                             self->priv->protocol,
                                                             EPC_PROTOCOL_UNKNOWN);

      g_signal_connect_swapped (self->priv->service_monitor, "service-found",
                                G_CALLBACK (epc_consumer_service_found_cb),
                                self);
    }
}

static void
epc_consumer_dispose (GObject *object)
{
  EpcConsumer *self = EPC_CONSUMER (object);

  if (self->priv->service_monitor)
    {
      g_object_unref (self->priv->service_monitor);
      self->priv->service_monitor = NULL;
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

  g_free (self->priv->name);
  self->priv->name = NULL;

  g_free (self->priv->domain);
  self->priv->domain = NULL;

  g_free (self->priv->application);
  self->priv->application = NULL;

  g_free (self->priv->hostname);
  self->priv->hostname = NULL;

  g_free (self->priv->username);
  self->priv->username = NULL;

  g_free (self->priv->password);
  self->priv->password = NULL;

  g_free (self->priv->path);
  self->priv->path = NULL;

  G_OBJECT_CLASS (epc_consumer_parent_class)->dispose (object);
}

static void
epc_consumer_class_init (EpcConsumerClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_consumer_set_property;
  oclass->get_property = epc_consumer_get_property;
  oclass->constructed = epc_consumer_constructed;
  oclass->dispose = epc_consumer_dispose;

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

  g_object_class_install_property (oclass, PROP_PATH,
                                   g_param_spec_string ("path", "Path",
                                                        "The path the publisher uses for contents",
                                                        "/contents",
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_USERNAME,
                                   g_param_spec_string ("username", "User Name",
                                                        "The user name to use for authentication",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_PASSWORD,
                                   g_param_spec_string ("password", "Password",
                                                        "The password to use for authentication",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EpcConsumer::authenticate:
   * @consumer: the #EpcConsumer emitting the signal
   * @realm: the realm being authenticated to
   *
   * Emitted when the #EpcConsumer requires authentication. The signal
   * handler should provide these credentials, which may come from the
   * user or from cached information by setting the #EpcConsumer:username
   * and #EpcConsumer:password properties. When providing credentials
   * the signal handler should return %TRUE to stop signal emission.
   *
   * If the provided credentials fail then the signal will be emmitted again.
   *
   * Returns: %TRUE when the signal handler handled the authentication request,
   * and %FALSE otherwise.
   */
  signals[SIGNAL_AUTHENTICATE] = g_signal_new ("authenticate",
                                               EPC_TYPE_CONSUMER, G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET (EpcConsumerClass, authenticate),
                                               g_signal_accumulator_true_handled, NULL,
                                               _epc_marshal_BOOLEAN__STRING,
                                               G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

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
   *
   * Publisher detection is integrated with the GLib main loop. Therefore the
   * signal will not be emitted before a main loop is run (#g_main_loop_run,
   * #gtk_main). So to reliably consume this signal connect to it directly
   * after creating the #EpcConsumer.
   *
   * See also: #epc_consumer_resolve_publisher, #epc_consumer_is_pulisher_resolved
   */
  signals[SIGNAL_PUBLISHER_RESOLVED] = g_signal_new ("publisher-resolved", EPC_TYPE_CONSUMER, G_SIGNAL_RUN_FIRST,
                                                     G_STRUCT_OFFSET (EpcConsumerClass, publisher_resolved), NULL, NULL,
                                                     _epc_marshal_VOID__ENUM_STRING_UINT, G_TYPE_NONE,
                                                     3, EPC_TYPE_PROTOCOL, G_TYPE_STRING, G_TYPE_UINT);

  g_type_class_add_private (cls, sizeof (EpcConsumerPrivate));
}

/**
 * epc_consumer_new:
 * @service: the publisher's service description
 *
 * Creates a new #EpcConsumer object and associates it with a known
 * #EpcPublisher. The @service description can be retrieved, for instance,
 * by using #EpcServiceMonitor, or by using the service selection dialog
 * of <citetitle>avahi-ui</citetitle> (#AuiServiceDialog).
 *
 * The connection is not established until functions like #epc_consumer_lookup,
 * #epc_consumer_list or #epc_consumer_resolve_publisher are called.
 *
 * Returns: The newly created #EpcConsumer object
 */
EpcConsumer*
epc_consumer_new (const EpcServiceInfo *service)
{
  EpcProtocol protocol;
  const gchar *type;

  g_return_val_if_fail (EPC_IS_SERVICE_INFO (service), NULL);

  type = epc_service_info_get_service_type (service);
  protocol = epc_service_type_get_protocol (type);

  g_return_val_if_fail (EPC_PROTOCOL_UNKNOWN != protocol, NULL);

  return g_object_new (EPC_TYPE_CONSUMER,
                       "protocol", protocol,
                       "hostname", epc_service_info_get_host (service),
                       "port", epc_service_info_get_port (service),
                       "path", epc_service_info_get_detail (service, "path"),
                       NULL);
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
 * See #epc_consumer_new_for_name_full for additional notes
 * and a method allowing better control over the search process.
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
 *  This function shall be used to re-connect to a formerly used #EpcPublisher,
 *  selected for instance from a list for recently used services. Therefore
 *  using #epc_consumer_new_for_name_full is a quite optimistic approach for
 *  contacting a publisher: You call it without really knowing if the
 *  publisher you requested really exists. You only know that it existed
 *  in the past when you added it to your list of recently used publishers,
 *  but you do not know if it still exists.
 *
 *  To let your users choose from an up-to-date service list, you have to
 *  use a dynamic service list as provided by avahi-ui for choosing a service
 *  and pass the information this widget provides (hostname, port, protocol)
 *  to #epc_consumer_new.
 * </para></note>
 *
 * <note><para>
 *  The connection is not established until a function retrieving
 *  data, like for instance #epc_consumer_lookup, is called.
 *
 *  Explicitly call #epc_consumer_resolve_publisher or connect to
 *  the #EpcConsumer::publisher-resolved signal, when your application
 *  needs reliable information about the existance of the #EpcPublisher
 *  described by @name.
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
 * See #EpcConsumer:protocol for details.
 */
void
epc_consumer_set_protocol (EpcConsumer *self,
                           EpcProtocol  protocol)
{
  g_return_if_fail (EPC_IS_CONSUMER (self));
  g_object_set (self, "protocol", protocol, NULL);
}

/**
 * epc_consumer_set_username:
 * @consumer: a #EpcConsumer
 * @username: the new user name, or %NULL
 *
 * Changes the user name used for authentication.
 * See #EpcConsumer:username for details.
 */
void
epc_consumer_set_username (EpcConsumer *self,
                           const gchar *username)
{
  g_return_if_fail (EPC_IS_CONSUMER (self));
  g_object_set (self, "username", username, NULL);
}

/**
 * epc_consumer_set_password:
 * @consumer: a #EpcConsumer
 * @password: the new password, or %NULL
 *
 * Changes the password used for authentication.
 * See #EpcConsumer:password for details.
 */
void
epc_consumer_set_password (EpcConsumer *self,
                           const gchar *password)
{
  g_return_if_fail (EPC_IS_CONSUMER (self));
  g_object_set (self, "password", password, NULL);
}

/**
 * epc_consumer_get_protocol:
 * @consumer: a #EpcConsumer
 *
 * Queries the transport protocol to use for contacting the publisher.
 * See #EpcConsumer:protocol for details.
 *
 * Returns: The transport protocol this consumer uses.
 */
EpcProtocol
epc_consumer_get_protocol (EpcConsumer *self)
{
  g_return_val_if_fail (EPC_IS_CONSUMER (self), EPC_PROTOCOL_UNKNOWN);
  return self->priv->protocol;
}

/**
 * epc_consumer_get_username:
 * @consumer: a #EpcConsumer
 *
 * Queries the user name used for authentication.
 * See #EpcConsumer:username for details.
 *
 * Returns: The user name this consumer uses.
 */
G_CONST_RETURN gchar*
epc_consumer_get_username (EpcConsumer *self)
{
  g_return_val_if_fail (EPC_IS_CONSUMER (self), NULL);
  return self->priv->username;
}

/**
 * epc_consumer_get_password:
 * @consumer: a #EpcConsumer
 *
 * Queries the password used for authentication.
 * See #EpcConsumer:password for details.
 *
 * Returns: The password this consumer uses.
 */
G_CONST_RETURN gchar*
epc_consumer_get_password (EpcConsumer *self)
{
  g_return_val_if_fail (EPC_IS_CONSUMER (self), NULL);
  return self->priv->password;
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

  return epc_consumer_is_publisher_resolved (self);
}

/**
 * epc_consumer_is_publisher_resolved:
 * @consumer: a #EpcConsumer
 *
 * Checks if the host name of this consumer's #EpcPublisher
 * has been resolved already.
 *
 * See also: #epc_consumer_resolve_publisher, #EpcPublisher::publisher-resolved
 *
 * Returns: %TRUE when the host name has been resolved, and %FALSE otherwise.
 */
gboolean
epc_consumer_is_publisher_resolved (EpcConsumer *self)
{
  g_return_val_if_fail (EPC_IS_CONSUMER (self), FALSE);
  return (NULL != self->priv->hostname);
}

static SoupMessage*
epc_consumer_create_request (EpcConsumer *self,
                             const gchar *path)
{
  SoupMessage *request = NULL;
  char *request_uri;

  if (NULL == path)
    path = "/";

  g_assert ('/' == path[0]);

  g_return_val_if_fail (NULL != self->priv->hostname, NULL);
  g_return_val_if_fail (self->priv->port > 0, NULL);

  request_uri = epc_protocol_build_uri (self->priv->protocol,
                                        self->priv->hostname,
                                        self->priv->port,
                                        path);

  g_return_val_if_fail (NULL != request_uri, NULL);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: Connecting to `%s'", G_STRLOC, request_uri);

  request = soup_message_new ("GET", request_uri);
  g_free (request_uri);

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
 * For instance, the error code will be #SOUP_STATUS_FORBIDDEN if
 * authentication failed (see #epc_publisher_set_auth_handler). You must
 * include <filename class="headerfile">libsoup/soup-status.h</filename>
 * to use this error code.
 *
 * The returned buffer should be freed when no longer needed.
 *
 * See the description of #EpcPublisher for discussion of %NULL values.
 *
 * Returns: A copy of the publisher's value for the the requested @key,
 * or %NULL when an error occurred.
 */
gpointer
epc_consumer_lookup (EpcConsumer  *self,
                     const gchar  *key,
                     gsize        *length,
                     GError      **error)
{
  SoupMessage *request = NULL;
  gchar *contents = NULL;
  gint status = 0;

  g_return_val_if_fail (EPC_IS_CONSUMER (self), NULL);
  g_return_val_if_fail (NULL != key, NULL);

  if (epc_consumer_resolve_publisher (self, EPC_CONSUMER_DEFAULT_TIMEOUT))
    {
      gchar *keyuri = NULL;
      gchar *path = NULL;

      keyuri = soup_uri_encode (key, NULL);
      path = g_strconcat (self->priv->path, "/", keyuri, NULL);
      request = epc_consumer_create_request (self, path);

      g_free (keyuri);
      g_free (path);
    }

  if (request)
    status = soup_session_send_message (self->priv->session, request);
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

  if (request)
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
 * @pattern: a glob-style pattern, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Matches published keys against patterns containing '*' (wildcard) and '?'
 * (joker). Passing %NULL as @pattern is equivalent to passing "*" and returns
 * all published keys. This function is useful to find and select dynamically
 * published values. See #GPatternSpec for information about glob-style
 * patterns.
 *
 * If the call was successful, a list of keys matching @pattern is returned.
 * If the call was not successful, it returns %NULL and sets @error.
 * The error domain is #EPC_HTTP_ERROR. Error codes are taken from the
 * #SoupKnownStatusCode enumeration.
 *
 * The returned list should be freed when no longer needed:
 *
 * <programlisting>
 *  g_list_foreach (keys, (GFunc) g_free, NULL);
 *  g_list_free (keys);
 * </programlisting>
 *
 * See also #epc_publisher_list for creating custom listings.
 *
 * Returns: A newly allocated list of keys, or %NULL when an error occurred.
 */
GList*
epc_consumer_list (EpcConsumer  *self,
                   const gchar  *pattern G_GNUC_UNUSED,
                   GError      **error G_GNUC_UNUSED)
{
  SoupMessage *request = NULL;
  EpcListingState state;
  gint status = 0;

  g_return_val_if_fail (EPC_IS_CONSUMER (self), NULL);
  g_return_val_if_fail (NULL == pattern || *pattern, NULL);

  if (epc_consumer_resolve_publisher (self, EPC_CONSUMER_DEFAULT_TIMEOUT))
    {
      gchar *path = g_strconcat ("/list/", pattern, NULL);
      request = epc_consumer_create_request (self, path);
      g_free (path);
    }

  if (request)
    status = soup_session_send_message (self->priv->session, request);
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
