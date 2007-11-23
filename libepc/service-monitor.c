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

#include "service-monitor.h"

#include "marshal.h"
#include "service-type.h"

enum
{
  PROP_NONE,
  PROP_SERVICE_TYPES
};

enum
{
  SIGNAL_SERVICE_FOUND,
  SIGNAL_SERVICE_LOST,
  SIGNAL_LAST
};

struct _EpcServiceMonitorPrivate
{
  gchar **service_types;
};

static guint signals[SIGNAL_LAST];

G_DEFINE_TYPE (EpcServiceMonitor, epc_service_monitor, G_TYPE_OBJECT);

static void
epc_service_monitor_init (EpcServiceMonitor *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPC_TYPE_SERVICE_MONITOR,
                                            EpcServiceMonitorPrivate);
}

static void
epc_service_monitor_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (object);

  switch (prop_id)
    {
      case PROP_SERVICE_TYPES:
        g_assert (NULL == self->priv->service_types);
        self->priv->service_types = g_value_get_boxed (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_service_monitor_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (object);

  switch (prop_id)
    {
      case PROP_SERVICE_TYPES:
        g_value_set_boxed (value, self->priv->service_types);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_service_monitor_dispose (GObject *object)
{
  EpcServiceMonitor *self = EPC_SERVICE_MONITOR (object);

  if (self->priv->service_types)
    self->priv->service_types = (g_strfreev (self->priv->service_types), NULL);

  G_OBJECT_CLASS (epc_service_monitor_parent_class)->dispose (object);
}

static void
epc_service_monitor_class_init (EpcServiceMonitorClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);

  oclass->set_property = epc_service_monitor_set_property;
  oclass->get_property = epc_service_monitor_get_property;
  oclass->dispose = epc_service_monitor_dispose;

  g_object_class_install_property (oclass, PROP_SERVICE_TYPES,
                                   g_param_spec_boxed ("service-types", "Service Types",
                                                       "The DNS-SD services types to watch",
                                                      G_TYPE_STRV,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                      G_PARAM_STATIC_BLURB));

  signals[SIGNAL_SERVICE_FOUND] = g_signal_new ("service-found",
                                                EPC_TYPE_SERVICE_MONITOR, G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET (EpcServiceMonitorClass, service_found),
                                                NULL, NULL,
                                                _epc_marshal_VOID__STRING_STRING_STRING_UINT,
                                                G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING,
                                                                G_TYPE_STRING, G_TYPE_UINT);

  signals[SIGNAL_SERVICE_LOST] = g_signal_new ("service-lost",
                                               EPC_TYPE_SERVICE_MONITOR, G_SIGNAL_RUN_FIRST,
                                               G_STRUCT_OFFSET (EpcServiceMonitorClass, service_lost),
                                               NULL, NULL,
                                               _epc_marshal_VOID__STRING_STRING_STRING_UINT,
                                               G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING,
                                                               G_TYPE_STRING, G_TYPE_UINT);

  g_type_class_add_private (cls, sizeof (EpcServiceMonitorPrivate));
}

EpcServiceMonitor*
epc_service_monitor_new (const gchar *first_service_type,
                                      ...)
{
  gchar **service_types = NULL;
  va_list args;
  gint i;

  for (i = 0; i < 2; ++i)
    {
      const gchar *type = first_service_type;
      gint tail = 0;

      va_start (args, first_service_type);

      while (NULL != type)
        {
          type = va_arg (args, const gchar*);

          if (service_types)
            service_types[tail] = g_strdup (type);

          tail += 1;
        }

      va_end (args);

      if (NULL == service_types)
        service_types = g_new0 (gchar*, tail + 1);
    }

  return g_object_new (EPC_TYPE_SERVICE_MONITOR,
                       "service-types", service_types,
                       NULL);
}

EpcServiceMonitor*
epc_service_monitor_new_for_application (const gchar *application,
                                         EpcProtocol  first_protocol,
                                         ...)
{
  gchar **service_types = NULL;
  va_list args;
  gint i;

  for (i = 0; i < 2; ++i)
    {
      EpcProtocol protocol = first_protocol;
      gint tail = 0;

      va_start (args, first_protocol);

      while (((gint) protocol) > EPC_PROTOCOL_UNKNOWN)
        {
          protocol = va_arg (args, EpcProtocol);

          if (service_types)
            service_types[tail] = epc_service_type_new (protocol, application);

          tail += 1;
        }

      va_end (args);

      if (NULL == service_types)
        service_types = g_new0 (gchar*, tail + 1);
    }

  return g_object_new (EPC_TYPE_SERVICE_MONITOR,
                       "service-types", service_types,
                       NULL);
}

