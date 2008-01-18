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
 *
 * This example (simple-publisher.c) is in the public domain.
 */
#include <libepc/publisher.h>

#include <glib/gi18n.h>
#include <locale.h>
#include <string.h>

static EpcProtocol protocol = EPC_PROTOCOL_HTTPS;

static EpcContents*
timestamp_handler (EpcPublisher *publisher G_GNUC_UNUSED,
                   const gchar  *key G_GNUC_UNUSED,
                   gpointer      data)
{
  time_t now = time (NULL);
  struct tm *tm = localtime (&now);
  const gchar *format = data;
  gsize length = 60;
  gchar *buffer;

  /* Create custom content */

  buffer = g_malloc (length);
  length = strftime (buffer, length, format, tm);

  return epc_contents_new ("text/plain", buffer, length, g_free);
}

static gboolean
authentication_handler (EpcAuthContext *context,
                        const gchar    *user_name,
                        gpointer        user_data)
{
  const gchar *password;

  /* Dump the supplied password when plain-text authentication was choosen.
   * See: EPC_AUTH_PASSWORD_TEXT_NEEDED */

  if (NULL != (password = epc_auth_context_get_password (context)))
    g_print ("%s: password='%s'\n", G_STRLOC, password);

  /* Check if he password supplied matches @user_data */

  return
    NULL != user_name &&
    g_str_equal (user_name, g_get_user_name ()) &&
    epc_auth_context_check_password (context, user_data);
}

static gboolean
parse_protocol (const gchar *option G_GNUC_UNUSED,
                const gchar *text,
                gpointer     data G_GNUC_UNUSED,
                GError     **error)
{
  /* Parse @text */

  protocol = epc_protocol_from_name (text, EPC_PROTOCOL_UNKNOWN);

  if (EPC_PROTOCOL_UNKNOWN != protocol)
    return TRUE;

  /* Report parsing error */

  g_set_error (error,
               G_OPTION_ERROR_FAILED,
               G_OPTION_ERROR_BAD_VALUE,
               _("Invalid transport protocol: '%s'"),
               text);

  return FALSE;
}

int
main (int   argc,
      char *argv[])
{
  gboolean basic_auth = FALSE;
  EpcPublisher *publisher;
  GOptionContext *options;
  GError *error = NULL;

  /* Declare command line options. */

  GOptionEntry entries[] =
    {
      { "protocol", 'p', 0, G_OPTION_ARG_CALLBACK, parse_protocol, N_("Transport protocol"), N_("PROTOCOL") },
      { "basic-auth", 'b', 0, G_OPTION_ARG_NONE, &basic_auth, N_("Use plain text authentication"), NULL },
      { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

  /* Initialize the toolkit. */

  setlocale (LC_ALL, "");
  g_thread_init (NULL);
  g_type_init ();

  /* Parse command line options. */

  options = g_option_context_new (NULL);
  g_option_context_add_main_entries (options, entries, NULL);

  if (!g_option_context_parse (options, &argc, &argv, &error))
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return 2;
    }

  g_option_context_free (options);

  /* Create a new publisher. */

  publisher = epc_publisher_new ("Easy Publisher Test",
                                 "test-publisher", NULL);

  epc_publisher_set_protocol (publisher, protocol);

  if (basic_auth)
    epc_publisher_set_auth_flags (publisher, EPC_AUTH_PASSWORD_TEXT_NEEDED);

  /* Create dynamic bookmark for the builtin HTTP server */

  epc_publisher_add_bookmark (publisher, NULL, NULL);

  if (1 == argc)
    {
      /* Publish some default values,
       * as no arguments are passed on the command line. */

      epc_publisher_add (publisher, "test", "value", -1);
      epc_publisher_add_handler (publisher, "date", timestamp_handler, "%x", NULL);
      epc_publisher_add_handler (publisher, "time", timestamp_handler, "%X", NULL);
      epc_publisher_add_handler (publisher, "date-time", timestamp_handler, "%x %X", NULL);

      epc_publisher_add (publisher, "sensitive",
                         "This value is top secret.", -1);
      epc_publisher_set_auth_handler (publisher, "sensitive",
                                      authentication_handler,
                                      "secret" /* user_data */, NULL);

      epc_publisher_add_file (publisher, "source-code", __FILE__);

      /* Create dynamic bookmark for this example */

      epc_publisher_add_bookmark (publisher, "source-code",
                                  "Source code of a simple publisher");
    }
  else
    {
      /* Publish the values passed on the command line. Each argument has the
       * form "key=value". When a key is prefix with the string "file:" the
       * file of the name passed as value is published. */

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

  /* Actually run the publisher. */

  if (!epc_publisher_run (publisher, &error))
    {
      g_print ("Cannot start publisher: %s.\n",
               error ? error->message : "Unknown error");
      g_error_free (error);
      return 2;
    }

  g_object_unref (publisher);

  return 0;
}
