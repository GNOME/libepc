#include "libepc/consumer.h"
#include "libepc/publisher.h"

#include "framework.h"

#include <string.h>

static gchar *test_name = NULL;
static gchar *test_path = NULL;
static gchar *test_value = NULL;
static gchar *test_key = NULL;

static void
service_found_cb (EpcServiceMonitor    *monitor G_GNUC_UNUSED,
                  const gchar          *name,
                  const EpcServiceInfo *service)
{
  EpcConsumer *consumer = NULL;
  GError *error = NULL;
  gchar *value = NULL;

  if (!test_name || strcmp (test_name, name))
    goto out;

  epc_test_pass_once (1 << 0);

  consumer = epc_consumer_new (service);

  epc_test_goto_if_fail (EPC_IS_CONSUMER (consumer), out);
  epc_test_pass_once (1 << 1);

  value = epc_consumer_lookup (consumer, test_key, NULL, &error);

  if (value)
    epc_test_pass_once (1 << 2);
  if (value && g_str_equal (value, test_value))
    epc_test_pass_once (1 << 3);

  g_free (value);
  g_object_get (consumer, "path", &value, NULL);

  if (value && g_str_equal (value, test_path))
    epc_test_pass_once (1 << 4);

  epc_test_quit ();

out:
  if (error)
    g_warning ("%s: lookup failed: %s", G_STRLOC, error->message);

  g_clear_error (&error);
  g_free (value);

  if (consumer)
    g_object_unref (consumer);
}

int
main (void)
{
  EpcServiceMonitor *monitor = NULL;
  EpcPublisher *publisher = NULL;
  gboolean running = FALSE;
  GError *error = NULL;
  gint result = 1;

  g_set_prgname (__FILE__);
  g_thread_init (NULL);
  g_type_init ();

  if (!epc_test_init (5))
    goto out;

  test_name  = g_strdup_printf ("%s %x", __FILE__, g_random_int ());
  test_path  = g_strdup_printf ("/stuff-%x", g_random_int ());
  test_key   = g_strdup_printf ("Maman %x",  g_random_int ());
  test_value = g_strdup_printf ("Bar: %x",   g_random_int ());

  monitor = epc_service_monitor_new (NULL, NULL, EPC_PROTOCOL_UNKNOWN);
  g_signal_connect (monitor, "service-found", G_CALLBACK (service_found_cb), NULL);

  publisher = epc_publisher_new (test_name, NULL, NULL);
  epc_test_goto_if_fail (EPC_IS_PUBLISHER (publisher), out);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);
  epc_publisher_set_contents_path (publisher, test_path);
  epc_publisher_add (publisher, test_key, test_value, -1);

  running = epc_publisher_run_async (publisher, &error);
  epc_test_goto_if_fail (running, out);

  result = epc_test_run ();

out:
  if (error)
    g_warning ("%s: lookup failed: %s", G_STRLOC, error->message);

  g_clear_error (&error);

  if (publisher)
    g_object_unref (publisher);
  if (monitor)
    g_object_unref (monitor);

  g_free (test_name);
  g_free (test_path);
  g_free (test_key);
  g_free (test_value);

  return result;
}
