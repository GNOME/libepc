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
#ifndef __EPC_ENTROPY_WINDOW_H__
#define __EPC_ENTROPY_WINDOW_H__

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define EPC_TYPE_ENTROPY_WINDOW           (epc_entropy_window_get_type())
#define EPC_ENTROPY_WINDOW(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_ENTROPY_WINDOW, EpcEntropyWindow))
#define EPC_ENTROPY_WINDOW_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_ENTROPY_WINDOW, EpcEntropyWindowClass))
#define EPC_IS_ENTROPY_WINDOW(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_ENTROPY_WINDOW))
#define EPC_IS_ENTROPY_WINDOW_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_ENTROPY_WINDOW))
#define EPC_ENTROPY_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_ENTROPY_WINDOW, EpcEntropyWindowClass))

typedef struct _EpcEntropyWindow        EpcEntropyWindow;
typedef struct _EpcEntropyWindowClass   EpcEntropyWindowClass;
typedef struct _EpcEntropyWindowPrivate EpcEntropyWindowPrivate;

/**
 * EpcEntropyWindow:
 *
 * Public fields of the #EpcEntropyWindow class.
 */
struct _EpcEntropyWindow
{
  /*< private >*/
  GtkWindow parent_instance;
  EpcEntropyWindowPrivate *priv;

  /*< public >*/
};

/**
 * EpcEntropyWindowClass:
 *
 * Virtual methods of the #EpcEntropyWindow class.
 */
struct _EpcEntropyWindowClass
{
  /*< private >*/
  GtkWindowClass parent_class;

  /*< public >*/
};

GType      epc_entropy_window_get_type (void) G_GNUC_CONST;
GtkWidget* epc_entropy_window_new      (void);
void       epc_entropy_window_install  (void);

G_END_DECLS

#endif /* __EPC_ENTROPY_WINDOW_H__ */
