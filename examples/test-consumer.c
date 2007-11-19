/* This program demonstrates and tests lookup of published values.
 *
 * Usage: test-consumer [KEY...]
 *
 * When key names are passed those are used for lookup,
 * otherwise value for the key "source-code" is retieved.
 */
#include "libepc/consumer.h"
#include "libepc-ui/password-dialog.h"

#include <avahi-ui/avahi-ui.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* show_error:
 * @message: the message to show
 * @...: arguments for @message
 *
 * Creates a dialog to show an error message.
 */
static void
show_error (const char *message, ...)
{
  GtkWidget *dialog;
  gchar *markup;
  va_list args;

  va_start (args, message);
  markup = g_markup_vprintf_escaped (message, args);
  va_end (args);

  dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, NULL);
  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);
  g_free (markup);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* show_value:
 * @publisher: the publisher's name
 * @key: the identifier of the value shown
 * @value: the value to show
 * @lenght: the length of @value in bytes
 *
 * Creates a dialog to show a text value.
 */
static void
show_value (const gchar *publisher,
            const gchar *key,
            const gchar *value,
            gsize        length)
{
  GtkTextBuffer *buffer;

  GtkWidget *text_view;
  GtkWidget *scroller;
  GtkWidget *dialog;
  GtkWidget *label;

  gchar *markup;

  dialog = gtk_dialog_new_with_buttons ("Published Value", NULL,
                                        GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                        NULL);

  /* Setup the heading label.
   */
  markup = g_markup_printf_escaped (
    "Value stored for <b>%s</b> at <b>%s</b> (%d %s):",
    key, publisher, length, ngettext ("byte", "bytes", length));

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_misc_set_padding (GTK_MISC (label), 6, 6);
  gtk_widget_show (label);
  g_free (markup);

  /* Setup the text view and its scrollbars.
   */
  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, value, length);

  text_view = gtk_text_view_new_with_buffer (buffer);

  scroller = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_set_border_width (GTK_CONTAINER (scroller), 6);
  gtk_container_add (GTK_CONTAINER (scroller), text_view);
  gtk_widget_show_all (scroller);

  /* Attach the widgets to the dialog and show the dialog.
   */
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      scroller, TRUE, TRUE, 0);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 350);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* lookup_value:
 * @consumer: the #EpcConsumer
 * @key: the key identifying the value
 * @publisher: the name of the publisher
 *
 * Looks up a value identifyed by @key using the @consumer passed.
 */
static void
lookup_value (EpcConsumer *consumer,
              const gchar *key,
              const gchar *publisher)
{
  const gchar *details;
  GError *error = NULL;
  gchar *value;
  gsize length;

  value = epc_consumer_lookup (consumer, key, &length, &error);

  if (value)
    show_value (publisher, key, value, length);
  else
    {
      details = error ? error->message : "No error details available.";

      show_error ("<b>Lookup Failed</b>\n\n"
                  "Failed to lookup <b>%s</b> at <b>%s</b>: %s",
                  key, publisher, details);
    }

  g_clear_error (&error);
  g_free (value);
}

int
main (int   argc,
      char *argv[])
{
  EpcConsumer *consumer = NULL;
  gchar *publisher_name = NULL;
  int i;

  GtkWidget *password_dialog = NULL;
  GtkWidget *dialog;

  /* Initialize the toolkit */

  g_thread_init (NULL);
  gdk_threads_init ();
  gtk_init (&argc, &argv);

  /* Show Avahi's stock dialog for choosing a publisher service.
   */
  dialog = aui_service_dialog_new ("Choose an Easy Publish and Consume Service", NULL,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT,
                                   NULL);

  aui_service_dialog_set_browse_service_types (AUI_SERVICE_DIALOG (dialog),
					       EPC_SERVICE_TYPE_HTTPS,
					       EPC_SERVICE_TYPE_HTTP,
                                               NULL);

#ifdef HAVE_AVAHI_UI_0_6_22
  aui_service_dialog_set_service_type_name (AUI_SERVICE_DIALOG (dialog),
                                            EPC_SERVICE_TYPE_HTTPS,
                                            "Secure Transport");
  aui_service_dialog_set_service_type_name (AUI_SERVICE_DIALOG (dialog),
                                            EPC_SERVICE_TYPE_HTTP,
                                            "Insecure Transport");
#endif

  if (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      /* Retrieve contact information for the selected service.
       */
      const gint port = aui_service_dialog_get_port (AUI_SERVICE_DIALOG (dialog));
      const gchar *host = aui_service_dialog_get_host_name (AUI_SERVICE_DIALOG (dialog));
      const gchar *transport = aui_service_dialog_get_service_type (AUI_SERVICE_DIALOG (dialog));

      /* Retrieve the human readable name of the selected service,
       * just for the purpose of displaying it in the UI later.
       */
      publisher_name = g_strdup (aui_service_dialog_get_service_name (AUI_SERVICE_DIALOG (dialog)));

      /* Create an EpcConsumer for the selected service.
       */
      consumer = epc_consumer_new (epc_service_type_get_protocol (transport), host, port);

      /* Associate a password dialog */

      password_dialog =
        epc_password_dialog_new ("Easy Consumer Test", NULL, NULL,
                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                 GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT,
                                 NULL);

      epc_password_dialog_set_anonymous_allowed (EPC_PASSWORD_DIALOG (password_dialog), FALSE);
      epc_password_dialog_attach (EPC_PASSWORD_DIALOG (password_dialog), consumer);
    }

  gtk_widget_destroy (dialog);

  if (consumer)
    {
      /* Lookup the values requested on the command line,
       * or "source-code" when no arguments were passed.
       */
      if (argc > 1)
        for (i = 1; i < argc; ++i)
          lookup_value (consumer, argv[i], publisher_name);
      else
        lookup_value (consumer, "source-code", publisher_name);

      g_object_unref (consumer);
    }

  if (password_dialog)
    gtk_widget_destroy (password_dialog);

  g_free (publisher_name);

  return 0;
}
