/* Easy Publish and Consume Library
 * Copyright (C) 2007  Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *      Mathias Hasselmann
 */
#include "tls.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#define epc_tls_check(result) G_STMT_START{     \
  if (GNUTLS_E_SUCCESS != (result))             \
    goto out;                                   \
}G_STMT_END

typedef struct _EcpTlsKeyContext EcpTlsKeyContext;

struct _EcpTlsKeyContext
{
  gnutls_x509_privkey_t key;
  GMainLoop *loop;
  gint rc;
};

static gpointer epc_tls_private_key_enter_default (void);

static EpcTlsPrivkeyEnterHook epc_tls_private_key_enter = epc_tls_private_key_enter_default;
static EpcTlsPrivkeyLeaveHook epc_tls_private_key_leave = NULL;

extern gboolean _epc_debug;

GQuark
epc_tls_error_quark (void)
{
  return g_quark_from_static_string ("epc-tls-error-quark");
}

static gchar*
epc_tls_get_filename (const gchar *basename,
                      const gchar *category)
{
  const gchar *progname = g_get_prgname ();

  g_return_val_if_fail (NULL != basename, NULL);
  g_return_val_if_fail (NULL != category, NULL);

  if (NULL == progname)
    {
      g_warning ("%s: No program name set. "
                 "Consider calling g_set_prgname().",
                 G_STRLOC);

      progname = "";
    }

  return g_build_filename (g_get_user_config_dir (), progname,
                           "libepc", category, basename, NULL);
}

gchar*
epc_tls_get_private_key_filename (const gchar *basename)
{
  return epc_tls_get_filename (basename, "keys");
}

gchar*
epc_tls_get_certificate_filename (const gchar *basename)
{
  return epc_tls_get_filename (basename, "certs");
}

static gpointer
epc_tls_private_key_enter_default (void)
{
  g_print (_("Generating server key. This may take some time. "
             "Type on the keyboard, move your mouse or "
             "browse the web to generate some entropy.\n"));

  return NULL;
}

void
epc_tls_set_private_key_hooks (EpcTlsPrivkeyEnterHook enter,
                               EpcTlsPrivkeyLeaveHook leave)
{
  epc_tls_private_key_enter = enter;
  epc_tls_private_key_leave = leave;
}

static gpointer
epc_tls_private_key_thread (gpointer data)
{
  EcpTlsKeyContext *context = data;

  context->rc = gnutls_x509_privkey_generate (context->key, GNUTLS_PK_RSA, 1024, 0);
  g_main_loop_quit (context->loop);

  return NULL;
}

gnutls_x509_privkey_t
epc_tls_private_key_new (GError **error)
{
  EcpTlsKeyContext context = { NULL, NULL, GNUTLS_E_SUCCESS };
  gpointer private_key_data = NULL;

  if (epc_tls_private_key_enter)
    private_key_data = epc_tls_private_key_enter ();

  context.rc = gnutls_x509_privkey_init (&context.key);
  epc_tls_check (context.rc);

  if (g_thread_supported ())
    {
      context.loop = g_main_loop_new (NULL, FALSE);
      g_thread_create (epc_tls_private_key_thread, &context, FALSE, NULL);
      g_main_loop_run (context.loop);
      g_main_loop_unref (context.loop);
    }
  else
    epc_tls_private_key_thread (&context);

  epc_tls_check (context.rc);

out:
  if (epc_tls_private_key_leave)
    epc_tls_private_key_leave (private_key_data);

  if (GNUTLS_E_SUCCESS != context.rc)
    {
      g_set_error (error, EPC_TLS_ERROR, context.rc,
                   _("Cannot create private server key: %s"),
                   gnutls_strerror (context.rc));

      if (context.key)
        gnutls_x509_privkey_deinit (context.key);

      context.key = NULL;
    }

  return context.key;
}

gnutls_x509_privkey_t
epc_tls_private_key_load (const gchar *filename,
                          GError     **error)
{
  gnutls_x509_privkey_t key = NULL;
  gint rc = GNUTLS_E_SUCCESS;
  gchar *contents = NULL;
  gnutls_datum_t buffer;

  g_return_val_if_fail (NULL != filename, NULL);

  if (g_file_get_contents (filename, &contents, &buffer.size, error))
    {
      if (G_UNLIKELY (_epc_debug))
        g_debug ("%s: Loading private key `%s'", G_STRLOC, filename);

      buffer.data = (guchar*) contents;

      epc_tls_check (rc = gnutls_x509_privkey_init (&key));
      epc_tls_check (rc = gnutls_x509_privkey_import (key, &buffer, GNUTLS_X509_FMT_PEM));
    }

out:
  if (GNUTLS_E_SUCCESS != rc)
    {
      g_set_error (error, EPC_TLS_ERROR, rc,
                   _("Cannot import private server key '%s': %s"),
                   filename, gnutls_strerror (rc));

      if (key)
        gnutls_x509_privkey_deinit (key);

      key = NULL;
    }

  g_free (contents);

  return key;
}

