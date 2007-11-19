#include "libepc/consumer.h"
#include "libepc/publisher.h"

#define epc_test_goto_if_fail(Test, Label) G_STMT_START{        \
  if (!(Test))                                                  \
    {                                                           \
      g_warning ("%s: Assertion failed: %s", G_STRLOC, #Test);  \
      goto Label;                                               \
    }                                                           \
}G_STMT_END

int
main (void)
{
  EpcPublisher *publisher = NULL;
  EpcConsumer *consumer = NULL;
  gchar *value = NULL;
  const gchar *name;
  gint result = 1;

  g_set_prgname (__FILE__);
  g_thread_init (NULL);
  g_type_init ();

  publisher = epc_publisher_new (NULL, NULL, NULL);
  epc_test_goto_if_fail (EPC_IS_PUBLISHER (publisher), out);
  epc_publisher_add (publisher, "maman", "bar", -1);
  epc_publisher_run_async (publisher);

  name = epc_publisher_get_name (publisher);
  epc_test_goto_if_fail (NULL != name, out);

  consumer = epc_consumer_new_for_name (name);
  epc_test_goto_if_fail (EPC_IS_CONSUMER (consumer), out);

  value = epc_consumer_lookup (consumer, "maman", NULL);
  epc_test_goto_if_fail (value && g_str_equal (value, "bar"), out);

  result = 0;

out:
  if (value)
    g_debug ("%s: value=%s\n", G_STRLOC, value);

  g_free (value);

g_debug (G_STRLOC);
  if (consumer)
    g_object_unref (consumer);

g_debug (G_STRLOC);
  if (publisher)
    {
      epc_publisher_quit (publisher);
g_debug (G_STRLOC);
      g_object_unref (publisher);
    }

g_debug (G_STRLOC);
  return result;
}
