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

#include "libepc/service-info.h"

#include <string.h>

/**
 * SECTION:service-info
 * @short_description: DNS-SD service descriptions
 * @include: libepc/service-info.h
 * @stability: Unstable
 *
 * The #EpcServiceInfo object describes DNS-SD services.
 */

/**
 * EpcServiceInfo:
 *
 * Description of a network service.
 * See also: epc_service_monitor_new().
 */
struct _EpcServiceInfo
{
  volatile gint    ref_count;

  gchar           *type;
  gchar           *host;
  guint            port;

  AvahiStringList *details;

  AvahiAddress    *address;
  gchar           *ifname;
};

GType
epc_service_info_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (!type))
    type = g_boxed_type_register_static ("EpcServiceInfo",
                                         (GBoxedCopyFunc) epc_service_info_ref,
                                         (GBoxedFreeFunc) epc_service_info_unref);

  return type;
}

/**
 * epc_service_info_new_full:
 * @type: the DNS-SD service type
 * @host: the DNS hostname
 * @port: the TCP/IP port
 * @details: list of key-value pairs, or %NULL
 * @address: IP address of the service, or %NULL
 * @ifname: network interface for contacting the service, or %NULL
 *
 * Creates a new service description using the information provided. The
 * @details list usually is retrieved from the TXT record the dynamic naming
 * system (DNS) provides for the service. When using Avahi's service chooser
 * aui_service_dialog_get_txt_data() can be used for getting a @details list.
 * To create an ad-hoc list use avahi_string_list_new() and related functions.
 *
 * Returns: The newly created service description, or %NULL on error.
 */
EpcServiceInfo*
epc_service_info_new_full (const gchar           *type,
                           const gchar           *host,
                           guint                  port,
                           const AvahiStringList *details,
                           const AvahiAddress    *address,
                           const gchar           *ifname)
{
  EpcServiceInfo *self;

  g_return_val_if_fail (NULL != type, NULL);
  g_return_val_if_fail (NULL != host, NULL);
  g_return_val_if_fail (port != 0,    NULL);

  self = g_slice_new0 (EpcServiceInfo);

  self->ref_count = 1;
  self->type = g_strdup (type);
  self->host = g_strdup (host);
  self->port = port;

  if (details)
    self->details = avahi_string_list_copy (details);

  if (address)
    self->address = g_memdup (address, sizeof *address);
  if (ifname)
    self->ifname = g_strdup (ifname);

  return self;
}

/**
 * epc_service_info_new:
 * @type: the DNS-SD service type
 * @host: the DNS hostname
 * @port: the TCP/IP port
 * @details: list of key-value pairs, or %NULL
 *
 * Creates a new service description using the information provided. The
 * @details list usually is retrieved from the TXT record the dynamic naming
 * system (DNS) provides for the service. When using Avahi's service chooser
 * aui_service_dialog_get_txt_data() can be used for getting a @details list.
 * To create an ad-hoc list use avahi_string_list_new() and related functions.
 *
 * Returns: The newly created service description, or %NULL on error.
 */
EpcServiceInfo*
epc_service_info_new (const gchar           *type,
                      const gchar           *host,
                      guint                  port,
                      const AvahiStringList *details)
{
  return epc_service_info_new_full (type, host, port, details, NULL, NULL);
}

/**
 * epc_service_info_ref:
 * @info: a #EpcServiceInfo
 *
 * Increases the reference count of @info by one.
 * See also: epc_service_info_unref()
 *
 * Returns: The same @info object.
 */
EpcServiceInfo*
epc_service_info_ref (EpcServiceInfo *self)
{
  g_return_val_if_fail (EPC_IS_SERVICE_INFO (self), NULL);
  g_atomic_int_inc (&self->ref_count);
  return self;
}

/**
 * epc_service_info_unref:
 * @info: a #EpcServiceInfo
 *
 * Decreases the reference count of @info by one. When its reference count
 * drops to 0, the object is finalized (i.e. its memory is freed).
 *
 * See also: epc_service_info_ref()
 */