gboolean
epc_tls_private_key_save (gnutls_x509_privkey_t  key,
                          const gchar           *filename,
                          GError               **error)
{
  gint rc = GNUTLS_E_SUCCESS;
  gchar *display_name = NULL;
  guchar *contents = NULL;
  gchar *dirname = NULL;
  gsize length = 0;
  gint fd = -1;

  g_return_val_if_fail (NULL != key, FALSE);
  g_return_val_if_fail (NULL != filename, FALSE);

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: Writing server key `%s'", G_STRLOC, filename);

  rc = gnutls_x509_privkey_export (key, GNUTLS_X509_FMT_PEM, NULL, &length);
  g_assert (GNUTLS_E_SHORT_MEMORY_BUFFER == rc);

  contents = g_malloc (length);

  if (GNUTLS_E_SUCCESS != (rc =
      gnutls_x509_privkey_export (key, GNUTLS_X509_FMT_PEM, contents, &length)))
    {
      g_set_error (error, EPC_TLS_ERROR, rc,
                   _("Cannot export private key to PEM format: %s"),
                   gnutls_strerror (rc));

      goto out;
    }

  if (g_mkdir_with_parents (dirname = g_path_get_dirname (filename), 0700))
    {
      display_name = g_filename_display_name (dirname);
      rc = GNUTLS_E_FILE_ERROR;

      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to create private key folder '%s': %s"),
                   display_name, g_strerror (errno));

      goto out;
    }

  fd = g_open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);

  if (-1 == fd)
    {
      display_name = g_filename_display_name (filename);
      rc = GNUTLS_E_FILE_ERROR;

      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to create private key file '%s': %s"),
                   display_name, g_strerror (errno));

      goto out;
    }

  if (write (fd, contents, length) < (gssize)length)
    {
      display_name = g_filename_display_name (filename);
      rc = GNUTLS_E_FILE_ERROR;

      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to write private key file '%s': %s"),
                   display_name, g_strerror (errno));

      goto out;
    }

out:
  if (-1 != fd)
    close (fd);

  if (GNUTLS_E_SUCCESS != rc)
    g_unlink (filename);

  g_free (display_name);
  g_free (contents);
  g_free (dirname);

  return (GNUTLS_E_SUCCESS == rc);
}

gnutls_x509_crt_t
epc_tls_certificate_new (const gchar            *hostname,
                         guint                   validity,
                         gnutls_x509_privkey_t   key,
                         GError                **error)
{
  gint rc = GNUTLS_E_SUCCESS;
  gnutls_x509_crt_t crt = NULL;
  time_t now = time (NULL);

  g_return_val_if_fail (NULL != key, NULL);
  g_return_val_if_fail (NULL != hostname, NULL);

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: Generating self signed server certificate for `%s'", G_STRLOC, hostname);

  epc_tls_check (rc = gnutls_x509_crt_init (&crt));
  epc_tls_check (rc = gnutls_x509_crt_set_version (crt, 1));
  epc_tls_check (rc = gnutls_x509_crt_set_key (crt, key));
  epc_tls_check (rc = gnutls_x509_crt_set_serial (crt, "", 1));
  epc_tls_check (rc = gnutls_x509_crt_set_activation_time (crt, now));
  epc_tls_check (rc = gnutls_x509_crt_set_expiration_time (crt, now + validity));
  epc_tls_check (rc = gnutls_x509_crt_set_subject_alternative_name (crt, GNUTLS_SAN_DNSNAME, hostname));
  epc_tls_check (rc = gnutls_x509_crt_set_dn_by_oid (crt, GNUTLS_OID_X520_COMMON_NAME, 0, hostname, strlen (hostname)));
  epc_tls_check (rc = gnutls_x509_crt_sign (crt, crt, key));

out:
  if (GNUTLS_E_SUCCESS != rc)
    {
      g_set_error (error, EPC_TLS_ERROR, rc,
                   _("Cannot create self signed server key for '%s': %s"),
                   hostname, gnutls_strerror (rc));

      if (crt)
        gnutls_x509_crt_deinit (crt);

      crt = NULL;
    }

  return crt;
}

gnutls_x509_crt_t
epc_tls_certificate_load (const gchar *filename,
                          GError     **error)
{
  gint rc = GNUTLS_E_SUCCESS;
  gchar *contents = NULL;

  gnutls_x509_crt_t crt = NULL;
  gnutls_datum_t buffer;

  g_return_val_if_fail (NULL != filename, NULL);

  if (g_file_get_contents (filename, &contents, &buffer.size, error))
    {
      if (G_UNLIKELY (_epc_debug))
        g_debug ("%s: Loading server certificate `%s'", G_STRLOC, filename);

      buffer.data = (guchar*) contents;

      epc_tls_check (rc = gnutls_x509_crt_init (&crt));
      epc_tls_check (rc = gnutls_x509_crt_import (crt, &buffer, GNUTLS_X509_FMT_PEM));
    }

out:
  if (GNUTLS_E_SUCCESS != rc)
    {
      g_set_error (error, EPC_TLS_ERROR, rc,
                   _("Cannot import server certificate '%s': %s"),
                   filename, gnutls_strerror (rc));

      if (crt)
        gnutls_x509_crt_deinit (crt);

      crt = NULL;
    }

  g_free (contents);

  return crt;
}

