/* Example for using the service monitor.
 * This example (service-browser.c) is in the public domain.
 */
#include <libepc/service-monitor.h>
#include <glib/gi18n.h>

static void
service_found_cb (EpcServiceMonitor *monitor G_GNUC_UNUSED,
                  const gchar       *name,
                  EpcServiceInfo    *info)
{
  AvahiProtocol proto = epc_service_info_get_address_family (info);
  const gchar *iface = epc_service_info_get_interface (info);
  const gchar *path = epc_service_info_get_detail (info, "path");
  const gchar *type = epc_service_info_get_service_type (info);
  const gchar *host = epc_service_info_get_host (info);
  guint port = epc_service_info_get_port (info);

  g_print ("+ %s (type=%s, addr=%s:%d (%s), interface=%s, path=%s)\n",
           name, type, host, port, avahi_proto_to_string (proto),
           iface, path ? path : "");
}

static void
service_removed_cb (EpcServiceMonitor *monitor G_GNUC_UNUSED,
                    const gchar       *name,
                    const gchar       *type)
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
