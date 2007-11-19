/* This example demonstrates how to retreive SSL/TLS server credentials.
 */
#include "libepc/tls.h"
#include "libepc-ui/entropy-window.h"
#include <gtk/gtk.h>

int
main (int   argc,
      char *argv[])
{
  const gchar *hostname = (argc > 1 ? argv[1] : "localhost");

  gchar *contents = NULL;
  gchar *keyfile = NULL;
  gchar *crtfile = NULL;
  GError *error = NULL;

  /* Initialize GTK+ and gnutls
   */
  gnutls_global_init ();
  g_thread_init (NULL);
  gdk_threads_init ();
  gtk_init (&argc, &argv);

  /* Show a progress window when generating new keys.
   */
  epc_entropy_window_install ();

  /* Retreive and display of server credentials.
   */
  if (epc_tls_get_server_credentials (hostname, &crtfile, &keyfile, &error))
    {
      g_print ("private key file: %s\n", keyfile);

      if (!g_file_get_contents (keyfile, &contents, NULL, &error))
        goto out;

      g_print ("%s", contents);
      g_free (contents);
      contents = NULL;

      g_print ("server certificate file: %s\n", crtfile);

      if (!g_file_get_contents (crtfile, &contents, NULL, &error))
        goto out;

      g_print ("%s", contents);
      g_free (contents);
      contents = NULL;
    }

out:
  /* Cleanup.
   */
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_free (keyfile);
  g_free (crtfile);

  return 0;
}
