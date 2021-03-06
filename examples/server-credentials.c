/* Example of how to retreive SSL/TLS server credentials.
 * This example (server-credentials.c) is in the public domain.
 */
#include <libepc/tls.h>
#include <libepc-ui/progress-window.h>
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
  gtk_init ();

  /* Show a progress window when generating new keys.
   */
  epc_progress_window_install (NULL);

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
