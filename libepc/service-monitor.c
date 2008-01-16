/* Easy Publish and Consume Library
 * Copyright (C) 2007, 2008  Openismus GmbH
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

#include "service-monitor.h"

#include "marshal.h"
#include "service-type.h"
#include "shell.h"

#include <avahi-common/error.h>
#include <net/if.h>

/**
 * SECTION:service-monitor
 * @short_description: find DNS-SD services
 * @include: libepc/service-monitor.h
 * @stability: Unstable
 *
 * The #EpcServiceMonitor object provides an easy method for finding DNS-SD
 * services. It hides all the boring state and callback handling Avahi
 * requires, and just exposes three simple GObject signals:
 * #EpcServiceMonitor:service-found, #EpcServiceMonitor:service-removed
 * and #EpcServiceMonitor:scanning-done.
 *
 * <example id="find-services">
 *  <title>Find an Easy-Publish service</title>
 *  <programlisting>
 *   monitor = epc_service_monitor_new ("glom", NULL, EPC_PROTOCOL_UNKNOWN);
 *
 *   g_signal_connect (monitor, "service-found", service_found_cb, self);
 *   g_signal_connect (monitor, "service-removed", service_removed_cb, self);
 *
 *   g_main_loop_run (loop);
 *  </programlisting>
 * </example>
 */

enum
{
  PROP_NONE,
  PROP_SERVICE_TYPES,
  PROP_APPLICATION,
  PROP_DOMAIN,
  PROP_SKIP_OUR_OWN
};

enum
{
  SIGNAL_SERVICE_FOUND,
  SIGNAL_SERVICE_REMOVED,
  SIGNAL_SCANNING_DONE,
  SIGNAL_LAST
};

/**
 * EpcServiceMonitorPrivate:
 *
 * Private fields of the #EpcServiceMonitor class.
 */
struct _EpcServiceMonitorPrivate
{
  GSList  *browsers;
  gchar   *application;
  gchar   *domain;
  gchar  **types;
  gboolean skip_our_own;
};

static guint signals[SIGNAL_LAST];

G_DEFINE_TYPE (EpcServiceMonitor, epc_service_monitor, G_TYPE_OBJECT);

static void
epc_service_monitor_init (EpcServiceMonitor *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPC_TYPE_SERVICE_MONITOR,
                                            EpcServiceMonitorPrivate);
}

