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
#ifndef __EPC_DISPATCHER_H__
#define __EPC_DISPATCHER_H__

#include <glib-object.h>
#include <sys/socket.h>

G_BEGIN_DECLS

#define EPC_TYPE_DISPATCHER           (epc_dispatcher_get_type())
#define EPC_DISPATCHER(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_DISPATCHER, EpcDispatcher))
#define EPC_DISPATCHER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_DISPATCHER, EpcDispatcherClass))
#define EPC_IS_DISPATCHER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_DISPATCHER))
#define EPC_IS_DISPATCHER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_DISPATCHER))
#define EPC_DISPATCHER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_DISPATCHER, EpcDispatcherClass))

typedef struct _EpcDispatcher        EpcDispatcher;
typedef struct _EpcDispatcherClass   EpcDispatcherClass;
typedef struct _EpcDispatcherPrivate EpcDispatcherPrivate;

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

/**
 * EpcDispatcher:
 *
 * Public fields of the #EpcDispatcher class.
 */
struct _EpcDispatcher
{
  /*< private >*/
  GObject parent_instance;
  EpcDispatcherPrivate *priv;
};

/**
 * EpcDispatcherClass:
 * @parent_class: virtual methods of the base class
 *
 * Virtual methods of the #EpcDispatcher class.
 */
struct _EpcDispatcherClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GType                 epc_dispatcher_get_type            (void) G_GNUC_CONST;

EpcDispatcher*        epc_dispatcher_new                 (const gchar      *name);
void                  epc_dispatcher_reset               (EpcDispatcher    *dispatcher);

void                  epc_dispatcher_add_service         (EpcDispatcher    *dispatcher,
                                                          EpcAddressFamily  protocol,
                                                          const gchar      *type,
                                                          const gchar      *domain,
                                                          const gchar      *host,
                                                          guint16           port,
                                                                            ...)
                                                          G_GNUC_NULL_TERMINATED;
void                  epc_dispatcher_add_service_subtype (EpcDispatcher    *dispatcher,
                                                          const gchar       *type,
                                                          const gchar       *subtype);
void                  epc_dispatcher_set_service_details (EpcDispatcher     *dispatcher,
                                                          const gchar       *type,
                                                                             ...)
                                                          G_GNUC_NULL_TERMINATED;

void                  epc_dispatcher_set_name            (EpcDispatcher     *dispatcher,
                                                          const gchar       *name);

G_CONST_RETURN gchar* epc_dispatcher_get_host_name       (EpcDispatcher     *dispatcher);
G_CONST_RETURN gchar* epc_dispatcher_get_name            (EpcDispatcher     *dispatcher);

G_END_DECLS

#endif /* __EPC_DISPATCHER_H__ */
