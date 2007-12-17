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

/* Test announcing multiple DNS-DS services */

#include "framework.h"
#include "libepc/dispatcher.h"
#include "libepc/service-type.h"

#include <string.h>

static gchar *test_types[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
static gchar *test_name = NULL;

static void
service_browser_cb (AvahiServiceBrowser     *browser G_GNUC_UNUSED,
                    AvahiIfIndex             interface G_GNUC_UNUSED,
                    AvahiProtocol            protocol G_GNUC_UNUSED,
                    AvahiBrowserEvent        event,
                    const char              *name G_GNUC_UNUSED,
                    const char              *type G_GNUC_UNUSED,
                    const char              *domain G_GNUC_UNUSED,
                    AvahiLookupResultFlags   flags,
                    void                    *data G_GNUC_UNUSED)
{
  if ((AVAHI_BROWSER_NEW == event ||
       AVAHI_BROWSER_ALL_FOR_NOW == event) && type)
    {
      unsigned i;

      if (0 != (flags & AVAHI_LOOKUP_RESULT_LOCAL) &&
          name && g_str_equal (name, test_name))
        {
          if (0 == strcmp (type, test_types[0] + 11))
            epc_test_pass_many (1);
        }
      else
        {
          for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
            if (g_str_equal (type, test_types[i]))
              epc_test_pass_once (2 << i);
        }
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher = NULL;
  gint result = EPC_TEST_MASK_ALL;
  gint hash = g_random_int ();
  const gchar *base_type;
  GError *error = NULL;
  unsigned i;

  g_type_init ();

  test_name = g_strdup_printf ("%s: %08x", __FILE__, hash);

  if (!epc_test_init (7))
    goto out;

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    {
      test_types[i] = g_strdup_printf ("_sub%d._sub._test-%08x._tcp", i, hash);

      g_assert (0 == strncmp ("._sub._test-", test_types[i] + 5, 12));
      g_assert (0 == strncmp ("_test-", test_types[i] + 11, 6));

      if (!epc_test_init_service_browser (test_types[i], service_browser_cb, NULL))
        goto out;
    }


  dispatcher = epc_dispatcher_new (test_name);

  if (!epc_dispatcher_run (dispatcher, &error))
    goto out;

  base_type = epc_service_type_get_base (test_types[0]);
  epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_UNSPEC,
                              base_type, NULL, NULL, 2007, NULL);

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    epc_dispatcher_add_service_subtype (dispatcher, base_type, test_types[i]);

  result = epc_test_run ();

out:
  if (error)
    g_print ("%s: %s\n", G_STRLOC, error->message);

  g_clear_error (&error);

  if (dispatcher)
    g_object_unref (dispatcher);

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    g_free (test_types[i]);

  g_free (test_name);

  return result;
}
