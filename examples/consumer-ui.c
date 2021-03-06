/* The program demonstrates UI integration of the EpcConsumer.
 * This example (consumer-ui.c) is in the public domain.
 */
#include "config.h"

#include <libepc/consumer.h>
#include <libepc-ui/password-dialog.h>

#include <avahi-ui/avahi-ui.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

static GtkTextBuffer *buffer = NULL;
static GtkWidget *keys_combo = NULL;
static GtkWidget *scroller = NULL;
static GtkWidget *stats = NULL;

static void
list_keys (EpcConsumer *consumer,
           const gchar *publisher)
{
  GError *error = NULL;
  GList *keys, *iter;
  gchar *markup;

  /* retreive a list of all published resources */

  keys = epc_consumer_list (consumer, NULL, &error);

  if (keys)
    {
      /* fill the combo box with all published resources */

      keys = g_list_sort (keys, (GCompareFunc) strcmp);

      for (iter = keys; iter; iter = iter->next)
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (keys_combo), NULL, iter->data);

      gtk_combo_box_set_active (GTK_COMBO_BOX (keys_combo), 0);
      gtk_widget_set_sensitive (keys_combo, TRUE);
    }
  else if (error)
    {
      /* listing failed, show the error message */

      markup = g_markup_printf_escaped ("Failed to list values at <b>%s</b>: %s",
                                        publisher, error->message);
      gtk_label_set_markup (GTK_LABEL (stats), markup);
      g_free (markup);
    }
  else
    gtk_label_set_text (GTK_LABEL (stats), "No publications available.");

  /* release resources */

  g_clear_error (&error);
  g_list_foreach (keys, (GFunc)g_free, NULL);
  g_list_free (keys);
}

static void
keys_combo_changed_cb (GtkComboBox *combo_box,
                       gpointer     data)
{
  EpcConsumer *consumer = data;
  gchar *key, *value = NULL;
  gchar *markup = NULL;
  GError *error = NULL;
  gsize length = 0;

  /* retreive a the currently selected resource */

  key = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box));

  if (key)
    value = epc_consumer_lookup (consumer, key, &length, &error);

  if (value)
    {
      /* display the resource content */

      markup = g_markup_printf_escaped (
        "Length of <b>%s</b>: %" G_GSIZE_FORMAT " %s", key, length,
        ngettext ("byte", "bytes", length));

      gtk_text_buffer_set_text (buffer, value, length);
      gtk_widget_set_sensitive (scroller, TRUE);
    }
  else if (error)
    {
      /* lookup failed, show error message */

        markup = g_markup_printf_escaped (
          "Look failed for <b>%s</b>: %s",
          key, error->message);

      gtk_text_buffer_set_text (buffer, "", 0);
      gtk_widget_set_sensitive (scroller, FALSE);
    }

  if (markup)
    gtk_label_set_markup (GTK_LABEL (stats), markup);

  /* release resources */

  g_free (markup);
  g_free (value);
  g_free (key);
  g_clear_error (&error);
}

static GtkWidget*
create_lookup_dialog (EpcConsumer *consumer,
                      const gchar *publisher)
{
  GtkWidget *text_view, *label;
  GtkWidget *vbox, *dialog;
  gchar *markup;

  /* Setup the combo box listing keys. */

  keys_combo = gtk_combo_box_text_new ();
  gtk_widget_set_sensitive (keys_combo, FALSE);

  g_signal_connect (keys_combo, "changed",
                    G_CALLBACK (keys_combo_changed_cb),
                    consumer);

  /* Setup the combo label. */

  label = gtk_label_new (NULL);
  markup = g_markup_printf_escaped ("Values at <b>%s</b>:", publisher);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_widget_set_halign (label, 0.0);
  gtk_widget_set_valign (label, 0.5);
  g_free (markup);

  /* Setup the text view and its scrollbars. */

  buffer = gtk_text_buffer_new (NULL);
  text_view = gtk_text_view_new_with_buffer (buffer);

  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_add (GTK_CONTAINER (scroller), text_view);
  gtk_widget_set_sensitive (scroller, FALSE);

  /* Setup the stats label. */

  stats = gtk_label_new ("No value selected.");

  /* Attach the widgets to some vertical box. */

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), keys_combo, FALSE, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), scroller, TRUE, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), stats, FALSE, TRUE);
  gtk_widget_set_margin_top (vbox, 6);
  gtk_widget_set_margin_bottom (vbox, 6);
  gtk_widget_set_margin_start (vbox, 6);
  gtk_widget_set_margin_end (vbox, 6);

  /* Create the dialog widget. */

  dialog = gtk_dialog_new_with_buttons ("Published Value", NULL,
                                        (GtkDialogFlags)0,
                                        _("_Close"), GTK_RESPONSE_CLOSE,
                                        NULL);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      vbox, TRUE, TRUE);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 350);

  return dialog;
}

