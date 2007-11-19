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
#include "service-names.h"

#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

#include <gmodule.h>

typedef struct _EpcAvahiShell EpcAvahiShell;

struct _EpcAvahiShell
{
  volatile gint  ref_count;
  AvahiGLibPoll *poll;

  void (*threads_enter) (void);
  void (*threads_leave) (void);
};

static EpcAvahiShell epc_shell = { 0, NULL, NULL, NULL };

void
epc_shell_ref (void)
{
  if (0 == g_atomic_int_exchange_and_add (&epc_shell.ref_count, 1))
    {
      GModule *module;
      gpointer symbol;

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
}

void
epc_shell_unref (void)
{
  if (g_atomic_int_dec_and_test (&epc_shell.ref_count))
    {
      g_assert (NULL != epc_shell.poll);
      avahi_glib_poll_free (epc_shell.poll);
      epc_shell.poll = NULL;

      epc_shell.threads_enter = NULL;
      epc_shell.threads_leave = NULL;
    }
}

void
epc_shell_leave (void)
{
  if (epc_shell.threads_leave)
    epc_shell.threads_leave ();
}

void
epc_shell_enter (void)
{
  if (epc_shell.threads_enter)
    epc_shell.threads_enter ();
}

G_CONST_RETURN AvahiPoll*
epc_shell_get_avahi_poll_api (void)
{
  g_return_val_if_fail (NULL != epc_shell.poll, NULL);
  return avahi_glib_poll_get (epc_shell.poll);
}

AvahiClient*
epc_shell_create_avahi_client (AvahiClientFlags    flags,
                               AvahiClientCallback callback,
                               gpointer            user_data)
{
  gint error = AVAHI_OK;
  AvahiClient *client;

  client = avahi_client_new (epc_shell_get_avahi_poll_api (),
                             flags, callback, user_data, &error);

  if (!client)
    g_warning ("%s: Failed to create Avahi client: %s (%d)\n",
               G_STRLOC, avahi_strerror (error), error);

  return client;
}

static gchar*
epc_shell_normalize_name (const gchar *name,
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
epc_shell_create_service_type (const gchar *basename)
{
  gchar *service_type = NULL;
  gchar *normalized = NULL;

  if (!basename)
    basename = g_get_prgname ();

  if (!basename)
    {
      g_warning ("%s: Cannot derive the DNS-SD service type, as no basename "
                 "was specified and g_get_prgname() returns NULL. Consider "
                 "calling g_set_prgname().", G_STRLOC);

      return NULL;
    }

  normalized = epc_shell_normalize_name (basename, -1);

  if (normalized)
    {
      service_type = g_strconcat ("_", normalized, "-app._sub.", EPC_SERVICE_NAME, NULL);
      g_free (normalized);
    }

  return service_type;
}


