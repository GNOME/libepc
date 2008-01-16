/* Easy Publish and Consume Library
 * Copyright (C) 2007, 2008  Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Mathias Hasselmann
 */
#ifndef __EPC_TLS_H__
#define __EPC_TLS_H__

#include <glib-object.h>
#include <gnutls/x509.h>

G_BEGIN_DECLS

/**
 * EPC_TLS_ERROR:
 *
 * Error domain for TLS operations. Errors in this domain will
 * be <citetitle>GNU TLS</citetitle> error codes. See GError
 * for information on error domains.
 */
#define EPC_TLS_ERROR (epc_tls_error_quark ())

/**
 * EPC_TLS_SECONDS_PER_MINUTE:
 *
 * The number of seconds per minute.
 */
#define EPC_TLS_SECONDS_PER_MINUTE  (60)

/**
 * EPC_TLS_SECONDS_PER_HOUR:
 *
 * The number of seconds per hour.
 */
#define EPC_TLS_SECONDS_PER_HOUR    (60 * EPC_TLS_SECONDS_PER_MINUTE)

/**
 * EPC_TLS_SECONDS_PER_DAY:
 *
 * The number of seconds per day.
 */
#define EPC_TLS_SECONDS_PER_DAY     (24 * EPC_TLS_SECONDS_PER_HOUR)

GQuark                epc_tls_error_quark              (void) G_GNUC_CONST;

gnutls_x509_crt_t     epc_tls_certificate_new          (const gchar            *hostname,
                                                        guint                   validity,
                                                        gnutls_x509_privkey_t   key,
                                                        GError                **error);
gnutls_x509_crt_t     epc_tls_certificate_load         (const gchar            *filename,
                                                        GError                **error);
gboolean              epc_tls_certificate_save         (gnutls_x509_crt_t       certificate,
                                                        const gchar            *filename,
                                                        GError                **error);

gnutls_x509_privkey_t epc_tls_private_key_new          (GError                **error);
gnutls_x509_privkey_t epc_tls_private_key_load         (const gchar            *filename,
                                                        GError                **error);
gboolean              epc_tls_private_key_save         (gnutls_x509_privkey_t   key,
                                                        const gchar            *filename,
                                                        GError                **error);

gchar*                epc_tls_get_certificate_filename (const gchar            *hostname);
gchar*                epc_tls_get_private_key_filename (const gchar            *hostname);
gboolean              epc_tls_get_server_credentials   (const gchar            *hostname,
                                                        gchar                 **crtfile,
                                                        gchar                 **keyfile,
                                                        GError                **error);

G_END_DECLS

#endif /* __EPC_TLS_H__ */
