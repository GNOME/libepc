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
#ifndef __EPC_AVAHI_SHELL_H__
#define __EPC_AVAHI_SHELL_H__

#include <glib.h>
#include <avahi-client/client.h>

G_BEGIN_DECLS

void                        epc_avahi_shell_ref                 (void);
void                        epc_avahi_shell_unref               (void);

G_CONST_RETURN AvahiPoll*   epc_avahi_shell_get_poll_api        (void);
AvahiClient*                epc_avahi_shell_create_client       (AvahiClientFlags     flags,
                                                                 AvahiClientCallback  callback,
                                                                 gpointer             user_data);

gchar*                      epc_avahi_shell_create_service_type (const gchar         *basename);

G_END_DECLS

#endif /* __EPC_AVAHI_SHELL_H__ */
