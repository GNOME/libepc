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
 * SECTION:dispatcher
 * @short_description: publish DNS-SD services
 * @include: libepc/dispatcher.h
 *
 * The #EpcDispatcher object provides an easy method for publishing
 * DNS-SD services. Unlike established APIs like Avahi or HOWL the
 * #EpcDispatcher doesn't expose any state changes reported by the
 * DNS-SD daemon, but tries handle them automatically. Such state
 * changes include for instance name collisions or restart of
 * the DNS-SD daemon.
 *
 * <example>
 *  <title>Publish a printing service</title>
 *  <programlisting>
 *   dispatcher = epc_dispatcher_new (AVAHI_IF_UNSPEC,
 *                                    AVAHI_PROTO_UNSPEC,
 *                                    "Dead Tree Desecrator");
 *   epc_dispatcher_add_service (dispatcher, "_ipp._tcp",
 *                               NULL, NULL, 651, "path=/printers", NULL);
 *   epc_dispatcher_add_service (dispatcher, "_duplex._sub._printer._tcp",
 *                               NULL, NULL, 515, NULL);
 *  </programlisting>
 * </example>
 */

#include "dispatcher.h"

#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

#include <string.h>

typedef struct _EpcService EpcService;

enum
{
  PROP_NONE,
  PROP_INTERFACE,
  PROP_PROTOCOL,
  PROP_NAME
};

struct _EpcService
{
  EpcDispatcher *dispatcher;
  AvahiEntryGroup *group;

  gchar *type;
  gchar *domain;
  gchar *host;
  guint16 port;

  GList *subtypes;
  AvahiStringList *details;
};

static gboolean epc_debug = FALSE;

/**
 * EpcDispatcherPrivate:
 *
 * Private fields of the #EpcDispatcher class.
 */
struct _EpcDispatcherPrivate
{
  gchar *name;
  AvahiGLibPoll *poll;
  AvahiClient *client;
  AvahiIfIndex interface;
  AvahiProtocol protocol;
  GHashTable *services;
};

static void epc_dispatcher_handle_collision (EpcDispatcher *self);
static void epc_dispatcher_reset_client     (EpcDispatcher *self);

static void
epc_service_publish_subtype (EpcService  *self,
                             const gchar *subtype,
                             gboolean     commit)
{
  gint result;

  if (epc_debug)
    g_debug ("%s: Publishing sub-service `%s' for `%s'...",
             G_STRLOC, subtype, self->dispatcher->priv->name);

  result =
    avahi_entry_group_add_service_subtype (self->group,
                                           self->dispatcher->priv->interface,
                                           self->dispatcher->priv->protocol, 0,
                                           self->dispatcher->priv->name,
                                           self->type, self->domain,
                                           subtype);

  if (AVAHI_OK != result)
    g_warning ("%s: Failed to publish sub-service `%s' for `%s': %s (%d)",
               G_STRLOC, subtype, self->dispatcher->priv->name,
               avahi_strerror (result), result);

  if (commit)
    avahi_entry_group_commit (self->group);
}

static void
epc_service_publish_details (EpcService *self,
                             gboolean    commit)
{
  gint result;

  if (epc_debug)
    g_debug ("%s: Publishing details for `%s'...",
             G_STRLOC, self->dispatcher->priv->name);

  result =
    avahi_entry_group_update_service_txt_strlst (self->group,
                                                 self->dispatcher->priv->interface,
                                                 self->dispatcher->priv->protocol, 0,
                                                 self->dispatcher->priv->name,
                                                 self->type, self->domain,
                                                 self->details);

  if (AVAHI_OK != result)
    g_warning ("%s: Failed publish details for `%s': %s (%d)",
               G_STRLOC, self->dispatcher->priv->name,
               avahi_strerror (result), result);

  if (commit)
    avahi_entry_group_commit (self->group);
}

static void
epc_service_publish (EpcService *self)
{
  gint result;
  GList *iter;

  if (epc_debug)
    g_debug ("%s: Publishing service `%s' for `%s'...",
             G_STRLOC, self->type, self->dispatcher->priv->name);

  result =
    avahi_entry_group_add_service_strlst (self->group,
                                          self->dispatcher->priv->interface,
                                          self->dispatcher->priv->protocol, 0,
                                          self->dispatcher->priv->name,
                                          self->type, self->domain,
                                          self->host, self->port,
                                          self->details);

  if (AVAHI_OK != result)
    g_warning ("%s: Failed to publish service `%s' for `%s': %s (%d)",
               G_STRLOC, self->type, self->dispatcher->priv->name,
               avahi_strerror (result), result);

  for (iter = self->subtypes; iter; iter = iter->next)
    epc_service_publish_subtype (self, iter->data, FALSE);

  avahi_entry_group_commit (self->group);
}

