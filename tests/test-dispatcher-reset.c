/* Test that epc_dispatcher_reset removes services */

#include "framework.h"
#include "libepc/dispatcher.h"

static gchar *test_type = NULL;
static gchar *test_name = NULL;

static void
service_browser_cb (AvahiServiceBrowser     *browser G_GNUC_UNUSED,
                    AvahiIfIndex             interface G_GNUC_UNUSED,
                    AvahiProtocol            protocol G_GNUC_UNUSED,
                    AvahiBrowserEvent        event,
                    const char              *name,
                    const char              *type,
                    const char              *domain G_GNUC_UNUSED,
                    AvahiLookupResultFlags   flags G_GNUC_UNUSED,
                    void                    *data)
{
  EpcDispatcher *dispatcher = data;

  if (name && g_str_equal (name, test_name) &&
      type && g_str_equal (type, test_type))
    {
      if (AVAHI_BROWSER_NEW == event)
        {
          epc_test_pass_once (1);
          epc_dispatcher_reset (dispatcher);
        }

      if (AVAHI_BROWSER_REMOVE == event)
        epc_test_pass_once (2);
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher = NULL;
  gint result = EPC_TEST_MASK_ALL;

  g_type_init ();

  test_name = g_strdup_printf ("%s: %08x", __FILE__, g_random_int ());
  test_type = g_strdup_printf ("_test-%08x._tcp", g_random_int ());

  dispatcher = epc_dispatcher_new (test_name);

  if (epc_test_init (3) &&
      epc_test_init_service_browser (test_type, service_browser_cb, dispatcher))
    {
      epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_UNSPEC,
                                  test_type, NULL, NULL, 2007, NULL);
      result = epc_test_run ();
    }

  if (dispatcher)
    g_object_unref (dispatcher);

  g_free (test_type);
  g_free (test_name);

  return result;
}
