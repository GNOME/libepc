#ifndef __EPC_CONSUMER_H__
#define __EPC_CONSUMER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EPC_TYPE_CONSUMER           (epc_consumer_get_type())
#define EPC_CONSUMER(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_CONSUMER, EpcConsumer))
#define EPC_CONSUMER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_CONSUMER, EpcConsumerClass))
#define EPC_IS_CONSUMER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_CONSUMER))
#define EPC_IS_CONSUMER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_CONSUMER))
#define EPC_CONSUMER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_CONSUMER, EpcConsumerClass))

typedef struct _EpcConsumer        EpcConsumer;
typedef struct _EpcConsumerClass   EpcConsumerClass;
typedef struct _EpcConsumerPrivate EpcConsumerPrivate;

struct _EpcConsumer
{
  GObject parent_instance;
  EpcConsumerPrivate *priv;
};

struct _EpcConsumerClass
{
  GObjectClass parent_class;
};

GType epc_consumer_get_type      (void) G_GNUC_CONST;

EpcConsumer* epc_consumer_new    (const gchar *host,
                                  guint16      port);
gchar*       epc_consumer_lookup (EpcConsumer *consumer,
                                  const gchar *key,
                                  gsize       *length);

G_END_DECLS

#endif /* __EPC_CONSUMER_H__ */
