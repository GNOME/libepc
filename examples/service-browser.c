#include <libepc/service-monitor.h>
#include <glib/gi18n.h>

static void
service_found_cb (EpcServiceMonitor *monitor G_GNUC_UNUSED,
                  const gchar       *type,
                  const gchar       *name,
                  const gchar       *host,
                  guint              port)
{
  g_print ("+ %s (%s, %s:%d)\n", name, type, host, port);
}

static void
service_removed_cb (EpcServiceMonitor *monitor G_GNUC_UNUSED,
                    const gchar       *type,
                    const gchar       *name)
{
  g_print ("- %s (%s)\n", name, type);
}

int
main (int   argc,
      char *argv[])
{
  EpcServiceMonitor *monitor;
  GOptionContext *options;
  GError *error = NULL;

  /* Declare command line options. */

  gchar *application = NULL;
  gchar *domain = NULL;

  GOptionEntry entries[] =
    {
      { "application", 'a', 0, G_OPTION_ARG_STRING, &application, N_("Application to look for"), N_("NAME") },
      { "domain", 'd', 0, G_OPTION_ARG_STRING, &domain, N_("The DNS domain to monitor"), N_("NAME") },
      { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

  /* Initialize the toolkit. */

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

  /* Create the service monitor. */

  monitor = epc_service_monitor_new (application, domain, EPC_PROTOCOL_UNKNOWN);

  g_signal_connect (monitor, "service-removed", G_CALLBACK (service_removed_cb), NULL);
  g_signal_connect (monitor, "service-found", G_CALLBACK (service_found_cb), NULL);

  /* Run the service monitor. */

  g_main_loop_run (g_main_loop_new (NULL, FALSE));

  return 1;
}
