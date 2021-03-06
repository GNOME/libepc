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

/* Test that changing of the publisher's service name gets announced */

#include "framework.h"

#include <libepc/publisher.h>
#include <libepc/shell.h>

static gchar *first_name = NULL;
static gchar *second_name = NULL;

static void
service_browser_cb (AvahiServiceBrowser     *browser G_GNUC_UNUSED,
                    AvahiIfIndex             interface,
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
      type && name)
    {
      if (g_str_equal (name, first_name))
        {
          if (g_str_equal (type, EPC_SERVICE_TYPE_HTTP))
            epc_test_pass_once_per_iface (1 << 1, interface);
          if (g_str_equal (type, "_http._tcp"))
            epc_test_pass_once_per_iface (1 << 2, interface);
        }

      if (g_str_equal (name, second_name))
        {
          if (g_str_equal (type, "_http._tcp"))
            epc_test_pass_once_per_iface (1 << 3, interface);
        }
    }
}

int
main (int   argc G_GNUC_UNUSED,
      char *argv[])
{
  EpcPublisher *publisher = NULL;
  int result = EPC_TEST_MASK_ALL;
  GError *error = NULL;
  gchar *prgname;
  gint hash;

  prgname = g_path_get_basename (argv[0]);
  g_set_prgname (prgname);
  g_free (prgname);

  hash = g_random_int ();
  first_name = g_strdup_printf ("%s-%08x-1", g_get_prgname (), hash);
  second_name = g_strdup_printf ("%s-%08x-2", g_get_prgname (), hash);

  publisher = epc_publisher_new (first_name, NULL, NULL);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);

  epc_publisher_add (publisher, "cookie", "Yummy: A tasty, brown cookie.", -1);
  epc_publisher_add_bookmark (publisher, "cookie", second_name);

  epc_publisher_add (publisher, "egg", "What an easter egg...", -1);
  epc_publisher_add_bookmark (publisher, "egg", NULL);

  if (epc_test_init (4) &&
      epc_test_init_service_browser (EPC_SERVICE_TYPE_HTTP, service_browser_cb, publisher) &&
      epc_test_init_service_browser ("_http._tcp", service_browser_cb, publisher) &&
      epc_publisher_run_async (publisher, &error))
    {
      epc_test_pass_once (1 << 0);
      result = epc_test_run ();
    }

  while (EPC_DEBUG_LEVEL (99))
    result = epc_test_run ();

  if (error)
    {
      g_print ("%s: %s\n", G_STRLOC, error->message);
      g_error_free (error);
    }

  if (publisher)
    g_object_unref (publisher);

  g_free (second_name);
  g_free (first_name);

  return result;
}
