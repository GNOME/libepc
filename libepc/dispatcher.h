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

#include <libepc/service-info.h>

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
 * EpcCollisionHandling:
 * @EPC_COLLISIONS_IGNORE: Don't handle collisions at all, just fail silently.
 * @EPC_COLLISIONS_CHANGE_NAME: Try to announce the service with another name.
 * @EPC_COLLISIONS_UNIQUE_SERVICE: Defer own service announcement until the other service.
 * disappears.
 *
 * Various strategies for handling service name collisions.
 */
typedef enum
{
  EPC_COLLISIONS_IGNORE,
  EPC_COLLISIONS_CHANGE_NAME,
  EPC_COLLISIONS_UNIQUE_SERVICE
}
EpcCollisionHandling;

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

  /*< public >*/
};

/**
 * EpcDispatcherClass:
 *
 * Virtual methods of the #EpcDispatcher class.
 */
struct _EpcDispatcherClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
};

GType                 epc_dispatcher_get_type               (void) G_GNUC_CONST;

EpcDispatcher*        epc_dispatcher_new                    (const gchar          *name);
gboolean              epc_dispatcher_run                    (EpcDispatcher        *dispatcher,
                                                             GError              **error);
void                  epc_dispatcher_reset                  (EpcDispatcher        *dispatcher);

void                  epc_dispatcher_add_service            (EpcDispatcher        *dispatcher,
                                                             EpcAddressFamily      protocol,
                                                             const gchar          *type,
                                                             const gchar          *domain,
                                                             const gchar          *host,
                                                             guint16               port,
                                                                                   ...)
                                                             G_GNUC_NULL_TERMINATED;
void                  epc_dispatcher_add_service_subtype    (EpcDispatcher        *dispatcher,
                                                             const gchar          *type,
                                                             const gchar          *subtype);
void                  epc_dispatcher_set_service_details    (EpcDispatcher        *dispatcher,
                                                             const gchar          *type,
                                                                                   ...)
                                                             G_GNUC_NULL_TERMINATED;

void                  epc_dispatcher_set_name               (EpcDispatcher        *dispatcher,
                                                             const gchar          *name);
void                  epc_dispatcher_set_cookie             (EpcDispatcher        *dispatcher,
                                                             const gchar          *cookie);
void                  epc_dispatcher_set_collision_handling (EpcDispatcher        *dispatcher,
                                                             EpcCollisionHandling  method);

G_CONST_RETURN gchar* epc_dispatcher_get_name               (EpcDispatcher        *dispatcher);
EpcCollisionHandling  epc_dispatcher_get_collision_handling (EpcDispatcher        *dispatcher);
G_CONST_RETURN gchar* epc_dispatcher_get_cookie             (EpcDispatcher        *dispatcher);

G_END_DECLS

#endif /* __EPC_DISPATCHER_H__ */
