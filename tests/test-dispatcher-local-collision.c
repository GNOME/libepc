/* Test handling of local DNS-DS service collisions */

#include "framework.h"
#include "libepc/dispatcher.h"

#include <avahi-common/alternative.h>
#include <string.h>

static gchar *test_type = NULL;
static gchar *preferred_name = NULL;
static gchar *alternative_name = NULL;

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
  if (AVAHI_BROWSER_NEW == event &&
      0 != (flags & AVAHI_LOOKUP_RESULT_LOCAL) &&
      type && g_str_equal (type, test_type) && name)
    {
      if (g_str_equal (name, preferred_name))
        epc_test_pass (1);
      if (g_str_equal (name, alternative_name))
        epc_test_pass (2);
    }
}

int
main (void)
{
  EpcDispatcher *dispatcher1 = NULL;
  EpcDispatcher *dispatcher2 = NULL;
  int result = EPC_TEST_MASK_ALL;

  g_type_init ();

  test_type = g_strdup_printf ("_test-%08x._tcp", g_random_int ());
  preferred_name = g_strdup_printf ("%s: %08x", __FILE__, g_random_int ());
  alternative_name = avahi_alternative_service_name (preferred_name);

  g_assert (!g_str_equal (preferred_name, alternative_name));

  if (epc_test_init () &&
      epc_test_init_service_browser (test_type, service_browser_cb, NULL))
    {
      epc_test_pass (~3);

      dispatcher1 = epc_dispatcher_new (AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, preferred_name);
      epc_dispatcher_add_service (dispatcher1, test_type, NULL, NULL, 2007, NULL);

      dispatcher2 = epc_dispatcher_new (AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, preferred_name);
      epc_dispatcher_add_service (dispatcher2, test_type, NULL, NULL, 2007, NULL);

      result = epc_test_run ();

      if (strcmp (alternative_name, epc_dispatcher_get_name (dispatcher2)))
        result |= 4;
      if (strcmp (preferred_name, epc_dispatcher_get_name (dispatcher1)))
        result |= 8;
    }

  if (dispatcher2)
    g_object_unref (dispatcher2);
  if (dispatcher1)
    g_object_unref (dispatcher1);

  g_free (alternative_name);
  g_free (preferred_name);
  g_free (test_type);

  return result;
}
