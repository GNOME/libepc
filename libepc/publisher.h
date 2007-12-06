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
#ifndef __EPC_PUBLISHER_H__
#define __EPC_PUBLISHER_H__

#include <libepc/contents.h>
#include <libepc/dispatcher.h>
#include <libepc/service-type.h>

G_BEGIN_DECLS

#define EPC_TYPE_PUBLISHER           (epc_publisher_get_type())
#define EPC_PUBLISHER(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_PUBLISHER, EpcPublisher))
#define EPC_PUBLISHER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_PUBLISHER, EpcPublisherClass))
#define EPC_IS_PUBLISHER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_PUBLISHER))
#define EPC_IS_PUBLISHER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_PUBLISHER))
#define EPC_PUBLISHER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_PUBLISHER, EpcPublisherClass))

typedef struct _EpcAuthContext                  EpcAuthContext;
typedef struct _EpcPublisher                    EpcPublisher;
typedef struct _EpcPublisherClass               EpcPublisherClass;
typedef struct _EpcPublisherPrivate             EpcPublisherPrivate;

/**
 * EpcContentsHandler:
 * @publisher: the #EpcPublisher
 * @key: the unique key
 * @user_data: user data set when the signal handler was installed
 *
 * This callback is used to generate custom contents published with the
 * #epc_publisher_add_handler function. The arguments passed are the same as
 * passed to #epc_publisher_add_handler. The #EpcPublisher will decrease the
 * reference count of the returned buffer after deliving it. It's valid to
 * return %NULL in situations were no contents can be generated.
 *
 * Returns: The #EpcContents buffer for this publication, or %NULL.
 */
typedef EpcContents* (*EpcContentsHandler) (EpcPublisher   *publisher,
                                            const gchar    *key,
                                            gpointer        user_data);

/**
 * EpcAuthHandler:
 * @context: the #EpcAuthContext
 * @username: the username provided for authentication, or %NULL
 * @user_data: user data set when the signal handler was installed
 *
 * Functions implementing this callback shall return %TRUE when the
 * credentials provided by the authentication request grant access
 * to the resource described by @context.
 *
 * The @username is %NULL when no creditials were passed, and anonymous access
 * is tried.
 * 
 * See also #epc_publisher_set_auth_flags. When EPC_AUTH_DEFAULT is used, 
 * you should call #epc_auth_context_check_password
 * to verify that the password passed in the request matches the known password
 * for that user. In this case there is no way to retrieve the password from 
 * the #EpcAuthContext because the network protocol transfers just a hash code, 
 * not the actual password.
 *
 * However, when EPC_AUTH_PASSWORD_TEXT_NEEDED is used, you should call 
 * epc_auth_context_get_password() and then do your own authentication check. 
 * For instance, you might need to delegate the authentication to some other 
 * code or server, such as a database server.
 *
 * Returns: %TRUE when access is granted, and %FALSE otherwise.
 */
typedef gboolean    (*EpcAuthHandler)    (EpcAuthContext *context,
                                          const gchar    *username,
                                          gpointer        user_data);

/**
 * EpcAuthFlags:
 * @EPC_AUTH_DEFAULT: The default authentication settings.
 * @EPC_AUTH_PASSWORD_TEXT_NEEDED: Set this flag when your #EpcAuthFlags
 * needs the supplied password in plain text - for instance to pass it to a
 * database server used by your application. This flag replaces the secure Digest
 * authentication scheme with the insecure Basic authentication scheme.
 * Therefore this setting is valid only when the publisher's transport
 * protocol is #EPC_PROTOCOL_HTTPS (secure http).
 *
 * These flags specify the authentication behaviour of an #EpcPublisher.
 */

typedef enum /* <flags> */
{
  EPC_AUTH_DEFAULT =                     0,
  EPC_AUTH_PASSWORD_TEXT_NEEDED =       (1 << 0)
} EpcAuthFlags;

/**
 * EpcPublisher:
 *
 * Public fields of the #EpcPublisher class.
 */
struct _EpcPublisher
{
  /*< private >*/
  GObject parent_instance;
  EpcPublisherPrivate *priv;

  /*< public >*/
};

/**
 * EpcPublisherClass:
 *
 * Virtual methods of the #EpcPublisher class.
 */
struct _EpcPublisherClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
};

GType                 epc_publisher_get_type               (void) G_GNUC_CONST;

