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
#ifndef __EPC_PROTOCOL_H__
#define __EPC_PROTOCOL_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * EpcProtocol:
 * @EPC_PROTOCOL_UNKNOWN: Used when the transport protocol is not known yet.
 * @EPC_PROTOCOL_HTTP: Plain HTTP. Passwords are protected using HTTP digest
 * authentication, remaining data is transfered without any encryption.
 * @EPC_PROTOCOL_HTTPS: Encrypted HTTP. Attempts are made to use this
 * transport method when ever possible.
 *
 * The transport protocols supported by libepc.
 */
typedef enum
{
  EPC_PROTOCOL_UNKNOWN,
  EPC_PROTOCOL_HTTP,
  EPC_PROTOCOL_HTTPS
}
EpcProtocol;

EpcProtocol           epc_protocol_from_name        (const gchar  *name,
                                                     EpcProtocol   fallback);

gchar*                epc_protocol_build_uri        (EpcProtocol   protocol,
                                                     const gchar  *hostname,
                                                     guint16       port,
                                                     const gchar  *path);

G_CONST_RETURN gchar* epc_protocol_get_service_type (EpcProtocol   protocol) G_GNUC_CONST;
G_CONST_RETURN gchar* epc_protocol_get_uri_scheme   (EpcProtocol   protocol) G_GNUC_CONST;

G_END_DECLS

#endif /* __EPC_PROTOCOL_H__ */