static void
epc_service_monitor_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (object);

  switch (prop_id)
    {
      case PROP_SERVICE_TYPES:
        g_assert (NULL == self->priv->browsers);
        self->priv->types = g_value_dup_boxed (value);
        break;

      case PROP_APPLICATION:
        g_assert (NULL == self->priv->browsers);
        self->priv->application = g_value_dup_string (value);
        break;

      case PROP_DOMAIN:
        g_assert (NULL == self->priv->browsers);
        self->priv->domain = g_value_dup_string (value);
        break;

      case PROP_SKIP_OUR_OWN:
        self->priv->skip_our_own = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_service_monitor_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (object);

  switch (prop_id)
    {
      case PROP_SERVICE_TYPES:
        g_value_set_boxed (value, self->priv->types);
        break;

      case PROP_APPLICATION:
        g_value_set_string (value, self->priv->application);
        break;

      case PROP_DOMAIN:
        g_value_set_string (value, self->priv->domain);
        break;

      case PROP_SKIP_OUR_OWN:
        g_value_set_boolean (value, self->priv->skip_our_own);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_service_monitor_resolver_cb (AvahiServiceResolver   *resolver,
                                 AvahiIfIndex            ifindex,
                                 AvahiProtocol           protocol,
                                 AvahiResolverEvent      event,
                                 const char             *name,
                                 const char             *type,
                                 const char             *domain G_GNUC_UNUSED,
                                 const char             *hostname,
                                 const AvahiAddress     *address,
                                 uint16_t                port,
                                 AvahiStringList        *txt,
                                 AvahiLookupResultFlags  flags G_GNUC_UNUSED,
                                 void                   *data)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (data);
  gchar ifname[IFNAMSIZ];
  EpcServiceInfo *info;
  gint error;

  switch (event)
    {
      case AVAHI_RESOLVER_FOUND:
        if (EPC_DEBUG_LEVEL (1))
          g_debug ("%s: Service resolved: type='%s', hostname='%s', port=%d, protocol=%s",
                   G_STRLOC, type, hostname, port, avahi_proto_to_string (protocol));

        g_assert (protocol == address->proto);

        info = epc_service_info_new_full (type, hostname, port, txt, address,
                                          if_indextoname (ifindex, ifname));
        g_signal_emit (self, signals[SIGNAL_SERVICE_FOUND], 0, name, info);
        epc_service_info_unref (info);
        break;

      case AVAHI_RESOLVER_FAILURE:
        error = avahi_client_errno (avahi_service_resolver_get_client (resolver));
        g_warning ("%s: %s (%d)", G_STRFUNC, avahi_strerror (error), error);
        break;
    }

  avahi_service_resolver_free (resolver);
}

static void
epc_service_monitor_browser_cb (AvahiServiceBrowser    *browser,
                                AvahiIfIndex            interface,
                                AvahiProtocol           protocol,
                                AvahiBrowserEvent       event,
                                const char             *name,
                                const char             *type,
                                const char             *domain,
                                AvahiLookupResultFlags  flags,
                                void                   *data)
{
  AvahiClient *client = avahi_service_browser_get_client (browser);
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (data);
  gint error;

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: event=%d, name=`%s', type=`%s', domain=`%s', our-own=%d",
             G_STRLOC, event, name, type, domain,
             flags & AVAHI_LOOKUP_RESULT_OUR_OWN);

  switch (event)
    {
      case AVAHI_BROWSER_NEW:
        if (!self->priv->skip_our_own || !(flags & AVAHI_LOOKUP_RESULT_OUR_OWN))
          avahi_service_resolver_new (client, interface, protocol, name, type, domain,
                                      protocol, 0, epc_service_monitor_resolver_cb, self);

        break;

      case AVAHI_BROWSER_REMOVE:
        g_signal_emit (self, signals[SIGNAL_SERVICE_REMOVED], 0, name, type);
        break;

      case AVAHI_BROWSER_CACHE_EXHAUSTED:
        break;

      case AVAHI_BROWSER_ALL_FOR_NOW:
        g_signal_emit (self, signals[SIGNAL_SCANNING_DONE], 0, type);
        break;

      case AVAHI_BROWSER_FAILURE:
        error = avahi_client_errno (client);

        g_warning ("%s: %s (%d)", G_STRFUNC,
                   avahi_strerror (error), error);

        break;
    }
}

static void
epc_service_monitor_constructed (GObject *object)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (object);
  gchar **service_types = self->priv->types;
  gchar **iter;

  if (G_OBJECT_CLASS (epc_service_monitor_parent_class)->constructed)
    G_OBJECT_CLASS (epc_service_monitor_parent_class)->constructed (object);

  if (NULL == service_types || NULL == *service_types)
    service_types = epc_service_type_list_supported (self->priv->application);

  for (iter = service_types; *iter; ++iter)
    {
      AvahiServiceBrowser *browser;
      GError *error = NULL;

      browser = epc_shell_create_service_browser (AVAHI_IF_UNSPEC,
                                                  AVAHI_PROTO_UNSPEC,
                                                  *iter, self->priv->domain, 0,
                                                  epc_service_monitor_browser_cb,
                                                  self, &error);

      if (error)
        {
          g_warning ("%s: Cannot create service type browser for '%s': %s (%d)",
                     G_STRFUNC, *iter, error->message, error->code);
          g_clear_error (&error);

          continue;
      }

      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: watching %s", G_STRLOC, *iter);

      self->priv->browsers = g_slist_prepend (self->priv->browsers, browser);
    }

  if (service_types != self->priv->types)
    g_strfreev (service_types);
}