static void
epc_service_reset (EpcService *self)
{
  avahi_entry_group_reset (self->group);
}

static void
epc_service_group_cb (AvahiEntryGroup      *group,
                      AvahiEntryGroupState  state,
                      gpointer              data)
{
  AvahiClient *client = avahi_entry_group_get_client (group);
  EpcService *self = data;

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
        g_warning ("%s: Avahi group failure: %s (%d)\n", G_STRLOC,
                   avahi_strerror (avahi_client_errno (client)),
                   avahi_client_errno (client));
        epc_dispatcher_reset_client (self->dispatcher);
        break;
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
                 const gchar   *type,
                 const gchar   *domain,
                 const gchar   *host,
                 guint16        port,
                 va_list        args)
{
  EpcService *self = g_slice_new0 (EpcService);
  const gchar *service;

  service = type + strlen (type);

  while (service > type && '.' != *(--service));
  while (service > type && '.' != *(--service));

  if (service > type)
    service += 1;

  self->dispatcher = dispatcher;
  self->details = avahi_string_list_new_va (args);
  self->type = g_strdup (service);
  self->port = port;

  if (domain)
    self->domain = g_strdup (domain);
  if (host)
    self->host = g_strdup (host);
  if (service > type)
    epc_service_add_subtype (self, type);

  self->group = avahi_entry_group_new (dispatcher->priv->client,
                                       epc_service_group_cb, self);

#if 0
  if (AVAHI_ERR_COLLISION == result)
    handle_collision (self);
  else if (result)
    g_warning ("%s: Failed to register %s service: %s (%d)\n",
               G_STRLOC, service, avahi_strerror(result), result);
#endif

  return self;
}

static void
epc_service_free (EpcService *self)
{
  avahi_entry_group_free (self->group);
  avahi_string_list_free (self->details);

  g_list_foreach (self->subtypes, (GFunc)g_free, NULL);
  g_list_free (self->subtypes);

  g_free (self->type);
  g_free (self->domain);
  g_free (self->host);

  g_slice_free (EpcService, self);
}

G_DEFINE_TYPE (EpcDispatcher, epc_dispatcher, G_TYPE_OBJECT);

static void
epc_dispatcher_publish_cb (gpointer key G_GNUC_UNUSED,
                           gpointer value,
                           gpointer data G_GNUC_UNUSED)
{
  epc_service_publish (value);
}

static void
epc_dispatcher_reset_cb (gpointer key G_GNUC_UNUSED,
                         gpointer value,
                         gpointer data G_GNUC_UNUSED)
{
  epc_service_reset (value);
}

static void
epc_dispatcher_client_cb (AvahiClient      *client,
                          AvahiClientState  state,
                          gpointer          data)
{
  EpcDispatcher *self = data;

  g_assert (NULL == self->priv->client ||
            client == self->priv->client);

  switch (state)
    {
      case AVAHI_CLIENT_S_RUNNING:
        if (epc_debug)
          g_debug ("%s: Avahi client is running...", G_STRLOC);

        g_hash_table_foreach (self->priv->services, epc_dispatcher_publish_cb, NULL);
        break;

      case AVAHI_CLIENT_S_COLLISION:
      case AVAHI_CLIENT_S_REGISTERING:
        if (epc_debug)
          g_debug ("%s: Avahi client collision/registering...", G_STRLOC);

        g_hash_table_foreach (self->priv->services, epc_dispatcher_reset_cb, self);
        break;

      case AVAHI_CLIENT_FAILURE:
        g_warning ("%s: Avahi client failure: %s (%d)\n", G_STRLOC,
                   avahi_strerror (avahi_client_errno (client)),
                   avahi_client_errno (client));
        epc_dispatcher_reset_client (self);
        break;

      case AVAHI_CLIENT_CONNECTING:
        if (epc_debug)
          g_debug ("%s: Waiting for Avahi server...", G_STRLOC);

        break;
    }
}

static void
epc_dispatcher_handle_collision (EpcDispatcher *self)
{
  gchar *alternate = avahi_alternative_service_name (self->priv->name);

  g_warning ("%s: Service name collision for `%s', renaming to `%s'.",
             G_STRLOC, self->priv->name, alternate);

  g_free (self->priv->name);
  self->priv->name = alternate;

  g_object_notify (G_OBJECT (self), "name");
  g_hash_table_foreach (self->priv->services, epc_dispatcher_publish_cb, NULL);
}

