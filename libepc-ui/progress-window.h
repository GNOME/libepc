/* Easy Publish and Consume Library
 * Copyright (C) 2007, 2008  Openismus GmbH
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
#ifndef __EPC_PROGRESS_WINDOW_H__
#define __EPC_PROGRESS_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPC_TYPE_PROGRESS_WINDOW           (epc_progress_window_get_type())
#define EPC_PROGRESS_WINDOW(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_PROGRESS_WINDOW, EpcProgressWindow))
#define EPC_PROGRESS_WINDOW_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST(cls, EPC_TYPE_PROGRESS_WINDOW, EpcProgressWindowClass))
#define EPC_IS_PROGRESS_WINDOW(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_PROGRESS_WINDOW))
#define EPC_IS_PROGRESS_WINDOW_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_PROGRESS_WINDOW))
#define EPC_PROGRESS_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_PROGRESS_WINDOW, EpcProgressWindowClass))

typedef struct _EpcProgressWindow        EpcProgressWindow;
typedef struct _EpcProgressWindowClass   EpcProgressWindowClass;
typedef struct _EpcProgressWindowPrivate EpcProgressWindowPrivate;

/**
 * EpcProgressWindow:
 *
 * Public fields of the #EpcProgressWindow class.
 */
struct _EpcProgressWindow
{
  /*< private >*/
  GtkWindow parent_instance;
  EpcProgressWindowPrivate *priv;

  /*< public >*/
};

/**
 * EpcProgressWindowClass:
 *
 * Virtual methods of the #EpcProgressWindow class.
 */
struct _EpcProgressWindowClass
{
  /*< private >*/
  GtkWindowClass parent_class;

  /*< public >*/
};

GType      epc_progress_window_get_type (void) G_GNUC_CONST;

GtkWidget* epc_progress_window_new      (const gchar       *title,
                                         GtkWindow         *parent,
                                         const gchar       *message);
void       epc_progress_window_update   (EpcProgressWindow *window,
                                         gdouble            progress,
                                         const gchar       *message);

void       epc_progress_window_install  (GtkWindow         *parent);

G_END_DECLS

#endif /* __EPC_PROGRESS_WINDOW_H__ */
