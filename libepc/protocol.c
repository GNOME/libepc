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

#include "libepc/protocol.h"
#include "libepc/service-type.h"
#include "libepc/enums.h"

#include <glib-object.h>

/**
 * SECTION:protocol
 * @short_description: transport protocols
 * @see_also: epc_service_type_new()
 * @include: libepc/protcol.h
 * @stability: Unstable
 *
 * Since the Easy Publish and Consume library hides the details of the
 * transport mechanism used, it is possible to support different mechanisms.
 * Currently there is support for HTTP and HTTPS.
 *
 * The #EpcProtocol enumeration is the maximum of information libepc wants to
 * expose regarding its transport mechanisms.
 */

/**
 * epc_protocol_build_uri:
 * @protocol: a #EpcProtocol
 * @hostname: the host to contact
 * @port: the service port
 * @path: the service path, or %NULL
 *
 * Builds the Unified Resource Identifier (URI) for a service.
 * The returned string should be released when no longer needed.
 *
 * Returns: A newly allocated string with the URI for the service,
 * or %NULL on error.
 */
gchar*
epc_protocol_build_uri (EpcProtocol  protocol,
                        const gchar *hostname,
                        guint16      port,
                        const gchar *path)
{
  const gchar *scheme;

  if (NULL == path)
    path = "/";

  g_return_val_if_fail (NULL != hostname, NULL);
  g_return_val_if_fail ('/' == path[0], NULL);
  g_return_val_if_fail (port > 0, NULL);

  scheme = epc_protocol_get_uri_scheme (protocol);
  g_return_val_if_fail (NULL != scheme, NULL);

  return g_strdup_printf ("%s://%s:%d/%s", scheme, hostname, port, path + 1);
}

/**
 * epc_protocol_from_name:
 * @name: a protocol name
 * @fallback: the #EpcProtocol to use on errors
 *
 * Parses the protocol @name. Case of the name doesn't matter. Returns the
 * matching #EpcProtocol, when the name was recognized, and the value of
 * @fallback otherwise.
 *
 * Returns: The #EpcProtocol matching @name, or @fallback on error.
 */
EpcProtocol
epc_protocol_from_name (const gchar *name,
                        EpcProtocol  fallback)
{
  static GEnumClass *cls = NULL;
  GEnumValue *result;
  gchar *lower;

  g_return_val_if_fail (NULL != name, fallback);

  if (G_UNLIKELY (NULL == cls))
    cls = g_type_class_ref (EPC_TYPE_PROTOCOL);

  lower = g_utf8_strdown (name, -1);
  result = g_enum_get_value_by_nick (cls, lower);
  g_free (lower);

  if (result && EPC_PROTOCOL_UNKNOWN != result->value)
    return result->value;

  return fallback;
}

/**
 * epc_protocol_get_service_type:
 * @protocol: a #EpcProtocol
 *
 * Queries the DNS-SD service type associated with a #EpcProtocol.
 * See #EPC_SERVICE_TYPE_HTTP, #EPC_SERVICE_TYPE_HTTPS.
 *
 * Returns: Returns the DNS-SD service type associated
 * with @protocol, or %NULL on unknown protocols.
 */
const gchar*
epc_protocol_get_service_type (EpcProtocol protocol)
{
  switch (protocol)
    {
      case EPC_PROTOCOL_HTTPS:
        return EPC_SERVICE_TYPE_HTTPS;

      case EPC_PROTOCOL_HTTP:
        return EPC_SERVICE_TYPE_HTTP;

      case EPC_PROTOCOL_UNKNOWN:
        return NULL;
    }

  g_return_val_if_reached (NULL);
}

/**
 * epc_protocol_get_uri_scheme:
 * @protocol: a #EpcProtocol
 *
 * Queries the URI scheme associated with a #EpcProtocol.
 * See epc_service_type_build_uri().
 *
 * Returns: Returns the URI scheme associated with @protocol,
 * or %NULL on unknown protocols.
 */
const gchar*
epc_protocol_get_uri_scheme (EpcProtocol  protocol)
{
  switch (protocol)
    {
      case EPC_PROTOCOL_HTTPS:
        return "https";

      case EPC_PROTOCOL_HTTP:
        return "http";

      case EPC_PROTOCOL_UNKNOWN:
        return NULL;
    }

  g_return_val_if_reached (NULL);
}
