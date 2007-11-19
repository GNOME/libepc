#ifndef __EPC_DISPATCHER_H__
#define __EPC_DISPATCHER_H__

#include <glib-object.h>

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

struct _EpcDispatcher
{
  GObject parent_instance;
  EpcDispatcherPrivate *priv;
};

struct _EpcDispatcherClass
{
  GObjectClass parent_class;
};

GType                epc_dispatcher_get_type    (void) G_GNUC_CONST;

EpcDispatcher*       epc_dispatcher_new         (const gchar   *name,
                                                 const gchar   *domain,
                                                 const gchar   *service);

void                 epc_dispatcher_set_name    (EpcDispatcher *dispatcher,
                                                 const gchar   *name);
G_CONST_RETURN char* epc_dispatcher_get_name    (EpcDispatcher *dispatcher);

void                 epc_dispatcher_set_domain  (EpcDispatcher *dispatcher,
                                                 const gchar   *domain);
G_CONST_RETURN char* epc_dispatcher_get_domain  (EpcDispatcher *dispatcher);

void                 epc_dispatcher_set_service (EpcDispatcher *dispatcher,
                                                 const gchar   *service);
G_CONST_RETURN char* epc_dispatcher_get_service (EpcDispatcher *dispatcher);

void                 epc_dispatcher_quit        (EpcDispatcher *dispatcher);

G_END_DECLS

#endif /* __EPC_DISPATCHER_H__ */
