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
#ifndef __EPC_CONSUMER_H__
#define __EPC_CONSUMER_H__

#include <glib-object.h>
#include <libepc/service-type.h>

G_BEGIN_DECLS

/**
 * EPC_HTTP_ERROR:
 *
 * Error domain for HTTP operations. Errors in this domain will
 * be from the #SoupKnownStatusCode enumeration. See GError for
 * information on error domains.
 */
#define EPC_HTTP_ERROR              (epc_http_error_quark())

#define EPC_TYPE_CONSUMER           (epc_consumer_get_type())
#define EPC_CONSUMER(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_CONSUMER, EpcConsumer))
#define EPC_CONSUMER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_CONSUMER, EpcConsumerClass))
#define EPC_IS_CONSUMER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_CONSUMER))
#define EPC_IS_CONSUMER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_CONSUMER))
#define EPC_CONSUMER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_CONSUMER, EpcConsumerClass))

/**
 * EPC_CONSUMER_DEFAULT_TIMEOUT:
 *
 * The timeout used when calling #epc_consumer_resolve_publisher internally.
 */
#define EPC_CONSUMER_DEFAULT_TIMEOUT 5000

typedef struct _EpcConsumer        EpcConsumer;
typedef struct _EpcConsumerClass   EpcConsumerClass;
typedef struct _EpcConsumerPrivate EpcConsumerPrivate;

/**
 * EpcConsumer:
 *
 * Public fields of the #EpcConsumer class.
 */
struct _EpcConsumer
{
  /*< private >*/
  GObject parent_instance;
  EpcConsumerPrivate *priv;

  /*< public >*/
};

/**
 * EpcConsumerClass:
 * @authenticate: virtual method of the #EpcConsumer::authenticate signal
 * @reauthenticate: virtual method of the #EpcConsumer::re-authenticate signal
 * @publisher_resolved: virtual method of the #EpcConsumer::publisher-resolved signal
 *
 * Virtual methods of the #EpcConsumer class.
 */
struct _EpcConsumerClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  void (*authenticate)       (EpcConsumer  *consumer,
                              const gchar  *realm,
                              gchar       **username,
                              gchar       **password);

  void (*reauthenticate)     (EpcConsumer  *consumer,
                              const gchar  *realm,
                              gchar       **username,
                              gchar       **password);

  void (*publisher_resolved) (EpcConsumer  *consumer,
                              EpcProtocol   protocol,
                              const gchar  *hostname,
                              guint         port);
};

GType                 epc_consumer_get_type          (void) G_GNUC_CONST;

EpcConsumer*          epc_consumer_new               (EpcProtocol   protocol,
                                                      const gchar  *hostname,
                                                      guint16       port);
EpcConsumer*          epc_consumer_new_for_name      (const gchar  *name);
EpcConsumer*          epc_consumer_new_for_name_full (const gchar  *name,
                                                      const gchar  *application,
                                                      const gchar  *domain);

void                  epc_consumer_set_protocol      (EpcConsumer  *consumer,
                                                      EpcProtocol   protocol);
EpcProtocol           epc_consumer_get_protocol      (EpcConsumer  *consumer);

gboolean              epc_consumer_resolve_publisher (EpcConsumer  *consumer,
                                                      guint         timeout);

gchar*                epc_consumer_lookup            (EpcConsumer  *consumer,
                                                      const gchar  *key,
                                                      gsize        *length,
                                                      GError      **error);
GList*                epc_consumer_list              (EpcConsumer  *consumer,
                                                      const gchar  *pattern,
                                                      GError      **error);

GQuark                epc_http_error_quark           (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __EPC_CONSUMER_H__ */
