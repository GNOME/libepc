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

#include "dispatcher.h"
#include "service-type.h"
#include "shell.h"

#include <avahi-common/alternative.h>
#include <avahi-common/error.h>

/**
 * SECTION:dispatcher
 * @short_description: publish DNS-SD services
 * @include: libepc/dispatcher.h
 * @stability: Unstable
 *
 * The #EpcDispatcher object provides an easy method for publishing
 * DNS-SD services. Unlike established APIs like Avahi or HOWL the
 * #EpcDispatcher doesn't expose any state changes reported by the
 * DNS-SD daemon, but instead tries to handle them automatically. Such state
 * changes include, for instance, name collisions or a restart of
 * the DNS-SD daemon.
 *
 * <example id="publish-printing-service">
 *  <title>Publish a printing service</title>
 *  <programlisting>
 *   dispatcher = epc_dispatcher_new ("Dead Tree Desecrator");
 *
 *   epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_IPV4, "_ipp._tcp",
 *                               NULL, NULL, 651, "path=/printers", NULL);
 *   epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_UNSPEC,
 *                               "_duplex._sub._printer._tcp",
 *                               NULL, NULL, 515, NULL);
 *  </programlisting>
 * </example>
 */

typedef struct _EpcService EpcService;

typedef void (*EpcServiceCallback) (EpcService *service);

enum
{
  PROP_NONE,
  PROP_NAME
};

struct _EpcService
{
  EpcDispatcher *dispatcher;
  AvahiEntryGroup *group;
  AvahiProtocol protocol;
  guint commit_handler;

  gchar *type;
  gchar *domain;
  gchar *host;
  guint16 port;

  GList *subtypes;
  AvahiStringList *details;
};

/**
 * EpcDispatcherPrivate:
 * @name: the service name
 * @client: the Avahi client
 * @services: all services announced with the service type as key
 *
 * Private fields of the #EpcDispatcher class.
 */
struct _EpcDispatcherPrivate
{
  gchar      *name;
  GHashTable *services;
  guint       watch_id;
};

static void epc_dispatcher_handle_collision (EpcDispatcher *self);
static void epc_service_run                 (EpcService    *self);

G_DEFINE_TYPE (EpcDispatcher, epc_dispatcher, G_TYPE_OBJECT);

static gboolean
epc_service_commit_cb (gpointer data)
{
  EpcService *self = data;

  self->commit_handler = 0;
  avahi_entry_group_commit (self->group);

  return FALSE;
}

static void
epc_service_schedule_commit (EpcService *self)
{
  if (!self->commit_handler)
    self->commit_handler = g_idle_add (epc_service_commit_cb, self);
}

static void
epc_service_publish_subtype (EpcService  *self,
                             const gchar *subtype)
{
  gint result;

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: Publishing sub-service `%s' for `%s'...",
             G_STRLOC, subtype, self->dispatcher->priv->name);

  result = avahi_entry_group_add_service_subtype (self->group,
                                                  AVAHI_IF_UNSPEC,
                                                  self->protocol, 0,
                                                  self->dispatcher->priv->name,
                                                  self->type, self->domain,
                                                  subtype);

  if (AVAHI_OK != result)
    g_warning ("%s: Failed to publish sub-service `%s' for `%s': %s (%d)",
               G_STRLOC, subtype, self->dispatcher->priv->name,
               avahi_strerror (result), result);

  epc_service_schedule_commit (self);
}

static void
epc_service_publish_details (EpcService *self)
{
  gint result;

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: Publishing details for `%s'...",
             G_STRLOC, self->dispatcher->priv->name);

  result = avahi_entry_group_update_service_txt_strlst (self->group,
                                                        AVAHI_IF_UNSPEC, self->protocol, 0,
                                                        self->dispatcher->priv->name,
                                                        self->type, self->domain,
                                                        self->details);

  if (AVAHI_OK != result)
    g_warning ("%s: Failed publish details for `%s': %s (%d)",
               G_STRLOC, self->dispatcher->priv->name,
               avahi_strerror (result), result);

  epc_service_schedule_commit (self);
}