static void
epc_dispatcher_reset_client (EpcDispatcher *self)
{
  int error = 0;

  if (self->priv->client)
    {
      avahi_client_free (self->priv->client);
      self->priv->client = NULL;
    }

  self->priv->client =
    avahi_client_new (avahi_glib_poll_get (self->priv->poll),
                      AVAHI_CLIENT_NO_FAIL, epc_dispatcher_client_cb,
                      self, &error);

  if (!self->priv->client)
    g_warning ("%s: Failed to setup Avahi client: %s (%d)\n",
               G_STRLOC, avahi_strerror (error), error);
}

static void
epc_dispatcher_init (EpcDispatcher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPC_TYPE_DISPATCHER,
                                            EpcDispatcherPrivate);

  self->priv->services =
    g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                           (GDestroyNotify) epc_service_free);

  avahi_set_allocator (avahi_glib_allocator ());

  self->priv->poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
  epc_dispatcher_reset_client (self);
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
      case PROP_INTERFACE:
        self->priv->interface = g_value_get_int (value);
        break;

      case PROP_PROTOCOL:
        self->priv->protocol = g_value_get_int (value);
        break;

      case PROP_NAME:
        g_assert (NULL == self->priv->name);
        self->priv->name = g_value_dup_string (value);
        /* TODO: re-publish */
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
      case PROP_INTERFACE:
        g_value_set_int (value, self->priv->interface);
        break;

      case PROP_PROTOCOL:
        g_value_set_int (value, self->priv->protocol);
        break;

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

  if (self->priv->client)
    {
      avahi_client_free (self->priv->client);
      self->priv->client = NULL;
    }

  if (self->priv->poll)
    {
      avahi_glib_poll_free (self->priv->poll);
      self->priv->poll = NULL;
    }

  g_free (self->priv->name);
  self->priv->name = NULL;

  G_OBJECT_CLASS (epc_dispatcher_parent_class)->dispose (object);
}

static void
epc_dispatcher_class_init (EpcDispatcherClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  if (g_getenv ("EPC_DEBUG"))
    epc_debug = TRUE;

  oclass->set_property = epc_dispatcher_set_property;
  oclass->get_property = epc_dispatcher_get_property;
  oclass->dispose = epc_dispatcher_dispose;

  g_object_class_install_property (oclass, PROP_INTERFACE,
                                   g_param_spec_int ("interface", "Interface Index",
                                                     "The interface this service shall be announced on",
                                                     AVAHI_IF_UNSPEC, G_MAXINT, AVAHI_IF_UNSPEC,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_PROTOCOL,
                                   g_param_spec_int ("protocol", "Protocol",
                                                     "The protocol this service shall be announced on",
                                                     AVAHI_PROTO_UNSPEC, G_MAXINT, AVAHI_PROTO_UNSPEC,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_NAME,
                                   g_param_spec_string ("name", "Name",
                                                        "User friendly name for the service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcDispatcherPrivate));
}

/**
 * epc_dispatcher_new:
 * @interface: index of the network interface to use
 * @protocol: the network protocol to use (IPv4, IPv6)
 * @name: the human friendly name of the service
 *
 * Creates a new #EpcDispatcher object for announcing a DNS-SD service.
 * The service is announced on all network interfaces, when AVAHI_IF_UNSPEC
 * is passed for @interface.
 *
 * Call #epc_dispatcher_add_service to actually announce a service.
 *
 * Returns: the newly created #EpcDispatcher object.
 */
EpcDispatcher*
epc_dispatcher_new (AvahiIfIndex   interface,
                    AvahiProtocol  protocol,
                    const gchar   *name)
{
  return g_object_new (EPC_TYPE_DISPATCHER,
                       "interface", interface,
                       "protocol", protocol,
                       "name", name, NULL);
}

/**
 * epc_dispatcher_add_service:
 * @dispatcher: the #EpcDispatcher
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
epc_dispatcher_add_service (EpcDispatcher *self,
                            const gchar   *type,
                            const gchar   *domain,
                            const gchar   *host,
                            guint16        port,
                                           ...)
{
  EpcService *service;
  va_list args;

  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_return_if_fail (NULL != type);
  g_return_if_fail (port > 0);

  va_start (args, port);
  service = epc_service_new (self, type, domain, host, port, args);
  g_hash_table_insert (self->priv->services, service->type, service);
  va_end (args);
}

/**
 * epc_dispatcher_add_service_subtype:
 * @dispatcher: the #EpcDispatcher
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
  epc_service_publish_subtype (service, subtype, TRUE);
}

/**
 * epc_dispatcher_set_service_details:
 * @dispatcher: the #EpcDispatcher
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

  epc_service_publish_details (service, TRUE);
}
