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
#ifndef __EPC_SERVICE_NAMES_H__
#define __EPC_SERVICE_NAMES_H__

G_BEGIN_DECLS

/**
 * SECTION:service-names
 * @short_description: common DNS-SD service names
 *
 * DNS-SD uses well-known services names to discover service providers.
 * The following macros describe the service names uses by this library.
 *
 * <example>
 *  <title>Find an Easy-Publish server</title>
 *  <programlisting>
 *   dialog = aui_service_dialog_new ("Choose an Easy Publish Server", NULL,
 *                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                    GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT,
 *                                    NULL);
 *   aui_service_dialog_set_browse_service_types (AUI_SERVICE_DIALOG (dialog),
 *                                                EPC_SERVICE_NAME, NULL);
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

/**
 * EPC_SERVICE_NAME:
 *
 * The well-known DNS-SD service name for finding #EpcPublisher servers.
 */
#define EPC_SERVICE_NAME        "_easy-publish._tcp"

/**
 * EPC_SERVICE_NAME_HTTP:
 *
 * The well-known DNS-SD service name for #EpcPublisher servers
 * providing unencrypted HTTP access.
 */
#define EPC_SERVICE_NAME_HTTP   "_http._sub._easy-publish._tcp"

/**
 * EPC_SERVICE_NAME_HTTPS:
 *
 * The well-known DNS-SD service name for #EpcPublisher servers
 * providing encrypted HTTPS access.
 */
#define EPC_SERVICE_NAME_HTTPS  "_https._sub._easy-publish._tcp"

G_END_DECLS

#endif /* __EPC_SERVICE_NAMES_H__ */
