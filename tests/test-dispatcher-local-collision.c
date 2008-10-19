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

/* Test handling of local DNS-DS service collisions */

#include "framework.h"
#include "libepc/dispatcher.h"

#include <avahi-common/alternative.h>
#include <string.h>

static gchar *test_type = NULL;
static gchar *preferred_name = NULL;
static gchar *alternative_name = NULL;

static void
service_browser_cb (AvahiServiceBrowser     *browser G_GNUC_UNUSED,
                    AvahiIfIndex             interface G_GNUC_UNUSED,
                    AvahiProtocol            protocol G_GNUC_UNUSED,
                    AvahiBrowserEvent        event,
                    const char              *name,
                    const char              *type,
                    const char              *domain G_GNUC_UNUSED,
                    AvahiLookupResultFlags   flags,
                    void                    *data G_GNUC_UNUSED)
{
  if (AVAHI_BROWSER_NEW == event &&
      0 != (flags & AVAHI_LOOKUP_RESULT_LOCAL) &&
      type && g_str_equal (type, test_type) && name)
    {
      if (g_str_equal (name, preferred_name))
        epc_test_pass_once (1);
      if (g_str_equal (name, alternative_name))
        epc_test_pass_once (2);
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher1 = NULL;
  EpcDispatcher *dispatcher2 = NULL;
  int result = EPC_TEST_MASK_ALL;
  GError *error = NULL;

  g_type_init ();

  test_type = g_strdup_printf ("_test-%08x._tcp", g_random_int ());
  preferred_name = g_strdup_printf ("%s: %08x", __FILE__, g_random_int ());
  alternative_name = avahi_alternative_service_name (preferred_name);

  g_assert (!g_str_equal (preferred_name, alternative_name));

  if (epc_test_init (2) &&
      epc_test_init_service_browser (test_type, service_browser_cb, NULL))
    {
      dispatcher1 = epc_dispatcher_new (preferred_name);

      g_assert (EPC_COLLISIONS_CHANGE_NAME ==
                epc_dispatcher_get_collision_handling (dispatcher1));

      if (!epc_dispatcher_run (dispatcher1, &error))
        goto out;

      epc_dispatcher_add_service (dispatcher1, EPC_ADDRESS_UNSPEC,
                                  test_type, NULL, NULL, 2007, NULL);

      dispatcher2 = epc_dispatcher_new (preferred_name);

      g_assert (EPC_COLLISIONS_CHANGE_NAME ==
                epc_dispatcher_get_collision_handling (dispatcher2));

      if (!epc_dispatcher_run (dispatcher2, &error))
        goto out;

      epc_dispatcher_add_service (dispatcher2, EPC_ADDRESS_UNSPEC,
                                  test_type, NULL, NULL, 2008, NULL);

      result = epc_test_run ();

      if (strcmp (alternative_name, epc_dispatcher_get_name (dispatcher2)))
        result |= 4;
      if (strcmp (preferred_name, epc_dispatcher_get_name (dispatcher1)))
        result |= 8;
    }

out:
  if (error)
    g_print ("%s: %s\n", G_STRLOC, error->message);

  g_clear_error (&error);

  if (dispatcher2)
    g_object_unref (dispatcher2);
  if (dispatcher1)
    g_object_unref (dispatcher1);

  g_free (alternative_name);
  g_free (preferred_name);
  g_free (test_type);

  return result;
}
