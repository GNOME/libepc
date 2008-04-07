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
#include "libepc/shell.h"

#include <avahi-client/client.h>
#include <avahi-common/error.h>

#include <net/if.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <unistd.h>

typedef struct _EpcIfTestStatus EpcIfTestStatus;

struct _EpcIfTestStatus
{
  guint  ifidx;
  gchar *name;
  gint   mask;
};

static GSList *epc_test_service_browsers = NULL;
static GMainLoop *epc_test_loop = NULL;
static guint epc_test_timeout = 0;
static gint epc_test_result = 1;

gboolean
epc_test_init (gint test_count)
{
  gint test_mask = (1 << test_count) - 1;

  g_assert (test_mask <= EPC_TEST_MASK_USER);
  epc_test_result = EPC_TEST_MASK_INIT | test_mask;

  if (NULL == epc_test_loop)
    epc_test_loop = g_main_loop_new (NULL, FALSE);

  g_return_val_if_fail (NULL != epc_test_loop, FALSE);

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

static EpcIfTestStatus*
epc_test_list_ifaces (void)
{
  EpcIfTestStatus *ifaces = NULL;
  gsize n_ifaces = 0, i, j;
  struct ifconf ifc;
  char buf[4096];
  gint fd = -1;

  fd = socket (AF_INET, SOCK_DGRAM, 0);

  if (fd < 0)
    {
      g_warning ("%s: socket(AF_INET, SOCK_DGRAM): %s",
                 G_STRLOC, g_strerror (errno));
      goto out;
    }

  ifc.ifc_buf = buf;
  ifc.ifc_len = sizeof buf;

  if (ioctl (fd, SIOCGIFCONF, &ifc) < 0)
    {
      g_warning ("%s: ioctl(SIOCGIFCONF): %s",
                 G_STRLOC, g_strerror (errno));
      goto out;
    }

  n_ifaces = ifc.ifc_len / sizeof(struct ifreq);
  ifaces = g_new0 (EpcIfTestStatus, n_ifaces + 1);

  for (i = 0, j = 0; i < n_ifaces; ++i)
    {
      struct ifreq *req = &ifc.ifc_req[i];

      ifaces[j].name = g_strdup (req->ifr_name);

      if (ioctl (fd, SIOCGIFFLAGS, req) < 0)
        {
          g_warning ("%s: ioctl(SIOCGIFFLAGS): %s",
                     G_STRLOC, g_strerror (errno));
          goto out;
        }

      if (req->ifr_flags & (IFF_LOOPBACK | IFF_POINTOPOINT))
        {
          g_free (ifaces[j].name);
          ifaces[j].name = NULL;
          continue;
        }

      if (ioctl (fd, SIOCGIFINDEX, req) < 0)
        {
          g_warning ("%s: ioctl(SIOCGIFINDEX): %s",
                     G_STRLOC, g_strerror (errno));
          goto out;
        }

      ifaces[j].ifidx = req->ifr_ifindex;
      ifaces[j].mask = epc_test_result;

      g_print ("%s: name=%s, ifidx=%u, \n",
               G_STRLOC, ifaces[j].name,
               ifaces[j].ifidx);

      ++j;
    }

out:
  if (fd >= 0)
    close (fd);

  return ifaces;
}

static gboolean
_epc_test_match_any (const gchar           *strloc,
                     const EpcIfTestStatus *status,
                     guint                  ifidx G_GNUC_UNUSED,
                     gint                   i)
{
  const gint mask = (1 << i);

  if (!(status->mask & mask))
    return FALSE;

  g_print ("%s: Test #%d passed for some network interface\n", strloc, i + 1);
  return TRUE;
}

static gboolean
_epc_test_match_one (const gchar           *strloc,
                     const EpcIfTestStatus *status,
                     guint                  ifidx,
                     gint                   i)
{
  if (status->ifidx != ifidx)
    return FALSE;

  g_print ("%s: Test #%d passed for %s\n", strloc, i + 1, status->name);
  return TRUE;
}

void
_epc_test_pass_once_per_iface (const gchar *strloc,
                               gint         mask,
                               guint        ifidx,
                               gboolean     any)
{
  gboolean (*match)(const gchar           *strloc,
                    const EpcIfTestStatus *status,
                    guint                  ifidx,
                    gint                   i);

  static EpcIfTestStatus *ifaces = NULL;
  gint pending;
  guint i, j;

  if (!ifaces)
    ifaces = epc_test_list_ifaces ();

  g_assert (NULL != ifaces);
  mask &= EPC_TEST_MASK_USER;

  if (any)
    match = _epc_test_match_any;
  else
    match = _epc_test_match_one;

  for (i = 0; (1 << i) < EPC_TEST_MASK_USER; ++i)
    {
      if (0 == (mask & (1 << i)))
        continue;

      pending = 0;

      for (j = 0; ifaces[j].name; ++j)
        {
          if (match (strloc, &ifaces[j], ifidx, i))
            {
              ifaces[j].mask &= ~(1 << i);
              break;
            }
        }

      for (j = 0; ifaces[j].name; ++j)
        pending |= (ifaces[j].mask & (1 << i));

      if (!pending)
        _epc_test_pass (strloc, TRUE, 1 << i);
    }
}

gboolean
epc_test_init_service_browser (const gchar                 *service,
                               AvahiServiceBrowserCallback  callback,
                               gpointer                     data)
{
  AvahiServiceBrowser *browser;
  GError *error = NULL;

  browser = epc_shell_create_service_browser (AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                                              service, NULL, AVAHI_LOOKUP_USE_MULTICAST,
                                              callback, data, &error);

  if (NULL == browser)
    {
      g_warning ("%s: %s (%d)", G_STRFUNC, error->message, error->code);
      return FALSE;
    }

  epc_test_service_browsers = g_slist_prepend (epc_test_service_browsers, browser);

  return TRUE;
}

gint
epc_test_run ()
{
  epc_test_result &= ~EPC_TEST_MASK_INIT;

  if (!epc_test_loop)
    epc_test_loop = g_main_loop_new (NULL, FALSE);

  epc_test_timeout = g_timeout_add (5000, epc_test_timeout_cb, NULL);
  g_main_loop_run (epc_test_loop);

  return epc_test_result;
}
