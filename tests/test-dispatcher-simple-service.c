/* Test announcing a single DNS-DS service */

#include "framework.h"
#include "libepc/dispatcher.h"

static gchar *test_type = NULL;
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
  if (AVAHI_BROWSER_NEW == event &&
      0 != (flags & AVAHI_LOOKUP_RESULT_LOCAL) &&
      name && g_str_equal (name, test_name) &&
      type && g_str_equal (type, test_type))
    {
      epc_test_pass_once (1);

      /* This epc_test_quit is not needed to reach the primary goal of this
       * test since epc_test_pass should call it, but it checks under real
       * conditionas that repeated calls of epc_test_quit work properly.
       */
      epc_test_quit ();
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher = NULL;
  gint result = EPC_TEST_MASK_ALL;
  gint hash = g_random_int ();
  GError *error = NULL;

  g_type_init ();

  test_name = g_strdup_printf ("%s: %08x", __FILE__, hash);
  test_type = g_strdup_printf ("_test-%08x._tcp", g_random_int ());

  if (epc_test_init (1) &&
      epc_test_init_service_browser (test_type, service_browser_cb, NULL))
    {
g_debug ("1");
      dispatcher = epc_dispatcher_new (test_name);

g_debug ("2");
      epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_UNSPEC,
                                  test_type, NULL, NULL, 2007, NULL);

g_debug ("3");
      if (epc_dispatcher_run (dispatcher, &error))
        result = epc_test_run ();
g_debug ("4");
    }

  if (error)
    g_print ("%s: %s\n", G_STRLOC, error->message);

  g_clear_error (&error);

  if (dispatcher)
    g_object_unref (dispatcher);

  g_free (test_type);
  g_free (test_name);

  return result;
}