int
main (int   argc,
      char *argv[])
{
  EpcConsumer *consumer = NULL;
  gchar *publisher_name = NULL;
  GtkWidget *password_dialog = NULL;
  GtkWidget *dialog = NULL;
  GError *error = NULL;

  gchar *service_type_https = NULL;
  gchar *service_type_http = NULL;

  /* Declare command line options. */

  const gchar *application = "test-publisher";
  gboolean browse_all = FALSE;


  /* Initialize the toolkit */

  gtk_init ();

  GOptionEntry entries[] =
    {
      { "application", 'n', 0, G_OPTION_ARG_STRING, &application,
        N_("Application name of the publisher"), N_("NAME") },
      { "all",         'a', 0, G_OPTION_ARG_NONE, &browse_all,
        N_("Browse all easy publishers"), NULL },
      { NULL, 0, 0, 0, NULL, NULL, NULL }
    };
  GOptionContext *context = g_option_context_new ("test-publisher");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Usage error: %s\n", error->message);
      g_clear_error (&error);

      return 2;
    }

  /* Show Avahi's stock dialog for choosing a publisher service */

  if (application && *application && !browse_all)
    {
      service_type_https = epc_service_type_new (EPC_PROTOCOL_HTTPS, application);
      service_type_http = epc_service_type_new (EPC_PROTOCOL_HTTP, application);
    }
  else
    {
      service_type_https = g_strdup (EPC_SERVICE_TYPE_HTTPS);
      service_type_http = g_strdup (EPC_SERVICE_TYPE_HTTP);
    }

  dialog = aui_service_dialog_new ("Choose an Easy Publish and Consume Service", NULL,
                                   _("_Cancel"), GTK_RESPONSE_CANCEL,
                                   _("Connect"), GTK_RESPONSE_ACCEPT,
                                   NULL);

  aui_service_dialog_set_browse_service_types (AUI_SERVICE_DIALOG (dialog),
                                               service_type_https,
                                               service_type_http,
                                               NULL);

  g_free (service_type_https);
  g_free (service_type_http);

#ifdef HAVE_SET_SERVICE_TYPE_NAME /* Setup pretty service names */

  aui_service_dialog_set_service_type_name (AUI_SERVICE_DIALOG (dialog),
                                            EPC_SERVICE_TYPE_HTTPS,
                                            "Secure Transport");
  aui_service_dialog_set_service_type_name (AUI_SERVICE_DIALOG (dialog),
                                            EPC_SERVICE_TYPE_HTTP,
                                            "Insecure Transport");
#endif

  if (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      /* Retrieve contact information for the selected service. */

      EpcServiceInfo *service =
        epc_service_info_new (aui_service_dialog_get_service_type (AUI_SERVICE_DIALOG (dialog)),
                              aui_service_dialog_get_host_name    (AUI_SERVICE_DIALOG (dialog)),
                              aui_service_dialog_get_port         (AUI_SERVICE_DIALOG (dialog)),
                              aui_service_dialog_get_txt_data     (AUI_SERVICE_DIALOG (dialog)));

      /* Retrieve the human readable name of the selected service,
       * just for the purpose of displaying it in the UI later. */

      publisher_name = g_strdup (aui_service_dialog_get_service_name (AUI_SERVICE_DIALOG (dialog)));

      /* Create an EpcConsumer for the selected service. */

      consumer = epc_consumer_new (service);
      epc_service_info_unref (service);

      /* Associate a password dialog */

      password_dialog =
        epc_password_dialog_new ("Easy Consumer Test", NULL, NULL,
                                 _("_Cancel"), GTK_RESPONSE_CANCEL,
                                 _("Connect"), GTK_RESPONSE_ACCEPT,
                                 NULL);

      epc_password_dialog_set_anonymous_allowed (EPC_PASSWORD_DIALOG (password_dialog), FALSE);
      epc_password_dialog_attach (EPC_PASSWORD_DIALOG (password_dialog), consumer);
    }

  gtk_widget_destroy (dialog);
  dialog = NULL;

  /* create and run the demo dialog */

  if (consumer)
    dialog = create_lookup_dialog (consumer, publisher_name);

  if (dialog)
    {
      list_keys (consumer, publisher_name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }

  /* release resources */

  if (consumer)
    g_object_unref (consumer);
  if (password_dialog)
    gtk_widget_destroy (password_dialog);
  g_free (publisher_name);

  return 0;
}
