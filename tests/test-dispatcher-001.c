#include "libepc/dispatcher.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-glib/glib-watch.h>

static gchar *test_type = NULL;
static gchar *test_name = NULL;
static gint result = 1;

static void
service_browser_cb (AvahiServiceBrowser     *browser G_GNUC_UNUSED,
                    AvahiIfIndex             interface G_GNUC_UNUSED,
                    AvahiProtocol            protocol G_GNUC_UNUSED,
                    AvahiBrowserEvent        event,
                    const char              *name G_GNUC_UNUSED,
                    const char              *type G_GNUC_UNUSED,
                    const char              *domain G_GNUC_UNUSED,
                    AvahiLookupResultFlags   flags,
                    void                    *data)
{
  if (AVAHI_BROWSER_NEW == event &&
      0 != (flags & AVAHI_LOOKUP_RESULT_LOCAL) &&
      name && g_str_equal (name, test_name) &&
      type && g_str_equal (type, test_type))
    {
      g_main_loop_quit (data);
      result = 0;
    }
}

static gboolean
timeout_cb (gpointer data)
{
  g_print ("%s: Timeout reached. Aborting.\n", G_STRLOC);
  g_main_loop_quit (data);
  return FALSE;
}

int
main ()
{
  AvahiGLibPoll *poll = NULL;
  AvahiClient *client = NULL;
  AvahiServiceBrowser *browser = NULL;
  EpcDispatcher *dispatcher = NULL;
  GMainLoop *loop = NULL;
  int error = AVAHI_OK;

  g_type_init ();

  test_name = g_strdup_printf ("Test Service %08x (%s)", g_random_int (), __FILE__);
  test_type = g_strdup_printf ("_test-%08x._tcp", g_random_int ());

#if 0
  g_print ("%s: name=%s\n", G_STRLOC, test_name);
  g_print ("%s: type=%s\n", G_STRLOC, test_type);
#endif

  loop = g_main_loop_new (NULL, FALSE);

  poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
  client = avahi_client_new (avahi_glib_poll_get (poll),
                             AVAHI_CLIENT_IGNORE_USER_CONFIG |
                             AVAHI_CLIENT_NO_FAIL,
                             NULL, NULL, &error);

  if (!client)
    goto out;

  browser = avahi_service_browser_new (
    client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, test_type, NULL,
    AVAHI_LOOKUP_USE_MULTICAST, service_browser_cb, loop);

  if (!browser)
    goto out;

  dispatcher = epc_dispatcher_new (AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, test_name);
  epc_dispatcher_add_service (dispatcher, test_type, NULL, NULL, 2007, NULL);

  g_timeout_add_seconds (5, timeout_cb, loop);
  g_main_loop_run (loop);

out:
  if (client)
    error = avahi_client_errno (client);
  if (!client || !browser)
    g_warning ("%s: %s (%d)", G_STRLOC, avahi_strerror (error), error);

  if (loop)
    g_main_loop_unref (loop);
  if (dispatcher)
    g_object_unref (dispatcher);
  if (browser)
    avahi_service_browser_free (browser);

  avahi_client_free (client);
  avahi_glib_poll_free (poll);

  g_free (test_type);
  g_free (test_name);

  return result;
}
