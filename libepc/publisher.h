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

#include <libepc/service-type.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPC_TYPE_PUBLISHER           (epc_publisher_get_type())
#define EPC_PUBLISHER(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_PUBLISHER, EpcPublisher))
#define EPC_PUBLISHER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_PUBLISHER, EpcPublisherClass))
#define EPC_IS_PUBLISHER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_PUBLISHER))
#define EPC_IS_PUBLISHER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_PUBLISHER))
#define EPC_PUBLISHER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_PUBLISHER, EpcPublisherClass))

typedef struct _EpcAuthContext                  EpcAuthContext;
typedef struct _EpcContent                      EpcContent;
typedef struct _EpcPublisher                    EpcPublisher;
typedef struct _EpcPublisherClass               EpcPublisherClass;
typedef struct _EpcPublisherPrivate             EpcPublisherPrivate;

/**
 * EpcContentHandler:
 * @publisher: the #EpcPublisher
 * @key: the unique key
 * @user_data: user data set when the signal handler was installed
 *
 * This callback is used to generate custom content published with the
 * #epc_publisher_add_handler function. The arguments passed are the same as
 * passed to #epc_publisher_add_handler. The #EpcPublisher will decrease the
 * reference count of the returned buffer after deliving it. It's valid to
 * return %NULL in situations were no content can be generated.
 *
 * Returns: The #EpcContent buffer for this publication, or %NULL.
 */
typedef EpcContent* (*EpcContentHandler) (EpcPublisher   *publisher,
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
 * is tried. When a @username was passed call #epc_auth_context_check_password
 * to verify that the password passed in the request matches the known password
 * for that user.
 *
 * Returns: %TRUE when access is granted, and %FALSE otherwise.
 */
typedef gboolean    (*EpcAuthHandler)    (EpcAuthContext *context,
                                          const gchar    *username,
                                          gpointer        user_data);

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

GType                 epc_publisher_get_type             (void) G_GNUC_CONST;

EpcPublisher*         epc_publisher_new                  (const gchar       *name,
                                                          const gchar       *domain);

void                  epc_publisher_set_service_name     (EpcPublisher      *publisher,
                                                          const gchar       *name);
void                  epc_publisher_set_credentials      (EpcPublisher      *publisher,
                                                          const gchar       *certfile,
                                                          const gchar       *keyfile);
void                  epc_publisher_set_protocol         (EpcPublisher      *publisher,
                                                          EpcProtocol        protocol);

G_CONST_RETURN gchar* epc_publisher_get_service_name     (EpcPublisher      *publisher);
G_CONST_RETURN gchar* epc_publisher_get_service_domain   (EpcPublisher      *publisher);
G_CONST_RETURN gchar* epc_publisher_get_certificate_file (EpcPublisher      *publisher);
G_CONST_RETURN gchar* epc_publisher_get_private_key_file (EpcPublisher      *publisher);
EpcProtocol           epc_publisher_get_protocol         (EpcPublisher      *publisher);

void                  epc_publisher_add                  (EpcPublisher      *publisher,
                                                          const gchar       *key,
                                                          const gchar       *value,
                                                          gssize             length);
void                  epc_publisher_add_file             (EpcPublisher      *publisher,
                                                          const gchar       *key,
                                                          const gchar       *filename);
void                  epc_publisher_add_handler          (EpcPublisher      *publisher,
                                                          const gchar       *key,
                                                          EpcContentHandler  handler,
                                                          gpointer           user_data,
                                                          GDestroyNotify     destroy_data);

void                  epc_publisher_set_auth_handler     (EpcPublisher      *publisher,
                                                          const gchar       *key,
                                                          EpcAuthHandler     handler,
                                                          gpointer           user_data,
                                                          GDestroyNotify     destroy_data);

void                  epc_publisher_run                  (EpcPublisher      *publisher);
void                  epc_publisher_run_async            (EpcPublisher      *publisher);
void                  epc_publisher_quit                 (EpcPublisher      *publisher);

EpcPublisher*         epc_auth_context_get_publisher     (EpcAuthContext    *context);
G_CONST_RETURN gchar* epc_auth_context_get_key           (EpcAuthContext    *context);
gboolean              epc_auth_context_check_password    (EpcAuthContext    *context,
                                                          const gchar       *password);

EpcContent*           epc_content_new                    (const gchar       *type,
                                                          gpointer           data,
                                                          gsize              length);
EpcContent*           epc_content_ref                    (EpcContent        *content);
void                  epc_content_unref                  (EpcContent        *content);

G_END_DECLS

#endif /* __EPC_PUBLISHER_H__ */
