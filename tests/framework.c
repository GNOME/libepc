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
#include "libepc/shell.h"

#include <avahi-client/client.h>
#include <avahi-common/error.h>

static AvahiClient *epc_test_client = NULL;
static GSList *epc_test_service_browsers = NULL;
static GMainLoop *epc_test_loop = NULL;
static guint epc_test_timeout = 0;
static gint epc_test_result = 1;

gboolean
epc_test_init (gint test_count)
{
  gint test_mask = (1 << test_count) - 1;
  GError *error = NULL;

  g_assert (test_mask <= EPC_TEST_MASK_USER);
  epc_test_result = EPC_TEST_MASK_INIT | test_mask;
  epc_shell_ref ();

  if (NULL == epc_test_loop)
    epc_test_loop = g_main_loop_new (NULL, FALSE);

  g_return_val_if_fail (NULL != epc_test_loop, FALSE);

  if (NULL == epc_test_client)
    epc_test_client = epc_shell_create_avahi_client (AVAHI_CLIENT_IGNORE_USER_CONFIG |
                                                     AVAHI_CLIENT_NO_FAIL,
                                                     NULL, NULL, &error);

  if (!epc_test_client)
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}

gint
_epc_test_quit (const gchar *strloc)
{
  gint i;

  if (epc_test_loop)
    g_main_loop_quit (epc_test_loop);

  g_slist_foreach (epc_test_service_browsers,
                   (GFunc) avahi_service_browser_free, NULL);
  epc_test_service_browsers = NULL;

  if (epc_test_client)
    {
      avahi_client_free (epc_test_client);
      epc_test_client = NULL;
      epc_shell_unref ();
    }

  if (epc_test_loop)
    {
      g_main_loop_unref (epc_test_loop);
      epc_test_loop = NULL;
    }

  for (i = 0; (1 << i) < EPC_TEST_MASK_USER; ++i)
    if (epc_test_result & (1 << i))
      g_print ("%s: Test #%d failed\n", strloc, i + 1);

  return epc_test_result;
}

static gboolean
epc_test_timeout_cb (gpointer data G_GNUC_UNUSED)
{
  GString *message = g_string_new (NULL);
  gint i;

  g_string_printf (message, "%s: Timeout reached. Aborting. Unsatisfied tests:", G_STRLOC);

  for (i = 0; (1 << i) < EPC_TEST_MASK_USER; ++i)
    if (epc_test_result & (1 << i))
      g_string_append_printf (message, " %d", i + 1);

  g_print ("%s\n", message->str);
  g_string_free (message, TRUE);

  g_return_val_if_fail (NULL != epc_test_loop, FALSE);
  g_main_loop_quit (epc_test_loop);

  return FALSE;
}

void
_epc_test_pass (const gchar *strloc,
                gboolean     once,
                gint         mask)
{
  int i;

  if (epc_test_timeout)
    {
      g_source_remove (epc_test_timeout);
      epc_test_timeout = g_timeout_add (5000, epc_test_timeout_cb, NULL);
    }

  mask &= EPC_TEST_MASK_USER;

  for (i = 0; (1 << i) < EPC_TEST_MASK_USER; ++i)
    if (mask & (1 << i))
      {
        if (once && !(epc_test_result & (1 << i)))
          g_error ("Test #%d passed a second time, which was not expected", i + 1);
        else
          g_print ("%s: Test #%d passed\n", strloc, i + 1);
      }

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
      g_warning ("%s: %s (%d)", G_STRFUNC, avahi_strerror (error), error);
      return FALSE;
    }

  epc_test_service_browsers =
    g_slist_prepend (epc_test_service_browsers, browser);

  return TRUE;
}

gint
epc_test_run ()
{
  epc_test_result &= ~EPC_TEST_MASK_INIT;

  if (!epc_test_loop)
    epc_test_loop = g_main_loop_new (NULL, FALSE);

  epc_test_timeout = g_timeout_add (5000, epc_test_timeout_cb, NULL);

  epc_shell_leave ();
  g_main_loop_run (epc_test_loop);
  epc_shell_enter ();

  return epc_test_result;
}