static void
epc_service_publish (EpcService *self)
{
  if (self->group)
    {
      gint result;
      GList *iter;

      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: Publishing service `%s' for `%s'...",
                 G_STRLOC, self->type, self->dispatcher->priv->name);

      result = avahi_entry_group_add_service_strlst (self->group,
                                                     AVAHI_IF_UNSPEC, self->protocol, 0,
                                                     self->dispatcher->priv->name,
                                                     self->type, self->domain,
                                                     self->host, self->port,
                                                     self->details);

      if (AVAHI_ERR_COLLISION == result)
        epc_dispatcher_handle_collision (self->dispatcher);
      else if (AVAHI_OK != result)
        g_warning ("%s: Failed to publish service `%s' for `%s': %s (%d)",
                   G_STRLOC, self->type, self->dispatcher->priv->name,
                   avahi_strerror (result), result);
      else
        {
          for (iter = self->subtypes; iter; iter = iter->next)
            epc_service_publish_subtype (self, iter->data);

          epc_service_schedule_commit (self);
        }
    }
  else
    epc_service_run (self);
}

static void
epc_service_reset (EpcService *self)
{
  if (self->group)
    {
      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: Resetting `%s' for `%s'...",
                 G_STRLOC, self->type, self->dispatcher->priv->name);

      avahi_entry_group_reset (self->group);
    }
}

static void
epc_service_group_cb (AvahiEntryGroup      *group,
                      AvahiEntryGroupState  state,
                      gpointer              data)
{
  EpcService *self = data;
  GError *error = NULL;

  g_assert (NULL == self->group || group == self->group);

  switch (state)
    {
      case AVAHI_ENTRY_GROUP_REGISTERING:
      case AVAHI_ENTRY_GROUP_ESTABLISHED:
        break;

      case AVAHI_ENTRY_GROUP_UNCOMMITED:
        self->group = group;
        epc_service_publish (self);
        break;

      case AVAHI_ENTRY_GROUP_COLLISION:
        self->group = group;
        epc_dispatcher_handle_collision (self->dispatcher);
        break;

      case AVAHI_ENTRY_GROUP_FAILURE:
        {
          AvahiClient *client = avahi_entry_group_get_client (group);
          gint error_code = avahi_client_errno (client);

          g_warning ("%s: Failed to publish service records: %s.",
                     G_STRFUNC, avahi_strerror (error_code));

          epc_shell_restart_avahi_client (G_STRLOC);
          break;
        }
    }

  g_clear_error (&error);
}

static void
epc_service_run (EpcService *self)
{
  if (NULL == self->group)
    {
      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: Creating service `%s' group for `%s'...",
                 G_STRLOC, self->type, self->dispatcher->priv->name);

      self->group = epc_shell_create_avahi_entry_group (epc_service_group_cb, self);
    }
}

static void
epc_service_add_subtype (EpcService  *service,
                         const gchar *subtype)
{
  service->subtypes = g_list_prepend (service->subtypes, g_strdup (subtype));
}

static EpcService*
epc_service_new (EpcDispatcher *dispatcher,
                 AvahiProtocol  protocol,
                 const gchar   *type,
                 const gchar   *domain,
                 const gchar   *host,
                 guint16        port,
                 va_list        args)
{
  const gchar *service = epc_service_type_get_base (type);
  EpcService *self = g_slice_new0 (EpcService);

  self->dispatcher = dispatcher;
  self->details = avahi_string_list_new_va (args);
  self->type = g_strdup (service);
  self->protocol = protocol;
  self->port = port;

  if (domain)
    self->domain = g_strdup (domain);
  if (host)
    self->host = g_strdup (host);
  if (service > type)
    epc_service_add_subtype (self, type);

  return self;
}

