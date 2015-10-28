/* This example demonstrates looking up Easy Publisher resources.
 * Expects the keys of the resources to lookup on command line.
 * Without any keys the `date-time' resource is looked up.
 *
 * This example (lookup-resource.c) is in the public domain.
 */
#include <libepc/consumer.h>
#include <glib/gi18n.h>

static void
lookup (EpcConsumer *consumer,
        const gchar *key)
{
  GError *error = NULL;
  gchar *value = NULL;
  gsize length = 0;

  /* retrieve the resource matching @key */

  g_print ("looking up `%s':\n", key);
  value = epc_consumer_lookup (consumer, key, &length, &error);

  /* print the retreived value */

  if (value)
    g_print ("%s\n%" G_GSIZE_FORMAT " byte(s)\n", value, length);
  else
    g_print ("%s: %s\n", key, error->message);

  /* release resources */

  g_clear_error (&error);
  g_free (value);
}

int
main (int   argc,
      char *argv[])
{
  GOptionContext *options = NULL;
  EpcConsumer *consumer = NULL;
  GError *error = NULL;
  int i;

  /* Declare command line options. */

  gchar *service_name = "Easy Publisher Test";
  gchar *application = "test-publisher";
  gchar *username = NULL;
  gchar *password = NULL;
  gchar *domain = NULL;

  GOptionEntry entries[] =
    {
      { "service-name", 'n', 0, G_OPTION_ARG_STRING, &service_name,
        N_("Service name of the publisher"), N_("NAME") },
      { "application", 'a', 0, G_OPTION_ARG_STRING, &application,
        N_("Application name of the publisher"), N_("NAME") },
      { "domain", 'd', 0, G_OPTION_ARG_STRING, &domain,
        N_("DNS domain of the publisher"), N_("DOMAIN") },
      { "username", 'u', 0, G_OPTION_ARG_STRING, &username,
        N_("The username to use for authenication"), N_("USERNAME") },
      { "password", 'p', 0, G_OPTION_ARG_STRING, &password,
        N_("The password to use for authenication"), N_("PASSWORD") },
      { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

  /* Initialize the toolkit. */

  /* Parse command line options. */

  options = g_option_context_new (_("[KEYS...] - looks up Easy Publisher resources"));
  g_option_context_add_main_entries (options, entries, NULL);

  if (!g_option_context_parse (options, &argc, &argv, &error))
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return 2;
    }

  g_option_context_free (options);

  /* Create the consumer */

  consumer = epc_consumer_new_for_name_full (service_name, application, domain);

  /* Attach default credentials for authentication, when provided. */

  if (username)
    epc_consumer_set_username (consumer, username);
  if (password)
    epc_consumer_set_password (consumer, password);

  /* Query the resources specified on command line. */

  if (argc > 1)
    for (i = 1; i < argc; ++i)
      lookup (consumer, argv[i]);
  else
    lookup (consumer, "date-time");

  /* Release resources. */

  g_object_unref (consumer);

  return 0;
}
