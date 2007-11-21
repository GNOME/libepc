/* Test that changing of the publisher's service name gets announced */

#include "framework.h"
#include "libepc/publisher.h"

static gchar *first_name = NULL;
static gchar *second_name = NULL;

static void
service_browser_cb (AvahiServiceBrowser     *browser G_GNUC_UNUSED,
                    AvahiIfIndex             interface G_GNUC_UNUSED,
                    AvahiProtocol            protocol G_GNUC_UNUSED,
                    AvahiBrowserEvent        event,
                    const char              *name,
                    const char              *type,
                    const char              *domain G_GNUC_UNUSED,
                    AvahiLookupResultFlags   flags,
                    void                    *data G_GNUC_UNUSED)
{
  EpcPublisher *publisher = data;

  if (AVAHI_BROWSER_NEW == event &&
      0 != (flags & AVAHI_LOOKUP_RESULT_LOCAL) &&
      type && g_str_equal (type, EPC_SERVICE_TYPE_HTTP) && name)
    {
      if (g_str_equal (name, first_name))
        {
          epc_test_pass_once (1 << 1);
          epc_publisher_set_service_name (publisher, second_name);
        }

      if (g_str_equal (name, second_name))
        epc_test_pass_once (1 << 2);
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

  prgname = g_path_get_basename (argv[0]);
  g_set_prgname (prgname);
  g_free (prgname);

  g_thread_init (NULL);
  g_type_init ();

  first_name = g_strdup_printf ("%s-%08x-1", g_get_prgname (), g_random_int ());
  second_name = g_strdup_printf ("%s-%08x-2", g_get_prgname (), g_random_int ());

  publisher = epc_publisher_new (first_name, NULL, NULL);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);

  if (epc_test_init (3) &&
      epc_test_init_service_browser (EPC_SERVICE_TYPE_HTTP, service_browser_cb, publisher) &&
      epc_publisher_run_async (publisher, &error))
    {
      epc_test_pass_once (1 << 0);
      result = epc_test_run ();
    }

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