static void
epc_service_suspend (EpcService *self)
{
  if (self->group)
    {
      avahi_entry_group_free (self->group);
      self->group = NULL;
    }
}

static void
epc_service_free (gpointer data)
{
  EpcService *self = data;

  epc_service_suspend (self);
  avahi_string_list_free (self->details);

  g_list_foreach (self->subtypes, (GFunc)g_free, NULL);
  g_list_free (self->subtypes);

  g_free (self->type);
  g_free (self->domain);
  g_free (self->host);

  if (self->commit_handler)
    g_source_remove (self->commit_handler);

  g_slice_free (EpcService, self);
}

static void
epc_dispatcher_services_cb (gpointer key G_GNUC_UNUSED,
                            gpointer value,
                            gpointer data)
{
  ((EpcServiceCallback) data) (value);
}

static void
epc_dispatcher_foreach_service (EpcDispatcher      *self,
                                EpcServiceCallback  callback)
{
  g_hash_table_foreach (self->priv->services, epc_dispatcher_services_cb, callback);
}

static void
epc_dispatcher_client_cb (AvahiClient      *client G_GNUC_UNUSED,
                          AvahiClientState  state,
                          gpointer          data)
{
  EpcDispatcher *self = data;
  GError *error = NULL;

  switch (state)
    {
      case AVAHI_CLIENT_S_RUNNING:
        if (EPC_DEBUG_LEVEL (1))
          g_debug ("%s: Avahi client is running...", G_STRLOC);

        epc_dispatcher_foreach_service (self, epc_service_publish);
        break;

      case AVAHI_CLIENT_S_REGISTERING:
        if (EPC_DEBUG_LEVEL (1))
          g_debug ("%s: Avahi client is registering...", G_STRLOC);

        epc_dispatcher_foreach_service (self, epc_service_reset);
        break;

      case AVAHI_CLIENT_S_COLLISION:
        if (EPC_DEBUG_LEVEL (1))
          g_debug ("%s: Collision detected...", G_STRLOC);

        epc_dispatcher_handle_collision (self);
        break;

      case AVAHI_CLIENT_FAILURE:
        if (EPC_DEBUG_LEVEL (1))
          g_debug ("%s: Suspending entry groups...", G_STRLOC);

        epc_dispatcher_foreach_service (self, epc_service_suspend);
        break;

      case AVAHI_CLIENT_CONNECTING:
        if (EPC_DEBUG_LEVEL (1))
          g_debug ("%s: Waiting for Avahi server...", G_STRLOC);

        break;
    }

  g_clear_error (&error);
}

static void
epc_dispatcher_handle_collision (EpcDispatcher *self)
{
  gchar *alternative;

  alternative = avahi_alternative_service_name (self->priv->name);

  g_warning ("%s: Service name collision for `%s', renaming to `%s'.",
             G_STRLOC, self->priv->name, alternative);

  epc_dispatcher_foreach_service (self, epc_service_reset);

  g_free (self->priv->name);
  self->priv->name = alternative;
  g_object_notify (G_OBJECT (self), "name");

  epc_dispatcher_foreach_service (self, epc_service_publish);
}

static void
epc_dispatcher_init (EpcDispatcher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPC_TYPE_DISPATCHER,
                                            EpcDispatcherPrivate);

  self->priv->services = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                NULL, epc_service_free);
}