EpcPublisher*         epc_publisher_new                    (const gchar           *name,
                                                            const gchar           *application,
                                                            const gchar           *domain);

void                  epc_publisher_set_service_name       (EpcPublisher          *publisher,
                                                            const gchar           *name);
void                  epc_publisher_set_credentials        (EpcPublisher          *publisher,
                                                            const gchar           *certfile,
                                                            const gchar           *keyfile);
void                  epc_publisher_set_protocol           (EpcPublisher          *publisher,
                                                            EpcProtocol            protocol);
void                  epc_publisher_set_contents_path      (EpcPublisher          *publisher,
                                                            const gchar           *path);
void                  epc_publisher_set_auth_flags         (EpcPublisher          *publisher,
                                                            EpcAuthFlags           flags);
void                  epc_publisher_set_collision_handling (EpcPublisher          *publisher,
                                                            EpcCollisionHandling   method);
void                  epc_publisher_set_service_cookie     (EpcPublisher          *publisher,
                                                            const gchar           *cookie);

G_CONST_RETURN gchar* epc_publisher_get_service_name       (EpcPublisher          *publisher);
G_CONST_RETURN gchar* epc_publisher_get_service_domain     (EpcPublisher          *publisher);
G_CONST_RETURN gchar* epc_publisher_get_certificate_file   (EpcPublisher          *publisher);
G_CONST_RETURN gchar* epc_publisher_get_private_key_file   (EpcPublisher          *publisher);
EpcProtocol           epc_publisher_get_protocol           (EpcPublisher          *publisher);
G_CONST_RETURN gchar* epc_publisher_get_contents_path      (EpcPublisher          *publisher);
EpcAuthFlags          epc_publisher_get_auth_flags         (EpcPublisher          *publisher);
EpcCollisionHandling  epc_publisher_get_collision_handling (EpcPublisher          *publisher);
G_CONST_RETURN gchar* epc_publisher_get_service_cookie     (EpcPublisher          *publisher);

void                  epc_publisher_add                    (EpcPublisher          *publisher,
                                                            const gchar           *key,
                                                            gconstpointer          data,
                                                            gssize                 length);
void                  epc_publisher_add_file               (EpcPublisher          *publisher,
                                                            const gchar           *key,
                                                            const gchar           *filename);
void                  epc_publisher_add_handler            (EpcPublisher          *publisher,
                                                            const gchar           *key,
                                                            EpcContentsHandler     handler,
                                                            gpointer               user_data,
                                                            GDestroyNotify         destroy_data);

void                  epc_publisher_set_auth_handler       (EpcPublisher          *publisher,
                                                            const gchar           *key,
                                                            EpcAuthHandler         handler,
                                                            gpointer               user_data,
                                                            GDestroyNotify         destroy_data);

void                  epc_publisher_add_bookmark           (EpcPublisher          *publisher,
                                                            const gchar           *key,
                                                            const gchar           *label);

gchar*                epc_publisher_get_path               (EpcPublisher          *publisher,
                                                            const gchar           *key);
gchar*                epc_publisher_get_uri                (EpcPublisher          *publisher,
                                                            const gchar           *key,
                                                            GError               **error);

gboolean              epc_publisher_remove                 (EpcPublisher          *publisher,
                                                            const gchar           *key);
gpointer              epc_publisher_lookup                 (EpcPublisher          *publisher,
                                                            const gchar           *key);
gboolean              epc_publisher_has_key                (EpcPublisher          *publisher,
                                                            const gchar           *key);
GList*                epc_publisher_list                   (EpcPublisher          *publisher,
                                                            const gchar           *pattern);

gboolean              epc_publisher_run                    (EpcPublisher          *publisher,
                                                            GError               **error);
gboolean              epc_publisher_run_async              (EpcPublisher          *publisher,
                                                            GError               **error);
gboolean              epc_publisher_quit                   (EpcPublisher          *publisher);

gchar*                epc_publisher_expand_name            (const gchar           *name,
                                                            GError               **error);

EpcPublisher*         epc_auth_context_get_publisher       (const EpcAuthContext  *context);
G_CONST_RETURN gchar* epc_auth_context_get_key             (const EpcAuthContext  *context);
G_CONST_RETURN gchar* epc_auth_context_get_password        (const EpcAuthContext  *context);
gboolean              epc_auth_context_check_password      (const EpcAuthContext  *context,
                                                            const gchar           *password);

G_END_DECLS

#endif /* __EPC_PUBLISHER_H__ */
