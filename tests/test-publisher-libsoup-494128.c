/* Test handling of libsoup bug #494128 */

#include "libepc/publisher.h"

static gboolean
sync_timeout_cb (gpointer data)
{
  epc_publisher_quit (data);
  return TRUE;
}

static gboolean
async_timeout_cb (gpointer data)
{
  g_main_loop_quit (data);
  return TRUE;
}

int
main (int   argc G_GNUC_UNUSED,
      char *argv[])
{
  gchar *prgname;
  GError *error = NULL;
  EpcPublisher *publisher;
  GMainLoop *loop;
  guint timeout;

  g_print ("0) INITIALIZE\n");

  g_setenv ("EPC_DEBUG", "1", FALSE);
  prgname = g_path_get_basename (argv[0]);
  g_set_prgname (prgname);
  g_free (prgname);

  g_thread_init (NULL);
  g_type_init ();

  loop = g_main_loop_new (NULL, FALSE);

  g_print ("1) CREATE, DESTROY\n");

  publisher = epc_publisher_new (NULL, NULL, NULL);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);
  g_object_unref (publisher);
  g_usleep (250 * 1000);

  g_print ("2) RUN, QUIT\n");

  publisher = epc_publisher_new (NULL, NULL, NULL);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);
  timeout = g_timeout_add (250, sync_timeout_cb, publisher);

  if (!epc_publisher_run (publisher, &error))
    g_error ("%s: %s", G_STRLOC, error->message);

  g_source_remove (timeout);
  g_object_unref (publisher);
  g_usleep (200 * 1000);

  g_print ("3) RUN_ASYNC, QUIT\n");

  publisher = epc_publisher_new (NULL, NULL, NULL);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);
  timeout = g_timeout_add (250, async_timeout_cb, loop);

  if (!epc_publisher_run_async (publisher, &error))
    g_error ("%s: %s", G_STRLOC, error->message);

  g_main_loop_run (loop);

  g_source_remove (timeout);
  g_object_unref (publisher);
  g_usleep (200 * 1000);

  g_print ("4) RUN_ASYNC, RUN, QUIT\n");

  publisher = epc_publisher_new (NULL, NULL, NULL);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);
  timeout = g_timeout_add (250, sync_timeout_cb, publisher);

  if (!epc_publisher_run_async (publisher, &error))
    g_error ("%s: %s", G_STRLOC, error->message);
  if (!epc_publisher_run (publisher, &error))
    g_error ("%s: %s", G_STRLOC, error->message);

  g_source_remove (timeout);
  g_object_unref (publisher);
  g_usleep (200 * 1000);

  g_print ("X) DONE\n");

  g_main_loop_unref (loop);

  return 0;
}
