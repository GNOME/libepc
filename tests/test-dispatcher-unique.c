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
#include "framework.h"

#include <libepc/consumer.h>
#include <libepc/dispatcher.h>
#include <libepc/service-monitor.h>

#include <avahi-common/alternative.h>
#include <string.h>

static gchar *test_cookie1 = NULL;
static gchar *test_cookie2 = NULL;
static gchar *test_name1 = NULL;
static gchar *test_name2 = NULL;

static EpcDispatcher *dispatcher1 = NULL;
static EpcDispatcher *dispatcher2 = NULL;
static EpcDispatcher *dispatcher3 = NULL;

static gboolean
stop_dispatcher_cb (gpointer data G_GNUC_UNUSED)
{
  if (dispatcher1)
    {
      g_print ("stopping first dispatcher\n");
      g_object_unref (dispatcher1);
      dispatcher1 = NULL;
    }

  return FALSE;
}

static void
service_found_cb (EpcServiceMonitor *monitor G_GNUC_UNUSED,
                  const gchar       *name,
                  EpcServiceInfo    *info)
{
  const gchar *cookie = epc_service_info_get_detail (info, "cookie");
  const gchar *dispatcher = epc_service_info_get_detail (info, "id");

  if (g_str_equal (name, test_name1) &&
      cookie && g_str_equal (cookie, test_cookie1) &&
      dispatcher && g_str_equal (dispatcher, "dispatcher1"))
    {
      epc_test_pass_once (1 << 3);
      g_timeout_add (500, stop_dispatcher_cb, NULL);
    }

  if (g_str_equal (name, test_name1) &&
      cookie && g_str_equal (cookie, test_cookie1) &&
      dispatcher && g_str_equal (dispatcher, "dispatcher2"))
    epc_test_pass_once (1 << 4);

  if (g_str_equal (name, test_name2) &&
      cookie && g_str_equal (cookie, test_cookie2) &&
      dispatcher && g_str_equal (dispatcher, "dispatcher3"))
    epc_test_pass_once (1 << 5);
}

int
main (void)
{
  EpcServiceMonitor *monitor = NULL;
  GError *error = NULL;
  gint result = 1;

  g_set_prgname (__FILE__);
  g_thread_init (NULL);
  g_type_init ();

  test_cookie1 = g_strdup_printf ("%08x-%08x-1", g_random_int (), g_random_int ());
  test_cookie2 = g_strdup_printf ("%08x-%08x-2", g_random_int (), g_random_int ());
  test_name1 = g_strdup_printf ("%s %08x", __FILE__, g_random_int ());
  test_name2 = avahi_alternative_service_name (test_name1);

  monitor = epc_service_monitor_new_for_types (NULL, EPC_SERVICE_TYPE_HTTP, NULL);
  g_signal_connect (monitor, "service-found", G_CALLBACK (service_found_cb), NULL);

  dispatcher1 = epc_dispatcher_new (test_name1);
  epc_dispatcher_set_cookie (dispatcher1, test_cookie1);
  epc_dispatcher_set_collision_handling (dispatcher1, EPC_COLLISIONS_UNIQUE_SERVICE);
  epc_dispatcher_add_service (dispatcher1, EPC_ADDRESS_IPV4, EPC_SERVICE_TYPE_HTTP,
                              NULL, NULL, 9000, "id=dispatcher1", NULL);

  dispatcher2 = epc_dispatcher_new (test_name1);
  epc_dispatcher_set_cookie (dispatcher2, test_cookie1);
  epc_dispatcher_set_collision_handling (dispatcher2, EPC_COLLISIONS_UNIQUE_SERVICE);
  epc_dispatcher_add_service (dispatcher2, EPC_ADDRESS_IPV4, EPC_SERVICE_TYPE_HTTP,
                              NULL, NULL, 9001, "id=dispatcher2", NULL);

  dispatcher3 = epc_dispatcher_new (test_name1);

  if (epc_test_init (6))
    {
      if (NULL == epc_dispatcher_get_cookie (dispatcher3))
        epc_test_pass_once (1 << 0);

      epc_dispatcher_set_collision_handling (dispatcher3, EPC_COLLISIONS_UNIQUE_SERVICE);

      if (NULL != epc_dispatcher_get_cookie (dispatcher3) &&
          36 == strlen (epc_dispatcher_get_cookie (dispatcher3)))
        epc_test_pass_once (1 << 1);

      epc_dispatcher_set_cookie (dispatcher3, test_cookie2);
      epc_dispatcher_add_service (dispatcher3, EPC_ADDRESS_IPV4, EPC_SERVICE_TYPE_HTTP,
                                  NULL, NULL, 9002, "id=dispatcher3", NULL);

      if (epc_dispatcher_run (dispatcher1, &error) &&
          epc_dispatcher_run (dispatcher2, &error) &&
          epc_dispatcher_run (dispatcher3, &error))
        {
          epc_test_pass_once (1 << 2);
          result = epc_test_run ();
        }
    }

  if (error)
    {
      g_print ("%s: %s\n", G_STRLOC, error->message);
      g_error_free (error);
    }

  if (dispatcher3)
    g_object_unref (dispatcher3);
  if (dispatcher2)
    g_object_unref (dispatcher2);
  if (dispatcher1)
    g_object_unref (dispatcher1);
  if (monitor)
    g_object_unref (monitor);

  g_free (test_cookie2);
  g_free (test_cookie1);
  g_free (test_name2);
  g_free (test_name1);

  return result;
}
