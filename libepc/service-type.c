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

/**
 * SECTION:service-type
 * @short_description: DNS-SD service types of this library
 *
 * DNS-SD uses well-known services types to discover service providers.
 * The following macros describe the service types uses by this library.
 *
 * <example>
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

gchar*
epc_service_type_build_uri (EpcProtocol  protocol,
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

EpcProtocol
epc_service_type_get_protocol (const gchar *service_type)
{
  g_return_val_if_fail (NULL != service_type, EPC_PROTOCOL_UNKNOWN);

  if (g_str_equal (service_type, EPC_SERVICE_TYPE_HTTPS))
    return EPC_PROTOCOL_HTTPS;
  if (g_str_equal (service_type, EPC_SERVICE_TYPE_HTTP))
    return EPC_PROTOCOL_HTTP;

  return EPC_PROTOCOL_UNKNOWN;
}

G_CONST_RETURN gchar*
epc_protocol_get_service_type (EpcProtocol  protocol)
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

G_CONST_RETURN gchar*
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

