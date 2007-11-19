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
#include "framework.h"
#include "libepc/avahi-shell.h"

#include <avahi-client/client.h>
#include <avahi-common/error.h>

static AvahiClient *epc_test_client = NULL;
static GSList *epc_test_service_browsers = NULL;
static GMainLoop *epc_test_loop = NULL;

static gint epc_test_result = 1;

gboolean
epc_test_init (void)
{
  epc_test_result = EPC_TEST_MASK_ALL;
  epc_avahi_shell_ref ();

  if (NULL == epc_test_loop)
    epc_test_loop = g_main_loop_new (NULL, FALSE);

  g_return_val_if_fail (NULL != epc_test_loop, FALSE);

  if (NULL == epc_test_client)
    epc_test_client = epc_avahi_shell_create_client (AVAHI_CLIENT_IGNORE_USER_CONFIG |
                                                     AVAHI_CLIENT_NO_FAIL, NULL, NULL);

  g_return_val_if_fail (NULL != epc_test_client, FALSE);

  return TRUE;
}

void
epc_test_quit (void)
{
  if (epc_test_loop)
    g_main_loop_quit (epc_test_loop);

  g_slist_foreach (epc_test_service_browsers,
                   (GFunc) avahi_service_browser_free, NULL);

  if (epc_test_client)
    avahi_client_free (epc_test_client);
  if (epc_test_loop)
    g_main_loop_unref (epc_test_loop);

  epc_avahi_shell_unref ();

  epc_test_service_browsers = NULL;
  epc_test_client = NULL;
  epc_test_loop = NULL;
}

void
epc_test_pass (gint mask)
{
  mask &= EPC_TEST_MASK_USER;
  epc_test_result &= ~mask;

  if (0 == epc_test_result)
    epc_test_quit ();
}

gboolean
epc_test_init_service_browser (const gchar                 *service,
                               AvahiServiceBrowserCallback  callback,
                               gpointer                     data)
{
  AvahiServiceBrowser *browser;

  browser = avahi_service_browser_new (
    epc_test_client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
    service, NULL, AVAHI_LOOKUP_USE_MULTICAST,
    callback, data);

  if (NULL == browser)
    {
      int error = avahi_client_errno (epc_test_client);
      g_warning ("%s: %s (%d)", G_STRLOC, avahi_strerror (error), error);
      return FALSE;
    }

  epc_test_service_browsers =
    g_slist_prepend (epc_test_service_browsers, browser);

  return TRUE;
}

static gboolean
epc_test_timeout_cb (gpointer data G_GNUC_UNUSED)
{
  g_print ("%s: Timeout reached. Aborting.\n", G_STRLOC);
  g_return_val_if_fail (NULL != epc_test_loop, FALSE);

  g_main_loop_quit (epc_test_loop);

  return FALSE;
}

gint
epc_test_run ()
{
  epc_test_result &= ~EPC_TEST_MASK_INIT;

  if (!epc_test_loop)
    epc_test_loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add_seconds (5, epc_test_timeout_cb, NULL);
  g_main_loop_run (epc_test_loop);

  return epc_test_result;
}
