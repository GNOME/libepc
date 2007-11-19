#include "libepc/publisher.h"

#include <gtk/gtk.h>
#include <avahi-ui/avahi-ui.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-session-sync.h>

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
show_response (SoupMessage *message)
{
  GtkTextBuffer *buffer;
  GtkWidget *text_view;
  GtkWidget *scroller;
  GtkWidget *dialog;

  dialog = gtk_dialog_new_with_buttons ("Glom Configuration", NULL,
                                        GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                        NULL);

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, message->response.body,
                                    message->response.length);

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
                      scroller, TRUE, TRUE, 0);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 350);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static SoupMessage*
build_document_request (const char *host,
                        int         port,
                        const char *path)
{
  SoupMessage *msg;
  char *uri;

  g_return_val_if_fail (port > 0 && port < 65536, NULL);
  g_return_val_if_fail (NULL != host, NULL);
  g_return_val_if_fail (NULL != path, NULL);

  if (NULL == path)
    path = "/";

  g_return_val_if_fail ('/' == path[0], NULL);

  uri = g_strdup_printf ("http://%s:%d/%s", host, port, path + 1);
  msg = soup_message_new ("GET", uri);

  g_free (uri);

  return msg;
}

static void
authenticate_cb (SoupSession  *session G_GNUC_UNUSED,
                 SoupMessage  *message G_GNUC_UNUSED,
                 gchar        *auth_type G_GNUC_UNUSED,
                 gchar        *auth_realm G_GNUC_UNUSED,
                 gchar       **username,
                 gchar       **password,
                 gpointer      data G_GNUC_UNUSED)
{
  *username = g_strdup (g_get_user_name ());
  *password = g_strdup ("yadda");
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *dialog;

  g_thread_init (NULL);
  gdk_threads_init ();
  gtk_init (&argc, &argv);

  dialog = aui_service_dialog_new ("Choose an Easy Publish and Commit Service",
				   NULL,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   GTK_STOCK_CONNECT, GTK_RESPONSE_ACCEPT,
                                   NULL);

  aui_service_dialog_set_browse_service_types (AUI_SERVICE_DIALOG (dialog),
					       EPC_PUBLISHER_SERVICE_TYPE,
                                               NULL);

  if (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      SoupSession *session;
      SoupMessage *request;
      guint status;

      session = soup_session_sync_new ();

      g_signal_connect (session, "authenticate",
                        G_CALLBACK (authenticate_cb), NULL);

      request = build_document_request (
        aui_service_dialog_get_host_name (AUI_SERVICE_DIALOG (dialog)),
        aui_service_dialog_get_port (AUI_SERVICE_DIALOG (dialog)),
        "/get/source-code");

      status = soup_session_send_message (session, request);

      if (SOUP_STATUS_OK == status)
        show_response (request);
      else
        show_error ("<b>Cannot retreive configuration file:</b>\n\n"
                    "Server response was `%s' (%d).",
                    soup_status_get_phrase (status), status);

      g_object_unref (request);
      g_object_unref (session);
    }

  gtk_widget_destroy (dialog);

  return 0;
}