gboolean
epc_tls_certificate_save (gnutls_x509_crt_t  crt,
                          const gchar       *filename,
                          GError           **error)
{
  gint rc = GNUTLS_E_SUCCESS;
  gchar *display_name = NULL;
  guchar *contents = NULL;
  gchar *dirname = NULL;
  gsize length = 0;

  g_return_val_if_fail (NULL != crt, FALSE);
  g_return_val_if_fail (NULL != filename, FALSE);

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: Writing server certificate `%s'", G_STRLOC, filename);

  rc = gnutls_x509_crt_export (crt, GNUTLS_X509_FMT_PEM, NULL, &length);
  g_assert (GNUTLS_E_SHORT_MEMORY_BUFFER == rc);

  contents = g_malloc (length);

  if (GNUTLS_E_SUCCESS != (rc =
      gnutls_x509_crt_export (crt, GNUTLS_X509_FMT_PEM, contents, &length)))
    {
      g_set_error (error, EPC_TLS_ERROR, rc,
                   _("Cannot export server certificate to PEM format: %s"),
                   gnutls_strerror (rc));

      goto out;
    }

  if (g_mkdir_with_parents (dirname = g_path_get_dirname (filename), 0700))
    {
      display_name = g_filename_display_name (dirname);
      rc = GNUTLS_E_FILE_ERROR;

      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to create server certificate folder '%s': %s"),
                   display_name, g_strerror (errno));

      goto out;
    }

  if (!g_file_set_contents (filename, (gchar*)contents, length, error))
    rc = GNUTLS_E_FILE_ERROR;

out:
  g_free (display_name);
  g_free (contents);
  g_free (dirname);

  return (GNUTLS_E_SUCCESS == rc);
}

gboolean
epc_tls_get_server_credentials (const gchar  *hostname,
                                gchar       **crtfile,
                                gchar       **keyfile,
                                GError      **error)
{
  gboolean success = FALSE;

  struct stat keyinfo, crtinfo;
  gnutls_x509_privkey_t key = NULL;
  gnutls_x509_crt_t crt = NULL;

  gchar *_keyfile = NULL;
  gchar *_crtfile = NULL;

  g_return_val_if_fail (NULL != hostname, FALSE);
  g_return_val_if_fail (NULL != crtfile, FALSE);
  g_return_val_if_fail (NULL != keyfile, FALSE);

  _crtfile = epc_tls_get_certificate_filename (hostname);
  _keyfile = epc_tls_get_private_key_filename (hostname);

  if (NULL == (key = epc_tls_private_key_load (_keyfile, NULL)))
    {
      if (!(key = epc_tls_private_key_new (error)) ||
          !(epc_tls_private_key_save (key, _keyfile, error)))
        goto out;
    }

  if (0 == g_stat (_keyfile, &keyinfo) &&
      0 == g_stat (_crtfile, &crtinfo) &&
      keyinfo.st_mtime <= crtinfo.st_mtime)
    crt = epc_tls_certificate_load (_crtfile, NULL);

  if (NULL != crt)
    {
      time_t now = time (NULL);
      gboolean invalid = TRUE;

      if (!gnutls_x509_crt_check_hostname (crt, hostname))
        g_warning ("%s: The certificate's owner doesn't match hostname '%s'.", G_STRLOC, hostname);
      else if (gnutls_x509_crt_get_activation_time (crt) > now)
        g_warning ("%s: The certificate is not yet activated.", G_STRLOC);
      else if (gnutls_x509_crt_get_expiration_time (crt) < now)
        g_warning ("%s: The certificate has expired.", G_STRLOC);
      else
        invalid = FALSE;

      if (invalid)
        {
          g_warning ("%s: Discarding invalid server certificate.", G_STRLOC);
          gnutls_x509_crt_deinit (crt);
          crt = NULL;
        }
    }

  if (NULL == crt)
    {
      if (!(crt = epc_tls_certificate_new (hostname, 365 * EPC_TLS_SECONDS_PER_DAY, key, error)) ||
          !(epc_tls_certificate_save (crt, _crtfile, error)))
        goto out;
    }

  success = TRUE;

out:
  if (!success)
    {
      g_free (_keyfile);
      g_free (_crtfile);

      _keyfile = NULL;
      _crtfile = NULL;
    }

  if (key)
    gnutls_x509_privkey_deinit (key);
  if (crt)
    gnutls_x509_crt_deinit (crt);

  *keyfile = _keyfile;
  *crtfile = _crtfile;

  return success;
}
