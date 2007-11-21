/* Test that epc_dispatcher_set_name removes the old service */

#include "framework.h"
#include "libepc/dispatcher.h"

static gchar *test_type = NULL;
static gchar *first_name = NULL;
static gchar *second_name = NULL;
static gchar *third_name = NULL;

static void
service_browser_cb (AvahiServiceBrowser     *browser G_GNUC_UNUSED,
                    AvahiIfIndex             interface G_GNUC_UNUSED,
                    AvahiProtocol            protocol G_GNUC_UNUSED,
                    AvahiBrowserEvent        event,
                    const char              *name G_GNUC_UNUSED,
                    const char              *type G_GNUC_UNUSED,
                    const char              *domain G_GNUC_UNUSED,
                    AvahiLookupResultFlags   flags G_GNUC_UNUSED,
                    void                    *data)
{
  EpcDispatcher *dispatcher = data;

  if (type && g_str_equal (type, test_type) && name)
    {
      switch (event)
        {
          case AVAHI_BROWSER_NEW:
            if (g_str_equal (name, first_name))
              {
                epc_test_pass_once (1 << 0);
                epc_dispatcher_set_name (dispatcher, second_name);
              }

            if (g_str_equal (name, second_name))
              {
                epc_test_pass_once (1 << 2);
                epc_dispatcher_set_name (dispatcher, third_name);
              }

            if (g_str_equal (name, third_name))
              epc_test_pass_once (1 << 4);

            break;

          case AVAHI_BROWSER_REMOVE:
            if (g_str_equal (name, first_name))
              epc_test_pass_once (1 << 1);

            if (g_str_equal (name, second_name))
              epc_test_pass_once (1 << 3);

            break;

          default:
            break;
        }
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher = NULL;
  gint result = EPC_TEST_MASK_ALL;
  GError *error = NULL;

  g_type_init ();

  test_type = g_strdup_printf ("_test-%08x._tcp", g_random_int ());
  first_name = g_strdup_printf ("%s: %08x-1", __FILE__, g_random_int ());
  second_name = g_strdup_printf ("%s: %08x-2", __FILE__, g_random_int ());
  third_name = g_strdup_printf ("%s: %08x-3", __FILE__, g_random_int ());

  dispatcher = epc_dispatcher_new (first_name);

  if (epc_test_init (5) &&
      epc_test_init_service_browser (test_type, service_browser_cb, dispatcher) &&
      epc_dispatcher_run (dispatcher, &error))
    {
      epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_UNSPEC,
                                  test_type, NULL, NULL, 2007, NULL);

      result = epc_test_run ();
    }

  if (error)
    g_print ("%s: %s\n", G_STRLOC, error->message);

  g_clear_error (&error);

  if (dispatcher)
    g_object_unref (dispatcher);

  g_free (third_name);
  g_free (second_name);
  g_free (first_name);
  g_free (test_type);

  return result;
}
