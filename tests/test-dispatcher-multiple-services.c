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

/* Test announcing multiple DNS-DS services */

#include "framework.h"
#include "libepc/dispatcher.h"

static gchar *test_types[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static gchar *test_name = NULL;

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
      name && type && g_str_equal (name, test_name))
    {
      unsigned i;

      for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
        if (g_str_equal (type, test_types[i]))
          epc_test_pass_once (1 << i);
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher = NULL;
  gint result = EPC_TEST_MASK_ALL;
  gint hash = g_random_int ();
  GError *error = NULL;
  unsigned i;

  g_type_init ();

  test_name = g_strdup_printf ("%s: %08x", __FILE__, hash);

  if (!epc_test_init (7))
    goto out;

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    {
      test_types[i] = g_strdup_printf ("_test-%08x-%d._tcp", hash, i);

      if (!epc_test_init_service_browser (test_types[i], service_browser_cb, NULL))
        goto out;
    }

  dispatcher = epc_dispatcher_new (test_name);

  if (!epc_dispatcher_run (dispatcher, &error))
    goto out;

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_UNSPEC,
                                test_types[i], NULL, NULL, 2007, NULL);

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