static void
epc_service_monitor_dispose (GObject *object)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (object);

  while (self->priv->browsers)
    {
      avahi_service_browser_free (self->priv->browsers->data);
      self->priv->browsers = g_slist_delete_link (self->priv->browsers, self->priv->browsers);
    }

  if (self->priv->types)
    {
      g_strfreev (self->priv->types);
      self->priv->types = NULL;
    }

  if (self->priv->domain)
    {
      g_free (self->priv->domain);
      self->priv->domain = NULL;
    }

  if (self->priv->application)
    {
      g_free (self->priv->application);
      self->priv->application = NULL;
    }

  G_OBJECT_CLASS (epc_service_monitor_parent_class)->dispose (object);
}

static void
epc_service_monitor_class_init (EpcServiceMonitorClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_service_monitor_set_property;
  oclass->get_property = epc_service_monitor_get_property;
  oclass->constructed = epc_service_monitor_constructed;
  oclass->dispose = epc_service_monitor_dispose;

  g_object_class_install_property (oclass, PROP_DOMAIN,
                                   g_param_spec_string ("domain", "Domain",
                                                        "The DNS domain to monitor",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_APPLICATION,
                                   g_param_spec_string ("application", "Application",
                                                        "The application to monitor",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_SERVICE_TYPES,
                                   g_param_spec_boxed ("service-types", "Service Types",
                                                       "The DNS-SD services types to watch",
                                                      G_TYPE_STRV,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_SKIP_OUR_OWN,
                                   g_param_spec_boolean ("skip-our-own", "Skip our Own",
                                                         "Skip services of the current application",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  /**
   * EpcServiceMonitor::service-found:
   * @monitor: a #EpcServiceMonitor
   * @name: name of the service removed
   * @info: a description of the service found
   *
   * This signal is emitted when a new service was found.
   */
  signals[SIGNAL_SERVICE_FOUND] = g_signal_new ("service-found",
                                                EPC_TYPE_SERVICE_MONITOR, G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET (EpcServiceMonitorClass, service_found),
                                                NULL, NULL, _epc_marshal_VOID__STRING_BOXED,
                                                G_TYPE_NONE, 2, G_TYPE_STRING, EPC_TYPE_SERVICE_INFO);

  /**
   * EpcServiceMonitor::service-removed:
   * @monitor: a #EpcServiceMonitor
   * @name: name of the service removed
   * @type: the service type watched
   *
   * This signal is emitted when a previously known service disappears.
   */
  signals[SIGNAL_SERVICE_REMOVED] = g_signal_new ("service-removed",
                                                  EPC_TYPE_SERVICE_MONITOR, G_SIGNAL_RUN_FIRST,
                                                  G_STRUCT_OFFSET (EpcServiceMonitorClass, service_removed),
                                                  NULL, NULL, _epc_marshal_VOID__STRING_STRING,
                                                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

  /**
   * EpcServiceMonitor::scanning-done:
   * @monitor: a #EpcServiceMonitor
   * @type: the service type watched
   *
   * This signal is emitted when active scanning as finished and most certainly
   * no new services will be detected for some time. Can be used for instance
   * to hide an progress indicator.
   */
  signals[SIGNAL_SCANNING_DONE] = g_signal_new ("scanning-done",
                                                EPC_TYPE_SERVICE_MONITOR, G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET (EpcServiceMonitorClass, scanning_done),
                                                NULL, NULL, g_cclosure_marshal_VOID__STRING,
                                                G_TYPE_NONE, 1, G_TYPE_STRING);

  g_type_class_add_private (cls, sizeof (EpcServiceMonitorPrivate));
}

/**
 * epc_service_monitor_new_for_types_strv:
 * @domain: the DNS domain to monitor, or %NULL
 * @types: a %NULL terminated list of service types to monitor
 *
 * Creates a new service monitor watching the specified @domain for the
 * service-types listed. Passing %NULL for @domain monitors the local network.
 * Passing an empty service list requests monitoring of all service-types
 * supported by the library (see epc_service_type_list_supported()).
 *
 * See also: epc_service_monitor_new_for_types()
 *
 * Returns: The newly created service monitor.
 */
EpcServiceMonitor*
epc_service_monitor_new_for_types_strv (const gchar  *domain,
                                        gchar       **types)
{
  g_return_val_if_fail (NULL != types, NULL);

  return g_object_new (EPC_TYPE_SERVICE_MONITOR,
                       "service-types", types,
                       "domain", domain,
                       NULL);
}

/**
 * epc_service_monitor_new_for_types:
 * @domain: the DNS domain to monitor, or %NULL
 * @...: a %NULL terminated list of service types to monitor
 *
 * Creates a new service monitor watching the specified @domain for the
 * service-types listed. Passing %NULL for @domain monitors the local network.
 * Passing an empty service list requests monitoring of all service-types
 * supported by the library (see epc_service_type_list_supported()).
 *
 * See also: epc_service_monitor_new_for_types_strv()
 *
 * Returns: The newly created service monitor.
 * Since: 0.3.1
 */
EpcServiceMonitor*
epc_service_monitor_new_for_types (const gchar *domain,
                                                ...)
{
  EpcServiceMonitor *self = NULL;
  gchar **types = NULL;
  va_list args;
  gint i;

  for (i = 0; i < 2; ++i)
    {
      const gchar *type;
      gint tail = 0;

      va_start (args, domain);

      while (NULL != (type = va_arg (args, const gchar*)))
        {
          if (types)
            types[tail] = g_strdup (type);

          tail += 1;
        }

      va_end (args);

      if (NULL == types)
        types = g_new0 (gchar*, tail + 1);
    }

  self = g_object_new (EPC_TYPE_SERVICE_MONITOR,
                       "service-types", types,
                       "domain", domain, NULL);

  g_strfreev (types);
  return self;
}

/**
 * epc_service_monitor_new:
 * @application: name of the application to monitor, or %NULL
 * @domain: the DNS domain to monitor, or %NULL
 * @first_protocol: the first protocol to monitor
 * @...: a list of additional protocols, terminated by #EPC_PROTOCOL_UNKNOWN
 *
 * Creates a new service monitor watching the specified @domain for a
 * certain @application using one of the protocols listed. Passing %NULL for
 * @application lists all libepc based applications. Passing %NULL for @domain
 * monitors the local network. Passing an empty protocol list requests
 * monitoring of all service-types supported by the library (see
 * epc_service_type_list_supported()).
 *
 * Returns: The newly created service monitor.
 */
EpcServiceMonitor*
epc_service_monitor_new (const gchar *application,
                         const gchar *domain,
                         EpcProtocol  first_protocol,
                                      ...)
{
  EpcServiceMonitor* self = NULL;
  gchar **types = NULL;
  va_list args;
  gint i;

  for (i = 0; i < 2; ++i)
    {
      EpcProtocol protocol = first_protocol;
      gint tail = 0;

      va_start (args, first_protocol);

      while (((gint) protocol) > EPC_PROTOCOL_UNKNOWN)
        {
          if (types)
            types[tail] = epc_service_type_new (protocol, application);

          protocol = va_arg (args, EpcProtocol);
          tail += 1;
        }

      va_end (args);

      if (NULL == types)
        types = g_new0 (gchar*, tail + 1);
    }

  self = g_object_new (EPC_TYPE_SERVICE_MONITOR,
                       "application", application,
                       "service-types", types,
                       "domain", domain,
                       NULL);

  g_strfreev (types);
  return self;
}

/**
 * epc_service_monitor_set_skip_our_own:
 * @monitor: a #EpcServiceMonitor
 * @setting: the new setting
 *
 * Updates the #EpcServiceMonitor::skip-our-own property.
 */
void
epc_service_monitor_set_skip_our_own (EpcServiceMonitor *self,
                                      gboolean           setting)
{
  g_return_if_fail (EPC_IS_SERVICE_MONITOR (self));
  g_object_set (self, "skip-our-own", setting, NULL);
}

/**
 * epc_service_monitor_get_skip_our_own:
 * @monitor: a #EpcServiceMonitor
 *
 * Queries the current value of the #EpcServiceMonitor::skip-our-own property.
 *
 * Returns: The current value of the #EpcServiceMonitor::skip-our-own property
 */
gboolean
epc_service_monitor_get_skip_our_own (EpcServiceMonitor *self)
{
  g_return_val_if_fail (EPC_IS_SERVICE_MONITOR (self), FALSE);
  return self->priv->skip_our_own;
}
