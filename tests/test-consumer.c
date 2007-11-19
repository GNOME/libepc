#include "libepc/consumer.h"
#include "libepc/publisher.h"
#include "libepc/service-names.h"

#include <avahi-ui/avahi-ui.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

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

  markup = g_markup_printf_escaped (
    "Value stored for <b>%s</b> at <b>%s</b> (%d %s):",
    key, publisher, length, ngettext ("byte", "bytes", length));

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_misc_set_padding (GTK_MISC (label), 6, 6);
  gtk_widget_show (label);
  g_free (markup);

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

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      scroller, TRUE, TRUE, 0);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 350);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
lookup_value (EpcConsumer *consumer,
              const gchar *key,
              const gchar *publisher)
{
  gchar *value;
  gsize length;

  value = epc_consumer_lookup (consumer, key, &length);

  if (value)
    show_value (publisher, key, value, length);
  else
    show_error ("<b>Value not found.</b>\n\n"
                "No value found for key <b>%s</b> at <b>%s</b>.",
                key, publisher);

  g_free (value);
}

int
main (int   argc,
      char *argv[])
{
  EpcConsumer *consumer = NULL;
  gchar *publisher = NULL;
  GtkWidget *dialog;
  int i;

  g_thread_init (NULL);
  gdk_threads_init ();
  gtk_init (&argc, &argv);

  dialog = aui_service_dialog_new ("Choose an Easy Publish and Consume Service", NULL,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT,
                                   NULL);

  aui_service_dialog_set_browse_service_types (AUI_SERVICE_DIALOG (dialog),
					       EPC_SERVICE_NAME, NULL);

  if (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      const gint port = aui_service_dialog_get_port (AUI_SERVICE_DIALOG (dialog));
      const gchar *host = aui_service_dialog_get_host_name (AUI_SERVICE_DIALOG (dialog));
      publisher = g_strdup (aui_service_dialog_get_service_name (AUI_SERVICE_DIALOG (dialog)));
      consumer = epc_consumer_new (host, port);
    }

  gtk_widget_destroy (dialog);

  if (consumer)
    {
      if (argc > 1)
        for (i = 1; i < argc; ++i)
          lookup_value (consumer, argv[i], publisher);
      else
        lookup_value (consumer, "source-code", publisher);

      g_object_unref (consumer);
    }

  g_free (publisher);

  return 0;
}
