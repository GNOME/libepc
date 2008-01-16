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
#include "progress-window.h"

#include <libepc/shell.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <string.h>

/**
 * SECTION:progress-window
 * @short_description: provide feedback during lengthly operations
 * @include: libepc-ui/progress-window.h
 * @stability: Unstable
 *
 * The #EpcProgressWindow can be used to provide some feedback during
 * lengthly operations, like generating server keys.
 *
 * See also: epc_tls_private_key_new()
 */

enum
{
  PROP_NONE,
  PROP_MESSAGE,
  PROP_TITLE,
  PROP_PROGRESS
};

/**
 * EpcProgressWindowPrivate:
 *
 * Private fields of the #EpcProgressWindow class.
 */
struct _EpcProgressWindowPrivate
{
  GtkWidget *message;
  GtkWidget *title;
  GtkWidget *progress;
  guint timeout_id;
};

G_DEFINE_TYPE (EpcProgressWindow, epc_progress_window, GTK_TYPE_WINDOW);

static gboolean
epc_progress_window_timeout_cb (gpointer data)
{
  EpcProgressWindow *self = EPC_PROGRESS_WINDOW (data);

  gtk_progress_bar_pulse (GTK_PROGRESS_BAR (self->priv->progress));

  return TRUE;
}

static void
epc_progress_window_init (EpcProgressWindow *self)
{
  GtkWidget *vbox;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            EPC_TYPE_PROGRESS_WINDOW,
                                            EpcProgressWindowPrivate);


  self->priv->title = gtk_label_new (NULL);
  gtk_label_set_justify (GTK_LABEL (self->priv->title), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (self->priv->title), 0.5, 0.5);

  self->priv->progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (self->priv->progress), 0.02);

  self->priv->message = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (self->priv->message), TRUE);
  gtk_label_set_justify (GTK_LABEL (self->priv->message), GTK_JUSTIFY_CENTER);
  gtk_misc_set_alignment (GTK_MISC (self->priv->message), 0.5, 0.5);

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), self->priv->title, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), self->priv->progress, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), self->priv->message, FALSE, TRUE, 0);
  gtk_widget_show_all (vbox);

  gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (self), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (self), FALSE);
  gtk_container_add (GTK_CONTAINER (self), vbox);
}

static gboolean
epc_progress_window_delete_event (GtkWidget   *widget G_GNUC_UNUSED,
                                  GdkEventAny *event G_GNUC_UNUSED)
{
  return TRUE;
}

static void
epc_progress_window_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (epc_progress_window_parent_class)->realize (widget);
  gdk_window_set_decorations (widget->window, GDK_DECOR_BORDER);
}

static void
epc_progress_window_stop_pulsing (EpcProgressWindow *self)
{
  if (self->priv->timeout_id)
    {
      g_source_remove (self->priv->timeout_id);
      self->priv->timeout_id = 0;
    }
}

static void
epc_progress_window_dispose (GObject *object)
{
  EpcProgressWindow *self = EPC_PROGRESS_WINDOW (object);

  epc_progress_window_stop_pulsing (self);

  return G_OBJECT_CLASS (epc_progress_window_parent_class)->dispose (object);
}

static void
epc_progress_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EpcProgressWindow *self = EPC_PROGRESS_WINDOW (object);
  const gchar *text = NULL;
  gchar *markup = NULL;
  gdouble progress;

  switch (prop_id)
    {
      case PROP_MESSAGE:
        gtk_label_set_text (GTK_LABEL (self->priv->message),
                            g_value_get_string (value));
        break;

      case PROP_TITLE:
        text = g_value_get_string (value);
        gtk_window_set_title (GTK_WINDOW (self), text);
        markup = g_markup_printf_escaped ("<b>%s</b>", text);
        gtk_label_set_markup (GTK_LABEL (self->priv->title), markup);
        g_free (markup);
        break;

      case PROP_PROGRESS:
        progress = g_value_get_double (value);

        if (progress >= 0 && progress <= 1)
          {
            epc_progress_window_stop_pulsing (self);
            gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->priv->progress), progress);
          }
        else if (0 == self->priv->timeout_id)
          self->priv->timeout_id = g_timeout_add (40, epc_progress_window_timeout_cb, self);

        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }

}

