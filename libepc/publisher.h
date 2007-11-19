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
#ifndef __EPC_PUBLISHER_H__
#define __EPC_PUBLISHER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EPC_TYPE_PUBLISHER           (epc_publisher_get_type())
#define EPC_PUBLISHER(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_PUBLISHER, EpcPublisher))
#define EPC_PUBLISHER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_PUBLISHER, EpcPublisherClass))
#define EPC_IS_PUBLISHER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_PUBLISHER))
#define EPC_IS_PUBLISHER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_PUBLISHER))
#define EPC_PUBLISHER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_PUBLISHER, EpcPublisherClass))

typedef struct _EpcPublisher        EpcPublisher;
typedef struct _EpcPublisherClass   EpcPublisherClass;
typedef struct _EpcPublisherPrivate EpcPublisherPrivate;

/**
 * EpcPublisher:
 *
 * Public fields of the #EpcPublisher class.
 */
struct _EpcPublisher
{
  /*< private >*/
  GObject parent_instance;
  EpcPublisherPrivate *priv;
};

/**
 * EpcPublisherClass:
 * @parent_class: virtual methods of the base class
 *
 * Virtual methods of the #EpcPublisher class.
 */
struct _EpcPublisherClass
{
  /*< private >*/
  GObjectClass parent_class;
};

GType                epc_publisher_get_type    (void) G_GNUC_CONST;

EpcPublisher*        epc_publisher_new         (const gchar   *name,
                                                const gchar   *service,
                                                const gchar   *domain);

void                 epc_publisher_set_name    (EpcPublisher  *publisher,
                                                const gchar   *name);
G_CONST_RETURN char* epc_publisher_get_name    (EpcPublisher  *publisher);
G_CONST_RETURN char* epc_publisher_get_domain  (EpcPublisher  *publisher);
G_CONST_RETURN char* epc_publisher_get_service (EpcPublisher  *publisher);

void                 epc_publisher_add         (EpcPublisher  *publisher,
                                                const gchar   *key,
                                                const gchar   *value,
                                                gssize         length);
gboolean             epc_publisher_add_file    (EpcPublisher  *publisher,
                                                const gchar   *key,
                                                const gchar   *filename,
                                                GError       **error);

void                 epc_publisher_run         (EpcPublisher  *publisher);
void                 epc_publisher_run_async   (EpcPublisher  *publisher);
void                 epc_publisher_quit        (EpcPublisher  *publisher);

G_END_DECLS

#endif /* __EPC_PUBLISHER_H__ */
