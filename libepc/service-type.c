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

#include "service-type.h"
#include "enums.h"

#include <string.h>

/**
 * SECTION:service-type
 * @short_description: service type details
 * @see_also: #EpcConsumer, #EpcPublisher
 * @include: libepc/service-type.h
 * @stability: Unstable
 *
 * DNS-SD uses well-known services types to discover service providers.
 * The following macros describe the service types uses by this library.
 *
 * <example id="find-publisher">
 *  <title>Find an Easy-Publish server</title>
 *  <programlisting>
 *   dialog = aui_service_dialog_new ("Choose an Easy Publish Server", NULL,
 *                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                    GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT,
 *                                    NULL);
 *   aui_service_dialog_set_browse_service_types (AUI_SERVICE_DIALOG (dialog),
 *                                                EPC_SERVICE_TYPE_HTTPS,
 *                                                EPC_SERVICE_TYPE_HTTP,
 *                                                NULL);
 *
 *   if (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
 *     {
 *       const gint port = aui_service_dialog_get_port (AUI_SERVICE_DIALOG (dialog));
 *       const gchar *host = aui_service_dialog_get_host_name (AUI_SERVICE_DIALOG (dialog));
 *       const gchar *type = aui_service_dialog_get_service_type (AUI_SERVICE_DIALOG (dialog));
 *       EpcProtocol protocol = epc_service_type_get_protocol (type);
 *       ...
 *     }
 *  </programlisting>
 * </example>
 */

static gchar*
epc_service_type_normalize_name (const gchar *name,
                                 gssize       length)
{
  GError *error = NULL;
  gchar *normalized, *s;

  g_return_val_if_fail (NULL != name, NULL);

  normalized = g_convert (name, length,
                          "ASCII//TRANSLIT", "UTF-8",
                          NULL, NULL, &error);

  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error->message);
      g_error_free (error);
    }

  if (normalized)
    {
      for (s = normalized; *s; ++s)
        if (!g_ascii_isalnum (*s))
          *s = '-';
    }

  return normalized;
}

/**
 * epc_service_type_new:
 * @protocol: a #EpcProtocol
 * @application: the application name, or %NULL
 *
 * Builds the DNS-SD service type for the given transport @protocol and
 * application. When @application is %NULL, the application name is retrieved by
 * calling #g_get_prgname(). %NULL is returned in that case if #g_get_prgname() returns %NULL.
 *
 * The string returned should be released when no longer needed.
 *
 * Returns: A newly allocated string holding the requested service-type,
 * or %NULL when @application is %NULL and #g_get_prgname fails.
 */
gchar*
epc_service_type_new (EpcProtocol  protocol,
                      const gchar *application)
{
  const gchar *transport = NULL;
  gchar *service_type = NULL;
  gchar *normalized = NULL;

  transport = epc_protocol_get_service_type (protocol);
  g_return_val_if_fail (NULL != transport, NULL);

  if (!application)
    application = g_get_prgname ();

  if (!application)
    {
      g_warning ("%s: Cannot derive the DNS-SD service type, as no "
                 "application name was specified and g_get_prgname() "
                 "returns NULL. Consider calling g_set_prgname().",
                 G_STRFUNC);

      return NULL;
    }

  normalized = epc_service_type_normalize_name (application, -1);

  if (normalized)
    {
      service_type = g_strconcat ("_", normalized, "._sub.", transport, NULL);
      g_free (normalized);
    }

  return service_type;
}

/**
 * epc_service_type_get_base:
 * @type: a DNS-SD service-type
 *
 * Extracts the base service-type of a DNS-SD service-type.
 *
 * DNS-SD service types may contain a sub service type, for instance the
 * service-type "_anon._sub._ftp._tcp" contains the base-type "_ftp._tcp"
 * and the sub-type "_anon". This function extracts extracts the base-type.
 * The service type is returned unmodifed if it doesn't contain a sub-type.
 *
 * Returns: The base-service-type.
 */
G_CONST_RETURN gchar*
epc_service_type_get_base (const gchar *type)
{
  const gchar *base;

  g_return_val_if_fail (NULL != type, NULL);
  base = type + strlen (type);

  while (base > type && '.' != *(--base));
  while (base > type && '.' != *(--base));


  if (base > type)
    base += 1;

  return base;
}

/**
 * epc_service_type_get_protocol:
 * @service_type: a DNS-SD service type
 *
 * Queries the #EpcProtocol associated with a DNS-SD service type.
 * See #EPC_SERVICE_TYPE_HTTP, #EPC_SERVICE_TYPE_HTTPS.
 *
 * Returns: Returns the #EpcProtocol associated with @service_type,
 * or #EPC_PROTOCOL_UNKNOWN for unrecognized or unsupported service types.
 */
EpcProtocol
epc_service_type_get_protocol (const gchar *service_type)
{
  g_return_val_if_fail (NULL != service_type, EPC_PROTOCOL_UNKNOWN);

  service_type = epc_service_type_get_base (service_type);
  g_assert (NULL != service_type);

  if (g_str_equal (service_type, EPC_SERVICE_TYPE_HTTPS))
    return EPC_PROTOCOL_HTTPS;
  if (g_str_equal (service_type, EPC_SERVICE_TYPE_HTTP))
    return EPC_PROTOCOL_HTTP;

  return EPC_PROTOCOL_UNKNOWN;
}

/**
 * epc_service_type_list_supported:
 * @application: an application name, or %NULL
 *
 * Lists all service types supported by the library. When @application is %NULL
 * just the generic types, otherwise the service-subtypes for that application
 * are returned. The returned list is terminated by %NULL and must be released
 * by the caller with #g_strfreev.
 *
 * See also: #epc_service_type_new.
 *
 * Returns: The %NULL terminated list of all supported service types.
 */
gchar**
epc_service_type_list_supported (const gchar *application)
{
  GEnumClass *protocol_class = epc_protocol_get_class ();
  gchar **types = NULL;
  guint vi, ti;

  types = g_new0 (gchar*, protocol_class->n_values);

  for (vi = 0, ti = 0; vi < protocol_class->n_values; ++vi)
    {
      const EpcProtocol protocol = protocol_class->values[vi].value;

      if (EPC_PROTOCOL_UNKNOWN == protocol)
        continue;

      types[ti++] = application ?
        epc_service_type_new (protocol, application) :
        g_strdup (epc_protocol_get_service_type (protocol));
    }

  return types;
}