static void
epc_progress_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EpcProgressWindow *self = EPC_PROGRESS_WINDOW (object);

  switch (prop_id)
    {
      case PROP_MESSAGE:
        g_object_get_property (G_OBJECT (self->priv->message), "text", value);
        break;

      case PROP_PROGRESS:
        if (self->priv->timeout_id)
          g_value_set_double (value, -1);
        else
          g_value_set_double (value,
            gtk_progress_bar_get_fraction (
            GTK_PROGRESS_BAR (self->priv->progress)));

        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_progress_window_class_init (EpcProgressWindowClass *cls)
{
  GObjectClass *oclass = G_OBJECT_CLASS (cls);
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (cls);

  oclass->set_property = epc_progress_window_set_property;
  oclass->get_property = epc_progress_window_get_property;
  oclass->dispose = epc_progress_window_dispose;

  wclass->delete_event = epc_progress_window_delete_event;
  wclass->realize = epc_progress_window_realize;

  g_object_class_install_property (oclass, PROP_MESSAGE,
                                   g_param_spec_string ("message", "Message",
                                                        "The progress message to show", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_TITLE,
                                   g_param_spec_string ("title", "Title",
                                                        "The title to show", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (oclass, PROP_PROGRESS,
                                   g_param_spec_double ("progress", "Progress",
                                                        "Current progress, or -1", -1, 1, -1,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcProgressWindowPrivate));
}

/**
 * epc_progress_window_new:
 * @title: the title of the window
 * @parent: the transient parent of the window, or %NULL
 * @message: the message to show, or %NULL
 *
 * Creates a new #EpcProgressWindow instance.
 *
 * Returns: The newly created #EpcProgressWindow.
 */
GtkWidget*
epc_progress_window_new (const gchar *title,
                         GtkWindow   *parent,
                         const gchar *message)
{
  GtkWidget *self;

  g_return_val_if_fail (NULL != title, NULL);

  self = g_object_new (EPC_TYPE_PROGRESS_WINDOW,
                       "type", GTK_WINDOW_TOPLEVEL,
                       "border-width", 12,
                       "message", message,
                       "title", title,
                       NULL);

  if (GTK_IS_WINDOW (parent))
    gtk_window_set_transient_for (GTK_WINDOW (self), parent);

  return self;
}

/**
 * epc_progress_window_update:
 * @window: a #EpcProgressWindow
 * @progress: the current progress, or -1
 * @message: a description of the current progress
 *
 * Updates the #EpcProgressWindow with new progress information. The @progress
 * is expressed as percentage in the range [0..1], or -1 if the progress cannot
 * be estimated.
 *
 * See also: #EpcProgressWindow:message, #EpcProgressWindow:progress
 */
void
epc_progress_window_update (EpcProgressWindow *self,
                            gdouble            progress,
                            const gchar       *message)
{
  g_return_if_fail (EPC_IS_PROGRESS_WINDOW (self));
  g_object_set (self, "message", message, "progress", progress, NULL);
}

typedef struct _EpcProgressWindowContext EpcProgressWindowContext;

struct _EpcProgressWindowContext
{
  GtkWindow *parent;
  GtkWidget *window;
};

static void
epc_progress_window_begin_cb (const gchar *title,
                              gpointer     data)
{
  EpcProgressWindowContext *context = data;

  g_return_if_fail (NULL != context);

  context->window = epc_progress_window_new (title, context->parent, NULL);
}

static void
epc_progress_window_update_cb (gdouble      progress,
                               const gchar *message,
                               gpointer     data)
{
  EpcProgressWindowContext *context = data;

  g_return_if_fail (NULL != context);
  g_return_if_fail (EPC_IS_PROGRESS_WINDOW (context->window));

  if (progress < 0 || progress > 1)
    progress = -1;

  epc_progress_window_update (EPC_PROGRESS_WINDOW (context->window),
                              progress, message);

  gtk_widget_show (context->window);
}

static void
epc_progress_window_end_cb (gpointer data)
{
  EpcProgressWindowContext *context = data;

  g_return_if_fail (NULL != context);
  g_return_if_fail (EPC_IS_PROGRESS_WINDOW (context->window));

  gtk_widget_destroy (context->window);
  context->window = NULL;
}

static EpcProgressWindowContext*
epc_progress_window_context_new (GtkWindow *parent)
{
  EpcProgressWindowContext *self = g_new0 (EpcProgressWindowContext, 1);

  if (parent)
    self->parent = g_object_ref (parent);

  return self;
}

static void
epc_progress_window_context_free (gpointer data)
{
  EpcProgressWindowContext *self = data;

  if (self->window)
    {
      gtk_widget_destroy (self->window);
      self->window = NULL;
    }

  if (self->parent)
    {
      g_object_unref (self->parent);
      self->parent = NULL;
    }

  g_free (self);
}

/**
 * epc_progress_window_install:
 * @parent: the transient parent of the window, or %NULL
 *
 * Configures the hooks provided by libepc to use #EpcProgressWindow when for
 * performing long standing tasks like for instance generating private keys.
 *
 * See also: epc_tls_set_private_key_hooks()
 */
void
epc_progress_window_install (GtkWindow *parent)
{
  static EpcShellProgressHooks hooks = {
    begin:  epc_progress_window_begin_cb,
    update: epc_progress_window_update_cb,
    end:    epc_progress_window_end_cb
  };

  epc_shell_set_progress_hooks (&hooks,
                                epc_progress_window_context_new (parent),
                                epc_progress_window_context_free);
}
