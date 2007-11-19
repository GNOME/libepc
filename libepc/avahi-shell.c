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
#include "avahi-shell.h"

#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

typedef struct _EpcAvahiShell EpcAvahiShell;

struct _EpcAvahiShell
{
  volatile gint  ref_count;
  AvahiGLibPoll *poll;
};

static EpcAvahiShell epc_avahi_shell;

void
epc_avahi_shell_ref (void)
{
  if (0 == g_atomic_int_exchange_and_add (&epc_avahi_shell.ref_count, 1))
    {
      avahi_set_allocator (avahi_glib_allocator ());

      g_assert (NULL == epc_avahi_shell.poll);
      epc_avahi_shell.poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
    }
}

void
epc_avahi_shell_unref (void)
{
  if (g_atomic_int_dec_and_test (&epc_avahi_shell.ref_count))
    {
      g_assert (NULL != epc_avahi_shell.poll);
      avahi_glib_poll_free (epc_avahi_shell.poll);
      epc_avahi_shell.poll = NULL;
    }
}

G_CONST_RETURN AvahiPoll*
epc_avahi_shell_get_poll_api (void)
{
  g_return_val_if_fail (NULL != epc_avahi_shell.poll, NULL);
  return avahi_glib_poll_get (epc_avahi_shell.poll);
}

AvahiClient*
epc_avahi_shell_create_client (AvahiClientFlags    flags,
                               AvahiClientCallback callback,
                               gpointer            user_data)
{
  gint error = AVAHI_OK;
  AvahiClient *client;

  client = avahi_client_new (epc_avahi_shell_get_poll_api (),
                             flags, callback, user_data, &error);

  if (!client)
    g_warning ("%s: Failed to create Avahi client: %s (%d)\n",
               G_STRLOC, avahi_strerror (error), error);

  return client;
}
