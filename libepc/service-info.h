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

#ifndef __EPC_SERVICE_INFO_H__
#define __EPC_SERVICE_INFO_H__

#include <avahi-common/address.h>
#include <avahi-common/strlst.h>
#include <glib-object.h>
#include <sys/socket.h>

G_BEGIN_DECLS

#define EPC_TYPE_SERVICE_INFO    (epc_service_info_get_type())
#define EPC_IS_SERVICE_INFO(obj) (NULL != (obj))

typedef struct _EpcServiceInfo EpcServiceInfo;

/**
 * EpcAddressFamily:
 * @EPC_ADDRESS_UNSPEC: No preferences exist. Use all address families supported.
 * @EPC_ADDRESS_IPV4: Exclusively use IPv4 for addressing network services.
 * @EPC_ADDRESS_IPV6: Exclusively use IPv6 for addressing network services.
 *
 * The address family to use for contacting network services.
 */
typedef enum
{
  EPC_ADDRESS_UNSPEC = AF_UNSPEC,
  EPC_ADDRESS_IPV4 = AF_INET,
  EPC_ADDRESS_IPV6 = AF_INET6
}
EpcAddressFamily;

GType                        epc_service_info_get_type           (void) G_GNUC_CONST;

EpcServiceInfo*              epc_service_info_new                (const gchar           *type,
                                                                  const gchar           *host,
                                                                  guint                  port,
                                                                  const AvahiStringList *details);
EpcServiceInfo*              epc_service_info_new_full           (const gchar           *type,
                                                                  const gchar           *host,
                                                                  guint                  port,
                                                                  const AvahiStringList *details,
                                                                  const AvahiAddress    *address,
                                                                  const gchar           *ifname);

EpcServiceInfo*              epc_service_info_ref                (EpcServiceInfo        *info);
void                         epc_service_info_unref              (EpcServiceInfo        *info);

G_CONST_RETURN gchar*        epc_service_info_get_service_type   (const EpcServiceInfo  *info);
G_CONST_RETURN gchar*        epc_service_info_get_host           (const EpcServiceInfo  *info);
guint                        epc_service_info_get_port           (const EpcServiceInfo  *info);
G_CONST_RETURN gchar*        epc_service_info_get_detail         (const EpcServiceInfo  *info,
                                                                  const gchar           *name);

G_CONST_RETURN gchar*        epc_service_info_get_interface      (const EpcServiceInfo  *info);
EpcAddressFamily             epc_service_info_get_address_family (const EpcServiceInfo  *info);
G_CONST_RETURN AvahiAddress* epc_service_info_get_address        (const EpcServiceInfo  *info);

G_END_DECLS

#endif /* __EPC_SERVICE_INFO_H__ */ 