void
epc_service_info_unref (EpcServiceInfo *self)
{
  g_return_if_fail (EPC_IS_SERVICE_INFO (self));

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_free (self->address);
      g_free (self->ifname);
      g_free (self->type);
      g_free (self->host);

      if (self->details)
        avahi_string_list_free (self->details);

      g_slice_free (EpcServiceInfo, self);
    }
}

/**
 * epc_service_info_get_service_type:
 * @info: a #EpcServiceInfo
 *
 * Retrieves the DNS-SD service type associated with @info.
 *
 * Returns: A DNS-SD service type.
 */
const gchar*
epc_service_info_get_service_type (const EpcServiceInfo *self)
{
  g_return_val_if_fail (NULL != self, NULL);
  return self->type;
}

/**
 * epc_service_info_get_host:
 * @info: a #EpcServiceInfo
 *
 * Retrieves the DNS host name associated with @info.
 *
 * Returns: A DNS host name.
 */
const gchar*
epc_service_info_get_host (const EpcServiceInfo *self)
{
  g_return_val_if_fail (NULL != self, NULL);
  return self->host;
}

/**
 * epc_service_info_get_port:
 * @info: a #EpcServiceInfo
 *
 * Retrieves the TCP/IP port associated with @info.
 *
 * Returns: A TCP/IP port.
 */
guint
epc_service_info_get_port (const EpcServiceInfo *self)
{
  g_return_val_if_fail (NULL != self, 0);
  return self->port;
}

/**
 * epc_service_info_get_detail:
 * @info: a #EpcServiceInfo
 * @name: the detail's name
 *
 * Retrieves a detail stored in the service's TXT record.
 * Returns %NULL when the requested information is not available.
 *
 * Returns: The requested service detail, or %NULL.
 */
const gchar*
epc_service_info_get_detail (const EpcServiceInfo *self,
                             const gchar          *name)
{
  AvahiStringList *match = NULL;
  const gchar *detail = NULL;

  g_return_val_if_fail (NULL != self, NULL);
  g_return_val_if_fail (NULL != name, NULL);

  if (self->details)
    match = avahi_string_list_find (self->details, name);

  if (match)
    {
      gsize len = strlen (name);

      g_assert (!memcmp (match->text, name, len));

      if ('=' == match->text[len])
        detail = (gchar*) &match->text[len + 1];
    }

  return detail;
}

/**
 * epc_service_info_get_interface:
 * @info: a #EpcServiceInfo
 *
 * Retrieves the name of the network interface which must be used for
 * contacting the service, or %NULL when that information is not available.
 *
 * Returns: A network interface name, or %NULL.
 */
const gchar*
epc_service_info_get_interface (const EpcServiceInfo *self)
{
  g_return_val_if_fail (NULL != self, NULL);
  return self->ifname;
}

/**
 * epc_service_info_get_address_family:
 * @info: a #EpcServiceInfo
 *
 * Retrieves the address family for contacting the service,
 * or #G_SOCKET_FAMILY_INVALID when that information is not available.
 *
 * Returns: A #GSocketFamily.
 */
GSocketFamily
epc_service_info_get_address_family (const EpcServiceInfo *self)
{
  g_return_val_if_fail (NULL != self, G_SOCKET_FAMILY_INVALID);

  if (self->address)
    return avahi_proto_to_af (self->address->proto);

  return G_SOCKET_FAMILY_INVALID;
}

/**
 * epc_service_info_get_address:
 * @info: a #EpcServiceInfo
 *
 * Retrieves the IP address for contacting the service,
 * or %NULL when that information is not available.
 *
 * Returns: A IP address, or %NULL.
 */
const AvahiAddress*
epc_service_info_get_address (const EpcServiceInfo *self)
{
  g_return_val_if_fail (NULL != self, NULL);
  return self->address;
}


