#include "framework.h"

#include <libepc/consumer.h>
#include <libepc/publisher.h>
#include <libepc/service-monitor.h>

static gchar *test_cookie = NULL;
static gchar *test_name = NULL;
static gchar *test_value1 = NULL;
static gchar *test_value2 = NULL;

static void
service_found_cb (EpcServiceMonitor *monitor G_GNUC_UNUSED,
                  const gchar       *name,
                  EpcServiceInfo    *info)
{
  if (g_str_equal (name, test_name))
    {
      EpcConsumer *consumer;
      GError *error = NULL;
      const gchar *cookie;
      gchar *value;

      epc_test_pass_many (1 << 1);

      cookie = epc_service_info_get_detail (info, "cookie");

      if (cookie && g_str_equal (cookie, test_cookie))
        epc_test_pass_many (1 << 2);

      consumer = epc_consumer_new (info);
      value = epc_consumer_lookup (consumer, "id", NULL, &error);

      if (value && g_str_equal (value, test_value1))
        epc_test_pass_once (1 << 4);
      if (value && g_str_equal (value, test_value2))
        epc_test_pass_once (1 << 6);
      if (error)
        g_warning ("%s: %s", G_STRLOC, error->message);

      g_object_unref (consumer);
      g_clear_error (&error);
      g_free (value);
    }
}

static gboolean
quit_publisher_cb (gpointer data)
{
  g_print ("%s: Stopping first provider...\n", G_STRFUNC);
  epc_publisher_quit (data);
  g_object_unref (data);

  return FALSE;
}

static EpcContents*
contents_handler_cb (EpcPublisher *publisher,
                     const gchar  *key G_GNUC_UNUSED,
                     gpointer      data)
{
  if (data == test_value1)
    epc_test_pass_once (1 << 3);
  if (data == test_value2)
    epc_test_pass_once (1 << 5);

  g_timeout_add (250, quit_publisher_cb, g_object_ref (publisher));

  return epc_contents_new_dup (NULL, data, -1);
}

int
main (void)
{
  EpcServiceMonitor *monitor;
  EpcPublisher *publisher1;
  EpcPublisher *publisher2;
  GError *error = NULL;
  gint result = 1;

  g_set_prgname (__FILE__);
  g_thread_init (NULL);
  g_type_init ();

  test_cookie = g_strdup_printf ("%08x-%08x", g_random_int (), g_random_int ());
  test_name = g_strdup_printf ("%s %08x", __FILE__, g_random_int ());
  test_value1 = g_strdup_printf ("Foo %08x", g_random_int ());
  test_value2 = g_strdup_printf ("Bar %08x", g_random_int ());

  monitor = epc_service_monitor_new (NULL, NULL, EPC_PROTOCOL_HTTP, 0);
  g_signal_connect (monitor, "service-found", G_CALLBACK (service_found_cb), NULL);

  publisher1 = epc_publisher_new (test_name, NULL, NULL);
  epc_publisher_add_handler (publisher1, "id", contents_handler_cb, test_value1, NULL);
  epc_publisher_set_collision_handling (publisher1, EPC_COLLISIONS_UNIQUE_SERVICE);
  epc_publisher_set_service_cookie (publisher1, test_cookie);
  epc_publisher_set_protocol (publisher1, EPC_PROTOCOL_HTTP);

  publisher2 = epc_publisher_new (test_name, NULL, NULL);
  epc_publisher_add_handler (publisher2, "id", contents_handler_cb, test_value2, NULL);
  epc_publisher_set_collision_handling (publisher2, EPC_COLLISIONS_UNIQUE_SERVICE);
  epc_publisher_set_service_cookie (publisher2, test_cookie);
  epc_publisher_set_protocol (publisher2, EPC_PROTOCOL_HTTP);

  if (epc_test_init (7) &&
      epc_publisher_run_async (publisher1, &error) &&
      epc_publisher_run_async (publisher2, &error))
    {
      epc_test_pass_once (1 << 0);
      result = epc_test_run ();
    }

  if (error)
    {
      g_print ("%s: %s\n", G_STRLOC, error->message);
      g_error_free (error);
    }

  if (publisher2)
    g_object_unref (publisher2);
  if (publisher1)
    g_object_unref (publisher1);
  if (monitor)
    g_object_unref (monitor);

  g_free (test_value2);
  g_free (test_value1);
  g_free (test_cookie);
  g_free (test_name);

  return result;
}
