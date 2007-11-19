/* This program demonstrates and tests publishing of values.
 *
 * Usage: test-publisher [KEY=VALUE [KEY=VALUE...]]
 *
 * The arguments have the form "key=value". When a key is prefix with
 * the string "file:" the file of the name passed as value is published.
 *
 * When no arguments are passed some default values are published,
 * including the contents of this file by the key "source-code".
 */
#include "libepc/publisher.h"

int
main (int   argc,
      char *argv[])
{
  EpcPublisher *publisher;

  /* Initialize the toolkit.
   */
  g_thread_init (NULL);
  g_type_init ();

  /* Create a new publisher.
   */
  publisher = epc_publisher_new ("Easy Publisher Test", NULL, NULL);

  if (1 == argc)
    {
      /* Publish some default values,
       * when no arguments are passed on the command line.
       */
      epc_publisher_add (publisher, "test", "value", -1);
      epc_publisher_add_file (publisher, "source-code", __FILE__);
    }
  else
    {
      /* Publish the values passed on the command line. Each argument has the
       * form "key=value". When a key is prefix with the string "file:" the
       * file of the name passed as value is published.
       */
      int i;

      for (i = 1; i < argc; ++i)
        {
          char **pair = g_strsplit (argv[i], "=", 2);
          const char *key = pair[0], *value = pair[1];

          if (g_str_has_prefix (key, "file:"))
            epc_publisher_add_file (publisher, key + 5, value);
          else
            epc_publisher_add (publisher, key, value, -1);

          g_strfreev (pair);
        }
    }

  /* Actually run the publisher.
   */
  epc_publisher_run (publisher);
  g_object_unref (publisher);

  return 0;
}
