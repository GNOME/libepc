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
#include "entropy-window.h"

#include <libepc/tls.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

/**
 * SECTION:entropy-window
 * @short_description: feedback when collecting random bits
 * @include: libepc-ui/entropy-window.h
 * @stability: Unstable
 *
 * The #EpcEntropyWindow can be used to provide some feedback when needs
 * to collect randomized data to generate for instance private keys.
 *
 * See also: #epc_tls_private_key_new
 */

/**
 * EpcEntropyWindowPrivate:
 *
 * Private fields of the #EpcEntropyWindow class.
 */
struct _EpcEntropyWindowPrivate
{
  guint timeout_id;
};

G_DEFINE_TYPE (EpcEntropyWindow, epc_entropy_window, GTK_TYPE_WINDOW);

static gboolean
epc_entropy_window_timeout_cb (gpointer data)
{
  gtk_progress_bar_pulse (data);
  return TRUE;
}

static void
epc_entropy_window_init (EpcEntropyWindow *self)
{
  GtkWidget *heading;
  GtkWidget *progress;
  GtkWidget *label;
  GtkWidget *vbox;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPC_TYPE_ENTROPY_WINDOW,
                                            EpcEntropyWindowPrivate);

  heading = gtk_label_new (NULL);

  gtk_label_set_markup (GTK_LABEL (heading), _("<b>Generating server key.</b>"));
  gtk_label_set_justify (GTK_LABEL (heading), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (heading), 0.5, 0.5);

  progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (progress), 0.02);

  label = gtk_label_new (_("Type on the keyboard, move your mouse,\n"
                           "or browse the web to generate some entropy."));

  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), heading, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), progress, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
  gtk_widget_show_all (vbox);

  gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (self), FALSE);
  gtk_container_add (GTK_CONTAINER (self), vbox);

  self->priv->timeout_id = g_timeout_add (40, epc_entropy_window_timeout_cb, progress);
}

static gboolean
epc_entropy_window_delete_event (GtkWidget   *widget G_GNUC_UNUSED,
                                 GdkEventAny *event G_GNUC_UNUSED)
{
  return TRUE;
}

static void
epc_entropy_window_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (epc_entropy_window_parent_class)->realize (widget);
  gdk_window_set_decorations (widget->window, GDK_DECOR_BORDER);
}

static gboolean
epc_entropy_window_destroy_event (GtkWidget   *widget,
                                  GdkEventAny *event)
{
  EpcEntropyWindow *self = EPC_ENTROPY_WINDOW (widget);

  if (self->priv->timeout_id)
    {
      g_source_remove (self->priv->timeout_id);
      self->priv->timeout_id = 0;
    }

  return GTK_WIDGET_CLASS (epc_entropy_window_parent_class)->destroy_event (widget, event);
}

static void
epc_entropy_window_class_init (EpcEntropyWindowClass *cls)
{
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (cls);

  wclass->delete_event = epc_entropy_window_delete_event;
  wclass->destroy_event = epc_entropy_window_destroy_event;
  wclass->realize = epc_entropy_window_realize;

  g_type_class_add_private (cls, sizeof (EpcEntropyWindowPrivate));
}

/**
 * epc_entropy_window_new:
 *
 * Creates a new #EpcEntropyWindow instance.
 *
 * Returns: The newly created #EpcEntropyWindow.
 */
GtkWidget*
epc_entropy_window_new (void)
{
  return g_object_new (EPC_TYPE_ENTROPY_WINDOW,
                       "type", GTK_WINDOW_TOPLEVEL,
                       "border-width", 12, NULL);
}

static gboolean
epc_entropy_window_idle_cb (gpointer data)
{
  gtk_widget_show (data);
  return FALSE;
}

static gpointer
epc_entropy_window_enter (void)
{
  GtkWidget *window;

  window = epc_entropy_window_new ();
  g_idle_add (epc_entropy_window_idle_cb, window);

  return window;
}

static void
epc_entropy_window_leave (gpointer data)
{
  if (data)
    gtk_widget_destroy (data);
}

/**
 * epc_entropy_window_install:
 *
 * Configures the hooks provided by libepc to use #EpcEntropyWindow when for
 * performing long standing tasks like for instance generating private keys.
 *
 * See also: #epc_tls_set_private_key_hooks
 *
 * Returns: The newly created #EpcEntropyWindow.
 */
void
epc_entropy_window_install (void)
{
  epc_tls_set_private_key_hooks (epc_entropy_window_enter,
                                 epc_entropy_window_leave);
}
