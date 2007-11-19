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
#include "shell.h"

#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <gnutls/gnutls.h>

/**
 * SECTION:shell
 * @short_description: library management functions
 * @include: libepc/shell.h
 * @stability: Private
 *
 * The methods of the #EpcShell singleton are used to manage library resources.
 */

typedef struct _EpcAvahiShell EpcAvahiShell;

struct _EpcAvahiShell
{
  volatile gint  ref_count;
  AvahiGLibPoll *poll;

  void (*threads_enter) (void);
  void (*threads_leave) (void);
};

static EpcAvahiShell epc_shell = { 0, NULL, NULL, NULL };
gboolean _epc_debug = FALSE;

/**
 * epc_shell_ref:
 *
 * Increases the reference count for this library. When called for the first
 * time essential resources are allocated. Each call to this function has to
 * be paired with a call to #epc_shell_unref.
 */
void
epc_shell_ref (void)
{
  if (0 == g_atomic_int_exchange_and_add (&epc_shell.ref_count, 1))
    {
      GModule *module;
      gpointer symbol;

      if (g_getenv ("EPC_DEBUG"))
        _epc_debug = TRUE;

      gnutls_global_init ();
      avahi_set_allocator (avahi_glib_allocator ());

      g_assert (NULL == epc_shell.poll);
      epc_shell.poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);

      g_assert (NULL == epc_shell.threads_enter);
      g_assert (NULL == epc_shell.threads_leave);

      module = g_module_open (NULL, G_MODULE_BIND_LAZY);

      symbol = NULL;
      g_module_symbol (module, "gdk_threads_enter", &symbol);
      epc_shell.threads_enter = symbol;

      symbol = NULL;
      g_module_symbol (module, "gdk_threads_leave", &symbol);
      epc_shell.threads_leave = symbol;

      g_module_close (module);
    }

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: %d", G_STRFUNC, epc_shell.ref_count);
}

/**
 * epc_shell_unref:
 *
 * Decreases the reference count for this library. When the reference count
 * reaches zero, global resources allocated by the library are released.
 */
void
epc_shell_unref (void)
{
  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: %d", G_STRFUNC, epc_shell.ref_count);

  if (g_atomic_int_dec_and_test (&epc_shell.ref_count))
    {
      g_assert (NULL != epc_shell.poll);
      avahi_glib_poll_free (epc_shell.poll);
      epc_shell.poll = NULL;

      epc_shell.threads_enter = NULL;
      epc_shell.threads_leave = NULL;
    }
}

/**
 * epc_shell_leave:
 *
 * Releases the big GDK lock when running unter GDK/GTK+.
 * It has to be called before entering GLib main loops to avoid race
 * conditions. See #gdk_threads_leave for details.
 */
void
epc_shell_leave (void)
{
  if (epc_shell.threads_leave)
    epc_shell.threads_leave ();
}

/**
 * epc_shell_enter:
 *
 * Acquires the big GDK lock when running unter GDK/GTK+.
 * It has to be called after leaving GLib main loops to avoid race
 * conditions. See #gdk_threads_enter for details.
 */
void
epc_shell_enter (void)
{
  if (epc_shell.threads_enter)
    epc_shell.threads_enter ();
}

/**
 * epc_shell_get_avahi_poll_api:
 *
 * Returns the <citetitle>Avahi</citetitle> polling API object used for
 * integrating the <citetitle>Avahi</citetitle> library with the
 * <citetitle>GLib</citetitle> main loop.
 *
 * You have to call #epc_shell_ref before using this function.
 *
 * Returns: The Avahi polling API used.
 */
G_CONST_RETURN AvahiPoll*
epc_shell_get_avahi_poll_api (void)
{
  g_return_val_if_fail (NULL != epc_shell.poll, NULL);
  return avahi_glib_poll_get (epc_shell.poll);
}

/**
 * epc_shell_create_avahi_client:
 * @flags: options for creating the client
 * @callback: the function to call on status changes, or %NULL
 * @user_data: data to passed to @callback
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new Avahi client with the behavior described by @flags. Whenever
 * the state of the client changes @callback is called. This callback may be
 * %NULL. If the call was not successful, it returns %FALSE. The error domain
 * is EPC_AVAHI_ERROR. Possible error codes are those of the
 * <citetitle>Avahi</citetitle> library.
 *
 * You have to call #epc_shell_ref before using this function.
 *
 * <note><para>
 *  Please note that this function is called for the first time from within the
 *  #epc_shell_create_avahi_client context! Thus, in the callback you should
 *  not make use of global variables that are initialized only after your
 *  call to #epc_shell_create_avahi_client. A common mistake is to store the
 *  AvahiClient pointer returned by #epc_shell_create_avahi_client in a global
 *  variable and assume that this global variable already contains the valid
 *  pointer when the callback is called for the first time. A work-around for
 *  this is to always use the AvahiClient pointer passed to the callback
 *  function instead of the global pointer.
 * </para></note>
 *
 * Returns: The newly created Avahi client instance, or %NULL on error.
 */
AvahiClient*
epc_shell_create_avahi_client (AvahiClientFlags      flags,
                               AvahiClientCallback   callback,
                               gpointer              user_data,
			       GError	           **error)
{
  gint error_code = AVAHI_OK;
  AvahiClient *client;

  client = avahi_client_new (epc_shell_get_avahi_poll_api (),
                             flags, callback, user_data,
                             &error_code);

  if (!client)
    g_set_error (error, EPC_AVAHI_ERROR, error_code,
                 _("Cannot create Avahi client: %s"),
                 avahi_strerror (error_code));

  return client;
}

GQuark
epc_avahi_error_quark (void)
{
  return g_quark_from_static_string ("epc-avahi-error-quark");
}
