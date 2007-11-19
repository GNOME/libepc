#include <libepc/consumer.h>
#include <glib/gi18n.h>

static void
list (EpcConsumer *consumer,
      const gchar *pattern)
{
  GError *error = NULL;
  GList *items, *iter;

  g_print ("listing `%s':\n", pattern ? pattern : "*");
  items = epc_consumer_list (consumer, pattern, &error);

  if (error)
    g_print ("- failed: %s\n", error->message);
  else if (NULL == items)
    g_print ("- no items found\n");
  else
    for (iter = items; iter; iter = iter->next)
      g_print ("- item: `%s'\n", (const gchar*) iter->data);

  g_clear_error (&error);
  g_list_foreach (items, (GFunc) g_free, NULL);
  g_list_free (items);
}

int
main (int   argc,
      char *argv[])
{
  GOptionContext *options = NULL;
  gchar *service_name = "Easy Publisher Test";
  gchar *application = "lt-test-publisher";
  gchar *domain = NULL;

  EpcConsumer *consumer = NULL;
  GError *error = NULL;
  int i;

  /* Parse command line options.
   */
  GOptionEntry entries[] =
    {
      { "service-name", 'n', 0, G_OPTION_ARG_STRING, &service_name,
        N_("Service name of the publisher"), N_("NAME") },
      { "application", 'a', 0, G_OPTION_ARG_STRING, &application,
        N_("Application name of the publisher"), N_("NAME") },
      { "domain", 'd', 0, G_OPTION_ARG_STRING, &domain,
        N_("DNS domain of the publisher"), N_("DOMAIN") },
      { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

  options = g_option_context_new (NULL);
  g_option_context_add_main_entries (options, entries, NULL);

  if (!g_option_context_parse (options, &argc, &argv, &error))
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return 2;
    }

  g_option_context_free (options);

  /* Initialize the toolkit.
   */
  g_thread_init (NULL);
  g_type_init ();

  /* Create an consumer and list the request items.
   */
  consumer = epc_consumer_new_for_name_full (service_name, application, domain);

  if (argc > 1)
    for (i = 1; i < argc; ++i)
      list (consumer, argv[i]);
  else
    list (consumer, NULL);

  g_object_unref (consumer);

  return 0;
}
