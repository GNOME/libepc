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
#ifndef __EPC_SHELL_H__
#define __EPC_SHELL_H__

#include <glib.h>
#include <avahi-client/client.h>

G_BEGIN_DECLS

/**
 * EPC_AVAHI_ERROR:
 *
 * Error domain for <citetitle>Avahi</citetitle> operations. Errors in this
 * domain will be <citetitle>Avahi</citetitle> error codes. See GError
 * for information on error domains.
 */
#define EPC_AVAHI_ERROR (epc_avahi_error_quark ())

typedef struct _EpcShellProgressHooks EpcShellProgressHooks;

/**
 * EpcShellProgressHooks:
 * @begin: the function called by #epc_shell_progress_begin
 * @update: the function called by #epc_shell_progress_update
 * @end: the function called by #epc_shell_progress_end
 *
 * This table is used by #epc_shell_set_progress_hooks to install functions
 * allowing the library to provide feedback on lengthly operations.
 *
 * See also: #epc_progress_window_install
 */
struct _EpcShellProgressHooks
{
  /*< public >*/
  gpointer (*begin)  (const gchar *title,
                      const gchar *message,
                      gpointer     user_data);
  void     (*update) (gpointer     context,
                      gdouble      percentage,
                      const gchar *message);
  void     (*end)    (gpointer     context);

  /*< private >*/
  gpointer reserved1;
  gpointer reserved2;
  gpointer reserved3;
  gpointer reserved4;
  gpointer reserved5;
};

void                        epc_shell_ref                 (void);
void                        epc_shell_unref               (void);
void                        epc_shell_leave               (void);
void                        epc_shell_enter               (void);

G_CONST_RETURN AvahiPoll*   epc_shell_get_avahi_poll_api  (void);
AvahiClient*                epc_shell_create_avahi_client (AvahiClientFlags             flags,
                                                           AvahiClientCallback          callback,
                                                           gpointer                     user_data,
                                                           GError                     **error);

void                        epc_shell_set_progress_hooks  (const EpcShellProgressHooks *hooks,
                                                           gpointer                     user_data,
                                                           GDestroyNotify               destroy_data);

gpointer                    epc_shell_progress_begin      (const gchar                 *title,
                                                           const gchar                 *message);
void                        epc_shell_progress_update     (gpointer                     context,
                                                           gdouble                      percentage,
                                                           const gchar                 *message);
void                        epc_shell_progress_end        (gpointer                     context);

GQuark                      epc_avahi_error_quark         (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __EPC_SHELL_H__ */