static void
epc_dispatcher_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EpcDispatcher *self = EPC_DISPATCHER (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_return_if_fail (NULL != g_value_get_string (value));

        g_free (self->priv->name);
        self->priv->name = g_value_dup_string (value);

        /* The reset also causes a transition into the UNCOMMITED state,
         * which causes re-publication of the services.
         */
        epc_dispatcher_foreach_service (self, epc_service_reset);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_dispatcher_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EpcDispatcher *self = EPC_DISPATCHER (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_dispatcher_dispose (GObject *object)
{
  EpcDispatcher *self = EPC_DISPATCHER (object);

  if (self->priv->services)
    {
      g_hash_table_unref (self->priv->services);
      self->priv->services = NULL;
    }

  if (self->priv->watch_id)
    {
      epc_shell_watch_remove (self->priv->watch_id);
      self->priv->watch_id = 0;
    }

  g_free (self->priv->name);
  self->priv->name = NULL;

  G_OBJECT_CLASS (epc_dispatcher_parent_class)->dispose (object);
}

static void
epc_dispatcher_class_init (EpcDispatcherClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_dispatcher_set_property;
  oclass->get_property = epc_dispatcher_get_property;
  oclass->dispose = epc_dispatcher_dispose;

  g_object_class_install_property (oclass, PROP_NAME,
                                   g_param_spec_string ("name", "Name",
                                                        "User friendly name of the service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcDispatcherPrivate));
}

/**
 * epc_dispatcher_new:
 * @name: the human friendly name of the service
 *
 * Creates a new #EpcDispatcher object for announcing a DNS-SD service.
 * The service is announced on all network interfaces.
 *
 * Call #epc_dispatcher_add_service to actually announce a service.
 *
 * Returns: the newly created #EpcDispatcher object.
 */
EpcDispatcher*
epc_dispatcher_new (const gchar *name)
{
  return g_object_new (EPC_TYPE_DISPATCHER, "name", name, NULL);
}

/**
 * epc_dispatcher_run:
 * @dispatcher: a #EpcDispatcher
 * @error: return location for a #GError, or %NULL
 *
 * Starts the <citetitle>Avahi</citetitle> client of the #EpcDispatcher. If the
 * client was not started, the function returns %FALSE and sets @error. The
 * error domain is #EPC_AVAHI_ERROR. Possible error codes are those of the
 * <citetitle>Avahi</citetitle> library.
 *
 * Returns: %TRUE when the dispatcher was started successfully,
 * %FALSE if an error occurred.
 */
gboolean
epc_dispatcher_run (EpcDispatcher  *self,
                    GError        **error)
{
  g_return_val_if_fail (EPC_IS_DISPATCHER (self), FALSE);
  g_return_val_if_fail (0 == self->priv->watch_id, FALSE);

  self->priv->watch_id =
    epc_shell_watch_avahi_client_state (epc_dispatcher_client_cb,
                                        self, NULL, error);

  return (0 != self->priv->watch_id);
}

/**
 * epc_dispatcher_reset:
 * @dispatcher: a #EpcDispatcher
 *
 * Revokes all service announcements of this #EpcDispatcher.
 */
void
epc_dispatcher_reset (EpcDispatcher *self)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_hash_table_remove_all (self->priv->services);
}

/**
 * epc_dispatcher_add_service:
 * @dispatcher: a #EpcDispatcher
 * @protocol: the #EpcAddressFamily this service supports
 * @type: the machine friendly name of the service
 * @domain: the DNS domain for the announcement, or %NULL
 * @host: the fully qualified host name of the service, or %NULL
 * @port: the TCP/IP port of the service
 * @...: an optional list of TXT records, terminated by %NULL
 *
 * Announces a TCP/IP service via DNS-SD.
 *
 * The service @type shall be a well-known DNS-SD service type as listed on
 * <ulink url="http://www.dns-sd.org/ServiceTypes.html" />. This function tries
 * to announce both the base service type and the sub service type when the
 * service name contains more than just one dot: Passing "_anon._sub._ftp._tcp"
 * for @type will announce the services "_ftp._tcp" and "_anon._sub._ftp._tcp".
 *
 * The function can be called more than once. Is this necessary when the server
 * provides different access methods. For instance a web server could provide
 * HTTP and encrypted HTTPS services at the same time. Calling this function
 * multiple times also is useful for servers providing the same service at
 * different, but not all network interfaces of the host.
 *
 * When passing %NULL for @domain, the service is announced within the local
 * network only, otherwise it is announced at the specified DNS domain. The
 * responsible server must be <ulink url="http://www.dns-sd.org/ServerSetup.html">
 * configured to support DNS-SD</ulink>.
 *
 * Pass %NULL for @host to use the official host name of the machine to announce
 * the service. On machines with multiple DNS entries you might want to explictly
 * choose a fully qualified DNS name to announce the service.
 */
void
epc_dispatcher_add_service (EpcDispatcher    *self,
                            EpcAddressFamily  protocol,
                            const gchar      *type,
                            const gchar      *domain,
                            const gchar      *host,
                            guint16           port,
                                              ...)
{
  EpcService *service;
  va_list args;

  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_return_if_fail (port > 0);

  g_return_if_fail (NULL != type);
  g_return_if_fail (type == epc_service_type_get_base (type));
  g_return_if_fail (NULL == g_hash_table_lookup (self->priv->services, type));

  va_start (args, port);

  service = epc_service_new (self, avahi_af_to_proto (protocol),
                             type, domain, host, port, args);

  va_end (args);

  g_hash_table_insert (self->priv->services, service->type, service);

  if (self->priv->watch_id)
    epc_service_run (service);
}

/**
 * epc_dispatcher_add_service_subtype:
 * @dispatcher: a #EpcDispatcher
 * @type: the base service type
 * @subtype: the sub service type
 *
 * Announces an additional sub service for a registered DNS-SD service.
 *
 * <note><para>
 * This function will fail silently, when the service specified by
 * @type hasn't been registered yet.
 * </para></note>
 */
void
epc_dispatcher_add_service_subtype (EpcDispatcher *self,
                                    const gchar   *type,
                                    const gchar   *subtype)
{
  EpcService *service;

  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_return_if_fail (NULL != subtype);
  g_return_if_fail (NULL != type);

  service = g_hash_table_lookup (self->priv->services, type);

  g_return_if_fail (NULL != service);

  epc_service_add_subtype (service, subtype);

  if (self->priv->watch_id)
    epc_service_publish_subtype (service, subtype);
}

/**
 * epc_dispatcher_set_service_details:
 * @dispatcher: a #EpcDispatcher
 * @type: the service type
 * @...: a list of TXT records, terminated by %NULL
 *
 * Updates the list of TXT records for a registered DNS-SD service.
 * The TXT records are specified by the service type and usually
 * have the form of key-value pairs:
 *
 * <informalexample><programlisting>
 *  path=/dwarf-blog/
 * </programlisting></informalexample>
 *
 * <note><para>
 * This function will fail silently, when the service specified by
 * @type hasn't been registered yet.
 * </para></note>
 */
void
epc_dispatcher_set_service_details (EpcDispatcher *self,
                                    const gchar   *type,
                                                   ...)
{
  EpcService *service;
  va_list args;

  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_return_if_fail (NULL != type);

  service = g_hash_table_lookup (self->priv->services, type);

  g_return_if_fail (NULL != service);

  va_start (args, type);
  avahi_string_list_free (service->details);
  service->details = avahi_string_list_new_va (args);
  va_end (args);

  epc_service_publish_details (service);
}

/**
 * epc_dispatcher_set_name:
 * @dispatcher: a #EpcDispatcher
 * @name: the new user friendly name
 *
 * Changes the user friendly name used for announcing services.
 * See #EpcDispatcher:name.
 */
void
epc_dispatcher_set_name (EpcDispatcher *self,
                         const gchar   *name)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_object_set (self, "name", name, NULL);
}

/**
 * epc_dispatcher_get_name:
 * @dispatcher: a #EpcDispatcher
 *
 * Queries the user friendly name used for announcing services.
 * See #EpcDispatcher:name.
 *
 * Returns: The user friendly name of the service.
 */
G_CONST_RETURN gchar*
epc_dispatcher_get_name (EpcDispatcher *self)
{
  g_return_val_if_fail (EPC_IS_DISPATCHER (self), NULL);
  return self->priv->name;
}
