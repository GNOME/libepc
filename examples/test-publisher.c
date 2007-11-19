/* This program demonstrates and tests publishing of values.
 *
 * Usage: test-publisher [KEY=VALUE [KEY=VALUE...]]
 *
 * The arguments have the form "key=value". When a key is prefix with
 * the string "file:" the file of the name passed as value is published.
 *
 * When no arguments are passed some default values are published,
 * including the contents of this file by the key "source-code".
 *
 * To access the default resource called "sensitive"
 * use your login name as user name and the word "secret" as password.
 */
#include "libepc/publisher.h"

#include <locale.h>
#include <string.h>

static EpcContent*
timestamp_handler (EpcPublisher *publisher G_GNUC_UNUSED,
                   const gchar  *key G_GNUC_UNUSED,
                   gpointer      data)
{
  const gchar *format = data;
  gsize bufsize = 60;
  gchar *buffer;
  struct tm *tm;
  time_t now;

  now = time (NULL);
  tm = localtime (&now);
  buffer = g_malloc (bufsize);
  strftime (buffer, bufsize, format, tm);

  return epc_content_new ("text/plain", buffer, strlen (buffer));
}

static gboolean
authentication_handler (EpcAuthContext *context,
                        const gchar    *user_name,
                        gpointer        user_data)
{
  return
    NULL != user_name &&
    g_str_equal (user_name, g_get_user_name ()) &&
    epc_auth_context_check_password (context, user_data);
}

int
main (int   argc,
      char *argv[])
{
  EpcPublisher *publisher;

  /* Initialize the toolkit.
   */
  setlocale (LC_ALL, "");
  g_thread_init (NULL);
  g_type_init ();

  /* Create a new publisher.
   */
  publisher = epc_publisher_new ("Easy Publisher Test", NULL, NULL);

  if (1 == argc)
    {
      /* Publish some default values,
       * as no arguments are passed on the command line.
       */
      epc_publisher_add (publisher, "test", "value", -1);
      epc_publisher_add_file (publisher, "source-code", __FILE__);
      epc_publisher_add_handler (publisher, "date", timestamp_handler, "%x", NULL);
      epc_publisher_add_handler (publisher, "time", timestamp_handler, "%X", NULL);
      epc_publisher_add_handler (publisher, "date-time", timestamp_handler, "%x %X", NULL);

      epc_publisher_add (publisher, "sensitive",
                         "This value is top secret.", -1);
      epc_publisher_set_auth_handler (publisher, "sensitive",
                                      authentication_handler,
                                      "secret", NULL);
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
