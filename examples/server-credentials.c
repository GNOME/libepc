/* This example demonstrates how to retreive SSL/TLS server credentials.
 */
#include "libepc/tls.h"

int
main (void)
{
  gchar *contents = NULL;
  gchar *keyfile = NULL;
  gchar *crtfile = NULL;
  GError *error = NULL;

  /* Initialize glib and gnutls
   */
  g_set_prgname ("server-credentials");
  gnutls_global_init ();

  /* Retreive and show filenames of server credentials.
   */
  if (epc_tls_get_server_credentials ("localhost", &crtfile, &keyfile, &error))
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
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_free (keyfile);
  g_free (crtfile);

  return 0;
}
