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
#ifndef __EPC_SHELL_H__
#define __EPC_SHELL_H__

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>

#include <glib.h>

G_BEGIN_DECLS

/**
 * EPC_DEBUG_LEVEL:
 * @level: the minimum level to check for
 *
 * Checks if the library's debug level, which can be modified by setting the
 * <varname>EPC_DEBUG</varname> environment variable, is above the value
 * sepecified by @level.
 *
 * Returns: %TRUE if the library's debug level is above the threshold specified
 * by @level, and %FALSE otherwise.
 */
#define EPC_DEBUG_LEVEL(level) G_UNLIKELY(epc_shell_get_debug_level () >= (level))

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
 * allowing the library to provide feedback during processing.
 *
 * See also: #epc_progress_window_install
 */
struct _EpcShellProgressHooks
{
  /*< public >*/
  void (*begin)  (const gchar *title,
                  gpointer     user_data);
  void (*update) (gdouble      percentage,
                  const gchar *message,
                  gpointer     user_data);
  void (*end)    (gpointer     user_data);

  /*< private >*/
  gpointer reserved1;
  gpointer reserved2;
  gpointer reserved3;
  gpointer reserved4;
};

guint                 epc_shell_get_debug_level          (void) G_GNUC_CONST;

void                  epc_shell_watch_remove             (guint id);

guint                 epc_shell_watch_avahi_client_state (AvahiClientCallback          callback,
                                                          gpointer                     user_data,
                                                          GDestroyNotify               destroy_data,
                                                          GError                     **error);
AvahiEntryGroup*      epc_shell_create_avahi_entry_group (AvahiEntryGroupCallback      callback,
                                                          gpointer                     user_data);
AvahiServiceBrowser*  epc_shell_create_service_browser   (AvahiIfIndex                 interface,
                                                          AvahiProtocol                protocol,
                                                          const gchar                 *type,
                                                          const gchar                 *domain,
                                                          AvahiLookupFlags             flags,
                                                          AvahiServiceBrowserCallback  callback,
                                                          gpointer                     user_data,
                                                          GError                     **error);
void                  epc_shell_restart_avahi_client     (const gchar                 *strloc);

G_CONST_RETURN gchar* epc_shell_get_host_name            (GError                     **error);

void                  epc_shell_set_progress_hooks       (const EpcShellProgressHooks *hooks,
                                                          gpointer                     user_data,
                                                          GDestroyNotify               destroy_data);
void                  epc_shell_progress_begin           (const gchar                 *title,
                                                          const gchar                 *message);
void                  epc_shell_progress_update          (gdouble                      percentage,
                                                          const gchar                 *message);
void                  epc_shell_progress_end             (void);

GQuark                epc_avahi_error_quark              (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __EPC_SHELL_H__ */
