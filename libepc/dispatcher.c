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

#include "libepc/dispatcher.h"

#include "libepc/enums.h"
#include "libepc/service-monitor.h"
#include "libepc/service-type.h"
#include "libepc/shell.h"

#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <uuid/uuid.h>
#include <string.h>

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
  PROP_NAME,
  PROP_COOKIE,
  PROP_COLLISION_HANDLING
};

struct _EpcService
{
  EpcDispatcher   *dispatcher;
  AvahiEntryGroup *group;
  AvahiProtocol    protocol;
  guint            commit_handler;

  gchar           *type;
  gchar           *domain;
  gchar           *host;
  guint16          port;

  GList           *subtypes;
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
  gchar                *name;
  gchar                *cookie;
  EpcCollisionHandling  collisions;
  EpcServiceMonitor    *monitor;
  GHashTable           *services;
  guint                 watch_id;
};

static void epc_dispatcher_handle_collision (EpcDispatcher *self,
                                             const gchar   *domain);
static void epc_service_run                 (EpcService    *self);

G_DEFINE_TYPE (EpcDispatcher, epc_dispatcher, G_TYPE_OBJECT);

static gboolean
epc_service_commit_cb (gpointer data)
{
  EpcService *self = data;

  self->commit_handler = 0;
  g_return_val_if_fail (NULL != self->group, FALSE);
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
        epc_dispatcher_handle_collision (self->dispatcher, self->domain);
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
  else
    epc_service_run (self);
}

