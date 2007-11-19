#include "dispatcher.h"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-glib/glib-watch.h>

enum
{
  PROP_NONE,
  PROP_NAME,
  PROP_DOMAIN,
  PROP_SERVICE
};

struct _EpcDispatcherPrivate
{
  AvahiGLibPoll *poll;
  AvahiClient *client;
  AvahiEntryGroup *group;
  guint startup_handler;

  gchar *name;
  gchar *domain;
  gchar *service;
};

G_DEFINE_TYPE (EpcDispatcher, epc_dispatcher, G_TYPE_OBJECT);

static void
epc_dispatcher_init (EpcDispatcher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPC_TYPE_DISPATCHER,
                                            EpcDispatcherPrivate);
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
        g_free (self->priv->name);
        self->priv->name = g_value_dup_string (value);
        break;

      case PROP_DOMAIN:
        g_free (self->priv->domain);
        self->priv->domain = g_value_dup_string (value);
        break;

      case PROP_SERVICE:
        g_free (self->priv->service);
        self->priv->service = g_value_dup_string (value);
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
      case PROP_NAME:
        g_value_set_string (value, self->priv->name);
        break;

      case PROP_DOMAIN:
        g_value_set_string (value, self->priv->domain);
        break;

      case PROP_SERVICE:
        g_value_set_string (value, self->priv->service);
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

  epc_dispatcher_quit (self);

  if (self->priv->poll)
    {
      avahi_glib_poll_free (self->priv->poll);
      self->priv->poll = NULL;
    }

  g_free (self->priv->name);
  self->priv->name = NULL;

  g_free (self->priv->domain);
  self->priv->domain = NULL;

  g_free (self->priv->service);
  self->priv->service = NULL;

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
                                                        "User friendly name for this service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_DOMAIN,
                                   g_param_spec_string ("domain", "Domain",
                                                        "Internet domain for publishing this service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (oclass, PROP_SERVICE,
                                   g_param_spec_string ("service", "Service",
                                                        "Wellknown DNS-SD name this service", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcDispatcherPrivate));
}

EpcDispatcher*
epc_dispatcher_new (const gchar *name,
                    const gchar *domain,
                    const gchar *service)
{
  g_return_val_if_fail (NULL != name, NULL);
  g_return_val_if_fail (NULL != service, NULL);

  return g_object_new (EPC_TYPE_DISPATCHER, "name", name,
                       "domain", domain, "service", service,
                       NULL);
}

void
epc_dispatcher_set_name (EpcDispatcher *self,
                         const gchar   *name)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_object_set (self, "name", name, NULL);
}

G_CONST_RETURN char*
epc_dispatcher_get_name (EpcDispatcher *self)
{
  g_return_val_if_fail (EPC_IS_DISPATCHER (self), NULL);
  return self->priv->name;
}

void
epc_dispatcher_set_domain (EpcDispatcher *self,
                           const gchar   *domain)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_object_set (self, "domain", domain, NULL);
}

G_CONST_RETURN char*
epc_dispatcher_get_domain (EpcDispatcher *self)
{
  g_return_val_if_fail (EPC_IS_DISPATCHER (self), NULL);
  return self->priv->domain;
}

void
epc_dispatcher_set_service (EpcDispatcher *self,
                            const gchar   *service)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));
  g_object_set (self, "service", service, NULL);
}

G_CONST_RETURN char*
epc_dispatcher_get_service (EpcDispatcher *self)
{
  g_return_val_if_fail (EPC_IS_DISPATCHER (self), NULL);
  return self->priv->service;
}

void
epc_dispatcher_quit (EpcDispatcher *self)
{
  g_return_if_fail (EPC_IS_DISPATCHER (self));

  if (self->priv->startup_handler)
    {
      g_source_remove (self->priv->startup_handler);
      self->priv->startup_handler = 0;
    }

  if (self->priv->group)
    {
      avahi_entry_group_free (self->priv->group);
      self->priv->group = NULL;
    }

  if (self->priv->client)
    {
      avahi_client_free (self->priv->client);
      self->priv->client = NULL;
    }
}
