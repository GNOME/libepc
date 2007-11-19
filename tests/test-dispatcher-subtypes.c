/* Test announcing multiple DNS-DS services */

#include "framework.h"
#include "libepc/dispatcher.h"
#include <string.h>

static gchar *test_types[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
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
  if ((AVAHI_BROWSER_NEW == event ||
       AVAHI_BROWSER_ALL_FOR_NOW == event) && type)
    {
      unsigned i;

      if (0 != (flags & AVAHI_LOOKUP_RESULT_LOCAL) &&
          name && g_str_equal (name, test_name))
        {
          if (0 == strcmp (type, test_types[0] + 11))
            epc_test_pass (1);
        }
      else
        {
          for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
            if (g_str_equal (type, test_types[i]))
              epc_test_pass (2 << i);
        }
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher = NULL;
  gint result = EPC_TEST_MASK_ALL;
  gint hash = g_random_int ();
  unsigned i;

  g_type_init ();

  test_name = g_strdup_printf ("%s: %08x", __FILE__, hash);

  if (!epc_test_init (EPC_TEST_MASK_ALL))
    goto out;

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    {
      test_types[i] = g_strdup_printf ("_sub%d._sub._test-%08x._tcp", i, hash);

      g_assert (0 == strncmp ("._sub._test-", test_types[i] + 5, 12));
      g_assert (0 == strncmp ("_test-", test_types[i] + 11, 6));

      if (!epc_test_init_service_browser (test_types[i], service_browser_cb, NULL))
        goto out;
    }


  dispatcher = epc_dispatcher_new (test_name);

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    epc_dispatcher_add_service (dispatcher, EPC_ADDRESS_UNSPEC,
                                test_types[i], NULL, NULL, 2007, NULL);

  result = epc_test_run ();

out:
  if (dispatcher)
    g_object_unref (dispatcher);

  for (i = 0; i < G_N_ELEMENTS (test_types); ++i)
    g_free (test_types[i]);

  g_free (test_name);

  return result;
}