static void
epc_service_group_cb (AvahiEntryGroup      *group,
                      AvahiEntryGroupState  state,
                      gpointer              data)
{
  EpcService *self = data;
  GError *error = NULL;

  if (self->group)
    g_assert (group == self->group);
  else
    self->group = group;

  switch (state)
    {
      case AVAHI_ENTRY_GROUP_REGISTERING:
      case AVAHI_ENTRY_GROUP_ESTABLISHED:
        break;

      case AVAHI_ENTRY_GROUP_UNCOMMITED:
        epc_service_publish (self);
        break;

      case AVAHI_ENTRY_GROUP_COLLISION:
        epc_dispatcher_handle_collision (self->dispatcher, self->domain);
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

      epc_shell_create_avahi_entry_group (epc_service_group_cb, self);
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
  if (self->commit_handler)
    {
      g_source_remove (self->commit_handler);
      self->commit_handler = 0;
    }

  if (self->group)
    {
      avahi_entry_group_free (self->group);
      self->group = NULL;
    }
}

static void
epc_service_remove_detail (EpcService  *self,
                           const gchar *key)
{
  AvahiStringList *curr = self->details;
  AvahiStringList *prev = NULL;

  gsize len = strlen(key);

  while (curr)
    {
      if (!memcmp (curr->text, key, len) && '=' == curr->text[len])
        {
          AvahiStringList *next = curr->next;

          curr->next = NULL;

          if (!prev)
            self->details = next;
          else
            prev->next = next;

          avahi_string_list_free (curr);
          curr = next;
        }
      else
        curr = avahi_string_list_get_next (prev = curr);
    }
}

static void
epc_service_set_detail (EpcService  *self,
                        const gchar *key,
                        const gchar *value)
{
  epc_service_remove_detail (self, key);
  self->details = avahi_string_list_add_pair (self->details, key, value);
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

        epc_dispatcher_handle_collision (self, NULL);
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
epc_dispatcher_change_name (EpcDispatcher *self)
{
  gchar *alternative = avahi_alternative_service_name (self->priv->name);

  g_message ("%s: Service name collision for `%s', renaming to `%s'.",
             G_STRFUNC, self->priv->name, alternative);

  g_free (self->priv->name);
  self->priv->name = alternative;
  g_object_notify (G_OBJECT (self), "name");

  epc_dispatcher_foreach_service (self, epc_service_publish);
}

static void
epc_dispatcher_service_removed_cb (EpcServiceMonitor *monitor,
                                   const gchar       *name,
                                   const gchar       *type G_GNUC_UNUSED,
                                   gpointer           data)
{
  EpcDispatcher *self = EPC_DISPATCHER (data);
  g_return_if_fail (monitor == self->priv->monitor);

  if (g_str_equal (name, self->priv->name))
    {
      g_message ("%s: Conflicting service for `%s' disappeared, republishing.",
                 G_STRFUNC, self->priv->name);

      g_object_unref (self->priv->monitor);
      self->priv->monitor = NULL;

      epc_dispatcher_foreach_service (self, epc_service_reset);
    }
}

static void
epc_dispatcher_service_found_cb (EpcServiceMonitor *monitor,
                                 const gchar       *name,
                                 EpcServiceInfo    *info,
                                 gpointer           data)
{
  EpcDispatcher *self = EPC_DISPATCHER (data);
  g_return_if_fail (monitor == self->priv->monitor);

  if (g_str_equal (name, self->priv->name))
    {
      const gchar *cookie = epc_service_info_get_detail (info, "cookie");

      if (EPC_DEBUG_LEVEL (1))
        g_debug ("%s: foreign cookie: %s, own cookie: %s",
                 G_STRFUNC, cookie, self->priv->cookie);

      if (NULL == cookie || NULL == self->priv->cookie ||
          strcmp (cookie, self->priv->cookie))
        {
          g_message ("%s: Conflicting service for `%s' has different cookie, "
                     "resorting to rename strategy.", G_STRFUNC, self->priv->name);

          g_signal_handlers_disconnect_by_func(monitor, epc_dispatcher_service_removed_cb, self);
          g_signal_handlers_disconnect_by_func(monitor, epc_dispatcher_service_found_cb, self);

          epc_dispatcher_change_name (self);
        }
    }
}

static void
epc_dispatcher_get_service_types_cb (gpointer key,
                                     gpointer value G_GNUC_UNUSED,
                                     gpointer data)
{
  gchar ***types = data;
  **types = key;
  *types += 1;
}

static gchar**
epc_dispatcher_get_service_types (EpcDispatcher *self)
{
  gchar **types, **iter;

  types = iter = g_new0 (gchar*, g_hash_table_size (self->priv->services) + 1);
  g_hash_table_foreach (self->priv->services, epc_dispatcher_get_service_types_cb, &iter);

  return types;
}

static void
epc_dispatcher_watch_other (EpcDispatcher *self,
                            const gchar   *domain)
{
  gchar **types;

  g_return_if_fail (NULL == self->priv->monitor);


  types = epc_dispatcher_get_service_types (self);
  self->priv->monitor = epc_service_monitor_new_for_types_strv (domain, types);
  g_free (types);

  g_signal_connect (self->priv->monitor, "service-found",
                    G_CALLBACK (epc_dispatcher_service_found_cb),
                    self);
  g_signal_connect (self->priv->monitor, "service-removed",
                    G_CALLBACK (epc_dispatcher_service_removed_cb),
                    self);

  g_message ("%s: Service name collision for `%s', "
             "waiting for other service to disappear.",
             G_STRFUNC, self->priv->name);
}

static void
epc_dispatcher_handle_collision (EpcDispatcher *self,
                                 const gchar   *domain)
{
  epc_dispatcher_foreach_service (self, epc_service_suspend);

  switch (self->priv->collisions)
    {
      case EPC_COLLISIONS_IGNORE:
        break; /* nothing to do */

      case EPC_COLLISIONS_CHANGE_NAME:
        epc_dispatcher_change_name (self);
        break;

      case EPC_COLLISIONS_UNIQUE_SERVICE:
        epc_dispatcher_watch_other (self, domain);
        break;
    }
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
epc_dispatcher_set_cookie_cb (gpointer key G_GNUC_UNUSED,
                              gpointer value,
                              gpointer data)
{
  EpcService *service = value;
  const gchar *cookie = data;

  if (cookie)
    epc_service_set_detail (service, "cookie", cookie);
  else
    epc_service_remove_detail (service, "cookie");

  epc_service_reset (service);
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

      case PROP_COOKIE:
        g_free (self->priv->cookie);
        self->priv->cookie = g_value_dup_string (value);
        g_hash_table_foreach (self->priv->services,
                              epc_dispatcher_set_cookie_cb,
                              self->priv->cookie);
        break;

      case PROP_COLLISION_HANDLING:
        self->priv->collisions = g_value_get_enum (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static const gchar*
epc_dispatcher_ensure_cookie (EpcDispatcher *self)
{
  if (EPC_COLLISIONS_UNIQUE_SERVICE == self->priv->collisions && !self->priv->cookie)
    {
      uuid_t cookie;

      self->priv->cookie = g_new0 (gchar, 37);

      uuid_generate_time (cookie);
      uuid_unparse_lower (cookie, self->priv->cookie);

      g_debug ("%s: generating service cookie: %s", G_STRLOC, self->priv->cookie);
    }

  return self->priv->cookie;
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

      case PROP_COOKIE:
        g_value_set_string (value, epc_dispatcher_ensure_cookie (self));
        break;

      case PROP_COLLISION_HANDLING:
        g_value_set_enum (value, self->priv->collisions);
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

  if (self->priv->monitor)
    {
      g_object_unref (self->priv->monitor);
      self->priv->monitor = NULL;
    }

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

  g_free (self->priv->cookie);
  self->priv->cookie = NULL;

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

  /**
   * EpcConsumer:cookie:
   *
   * Unique identifier of the service. This cookie is used for implementing
   * #EPC_COLLISIONS_UNIQUE_SERVICE, and usually is a UUID or the MD5/SHA1/...
   * checksum of a central document. When passing %NULL, but using the
   * #EPC_COLLISIONS_UNIQUE_SERVICE strategy a time based UUID is
   * generated and used as service identifier.
   *
   * Since: 0.3.1
   */
  g_object_class_install_property (oclass, PROP_COOKIE,
                                   g_param_spec_string ("cookie", "Cookie",
                                                        "Unique identifier of the service",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  /**
   * EpcConsume:collision-handling:
   *
   * The collision handling method to use.
   *
   * Since: 0.3.1
   */
  g_object_class_install_property (oclass, PROP_COLLISION_HANDLING,
                                   g_param_spec_enum ("collision-handling", "Collision Handling",
                                                      "The collision handling method to use",
                                                      EPC_TYPE_COLLISION_HANDLING,
                                                      EPC_COLLISIONS_CHANGE_NAME,
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
 * Call epc_dispatcher_add_service() to actually announce a service.
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

  if (epc_dispatcher_ensure_cookie (self))
    epc_service_set_detail (service, "cookie", self->priv->cookie);

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

  if (self->priv->watch_id && service->group)
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
 * epc_dispatcher_set_cookie:
 * @dispatcher: a #EpcDispatcher
 * @cookie: the new service identifier, or %NULL
 *
 * Changes the unique identifier of the service.
 * See #EpcDispatcher:cookie for details.
 *
 * Since: 0.3.1
 */
void
epc_dispatcher_set_cookie (EpcDispatcher *self,
                           const gchar   *cookie)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_object_set (self, "cookie", cookie, NULL);
}

/**
 * epc_dispatcher_set_collision_handling:
 * @dispatcher: a #EpcDispatcher
 * @method: the new strategy
 *
 * Changes the collision handling strategy the dispatcher uses.
 * See #EpcDispatcher:collision-handling for details.
 *
 * Since: 0.3.1
 */
void
epc_dispatcher_set_collision_handling (EpcDispatcher       *self,
                                       EpcCollisionHandling method)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_object_set (self, "collision-handling", method, NULL);
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

/**
 * epc_dispatcher_get_cookie:
 * @dispatcher: a #EpcDispatcher
 *
 * Queries the unique identifier of the service.
 * See #EpcDispatcher:cookie for details.
 *
 * Returns: The unique identifier of the service, or %NULL on error.
 * Since: 0.3.1
 */
G_CONST_RETURN gchar*
epc_dispatcher_get_cookie (EpcDispatcher *self)
{
  g_return_val_if_fail (EPC_IS_DISPATCHER (self), NULL);
  return epc_dispatcher_ensure_cookie (self);
}

/**
 * epc_dispatcher_get_collision_handling:
 * @dispatcher: a #EpcDispatcher
 *
 * Queries the collision handling strategy the dispatcher uses.
 * See #EpcDispatcher:collision-handling for details.
 *
 * Returns: The dispatcher's collision handling strategy,
 * or #EPC_COLLISIONS_IGNORE on error.
 * Since: 0.3.1
 */
EpcCollisionHandling
epc_dispatcher_get_collision_handling (EpcDispatcher *self)
{
  g_return_val_if_fail (EPC_IS_DISPATCHER (self), EPC_COLLISIONS_IGNORE);
  return self->priv->collisions;
}


