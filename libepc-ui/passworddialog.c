#include "passworddialog.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

enum
{
  PROP_NONE,
  PROP_ANONYMOUS_ALLOWED,
  PROP_ANONYMOUS,
  PROP_USERNAME,
  PROP_PASSWORD,
  PROP_REALM
};

struct _EpcPasswordDialogPrivate
{
  GtkWidget *anonymous;
  GtkWidget *username_label;
  GtkWidget *username;
  GtkWidget *password_label;
  GtkWidget *password;
  GtkWidget *heading;

  gchar *realm;
};

G_DEFINE_TYPE (EpcPasswordDialog, epc_password_dialog, GTK_TYPE_DIALOG)

static void
anonymous_toggled_cb (EpcPasswordDialog *self)
{
  gboolean anonymous = epc_password_dialog_is_anonymous (self);

  gtk_widget_set_sensitive (self->priv->username_label, !anonymous);
  gtk_widget_set_sensitive (self->priv->username,       !anonymous);
  gtk_widget_set_sensitive (self->priv->password_label, !anonymous);
  gtk_widget_set_sensitive (self->priv->password,       !anonymous);

  g_object_notify (G_OBJECT (self), "anonymous");
}

static void
username_changed_cb (EpcPasswordDialog *self)
{
  g_object_notify (G_OBJECT (self), "username");
}

static void
password_changed_cb (EpcPasswordDialog *self)
{
  g_object_notify (G_OBJECT (self), "password");
}

static void
epc_password_dialog_init (EpcPasswordDialog *self)
{
  GtkWidget *icon, *table;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EPC_TYPE_PASSWORD_DIALOG, EpcPasswordDialogPrivate);

  icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
                                   GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0.0);

  self->priv->heading = gtk_label_new (NULL);
  gtk_label_set_justify (GTK_LABEL (self->priv->heading), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (self->priv->heading), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (self->priv->heading), 40);
  gtk_misc_set_alignment (GTK_MISC (self->priv->heading), 0.0, 0.0);

  self->priv->username = gtk_entry_new ();
  self->priv->username_label = gtk_label_new_with_mnemonic (_("_Username:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (self->priv->username_label), self->priv->username);
  g_signal_connect_swapped (self->priv->username, "changed", G_CALLBACK (username_changed_cb), self);

  self->priv->password = gtk_entry_new ();
  self->priv->password_label = gtk_label_new_with_mnemonic (_("_Password:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (self->priv->password_label), self->priv->password);
  gtk_entry_set_visibility (GTK_ENTRY (self->priv->password), FALSE);
  g_signal_connect_swapped (self->priv->password, "changed", G_CALLBACK (password_changed_cb), self);

  self->priv->anonymous = gtk_check_button_new_with_label (_("Anonymous Authentication"));

  g_object_connect (self->priv->anonymous,
                    "swapped-signal::toggled", anonymous_toggled_cb, self,
                    "swapped-signal::notify::visible", anonymous_toggled_cb, self,
                    NULL);

  table = gtk_table_new (4, 3, FALSE);

  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 12);
  gtk_table_set_col_spacing (GTK_TABLE (table), 1, 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);

  gtk_table_attach (GTK_TABLE (table), icon, 0, 1, 0, 4, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->priv->heading, 1, 3, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->priv->username_label, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->priv->username, 2, 3, 1, 2, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->priv->password_label, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->priv->password, 2, 3, 2, 3, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table), self->priv->anonymous, 2, 3, 3, 4, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);

  gtk_widget_show_all (table);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (self)->vbox), table, TRUE, TRUE, 0);
}

static void
epc_password_dialog_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EpcPasswordDialog *self = EPC_PASSWORD_DIALOG (object);

  switch (prop_id)
    {
      case PROP_ANONYMOUS_ALLOWED:
        g_value_set_boolean (value, epc_password_dialog_get_anonymous_allowed (self));
        break;

      case PROP_ANONYMOUS:
        g_value_set_boolean (value, epc_password_dialog_is_anonymous (self));
        break;

      case PROP_USERNAME:
        g_value_set_string (value, epc_password_dialog_get_username (self));
        break;

      case PROP_PASSWORD:
        g_value_set_string (value, epc_password_dialog_get_password (self));
        break;

      case PROP_REALM:
        g_value_set_string (value, self->priv->realm);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_password_dialog_real_set_realm (EpcPasswordDialog *self,
			            const gchar       *realm)
{
  gchar *markup;

  if (realm)
    markup = g_markup_printf_escaped (_("Authentication required for <b>%s</b>."), realm);
  else
    markup = _("Authentication required.");

  gtk_label_set_markup (GTK_LABEL (self->priv->heading), markup);

  if (realm)
    g_free (markup);
}

static void
epc_password_dialog_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EpcPasswordDialog *self = EPC_PASSWORD_DIALOG (object);
  const gchar *text;

  switch (prop_id)
    {
      case PROP_ANONYMOUS_ALLOWED:
        g_object_set (self->priv->anonymous, "visible",
                      g_value_get_boolean (value), NULL);
        break;

      case PROP_ANONYMOUS:
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->anonymous),
                                      g_value_get_boolean (value));
        break;

      case PROP_USERNAME:
        text = g_value_get_string (value);
        gtk_entry_set_text (GTK_ENTRY (self->priv->username), text ? text : "");
        break;

      case PROP_PASSWORD:
        text = g_value_get_string (value);
        gtk_entry_set_text (GTK_ENTRY (self->priv->password), text ? text : "");
        break;

      case PROP_REALM:
        epc_password_dialog_real_set_realm (self, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
epc_password_dialog_dispose (GObject *object)
{
  EpcPasswordDialog *self = EPC_PASSWORD_DIALOG (object);

  g_free (self->priv->realm);
  self->priv->realm = NULL;

  G_OBJECT_CLASS (epc_password_dialog_parent_class)->dispose (object);
}

static void
epc_password_dialog_class_init (EpcPasswordDialogClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);

  object_class->get_property = epc_password_dialog_get_property;
  object_class->set_property = epc_password_dialog_set_property;
  object_class->dispose = epc_password_dialog_dispose;

  g_object_class_install_property (object_class, PROP_ANONYMOUS_ALLOWED,
                                   g_param_spec_boolean ("anonymous-allowed", "Allow Anonymous Authentication",
                                                         "Show widget to allow anonymous authentication", TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_ANONYMOUS,
                                   g_param_spec_boolean ("anonymous", "Anonymous Authentication",
                                                         "Try to use anonymous authentication", FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                         G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_USERNAME,
                                   g_param_spec_string ("username", "Username",
                                                        "The username to use for authentication",
                                                        g_get_user_name (),
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_PASSWORD,
                                   g_param_spec_string ("password", "Password",
                                                        "The password to use for authentication", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_REALM,
                                   g_param_spec_string ("realm", "Authentication Realm",
                                                        "The authentication realm the dialog is used for", NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

  g_type_class_add_private (cls, sizeof (EpcPasswordDialogClass));
}

GtkWidget*
epc_password_dialog_new (const gchar    *title,
                         GtkWindow      *parent,
                         const gchar    *realm,
                         const gchar    *first_button_text,
                         ...)
{
  GtkDialog *dialog;
  va_list args;

  gint default_response = GTK_RESPONSE_NONE;
  const gchar *button_text;
  gint response_id;

  dialog = g_object_new (EPC_TYPE_PASSWORD_DIALOG,
                         "has-separator", FALSE, "title", title,
                         "realm", realm, NULL);

  if (parent)
    g_object_set (dialog, "parent", parent, NULL);

  va_start (args, first_button_text);

  for (button_text = first_button_text; button_text; button_text = va_arg (args, const gchar*))
    {
      response_id = va_arg (args, gint);

      gtk_dialog_add_button (dialog, button_text, response_id);

      if (GTK_RESPONSE_NONE != default_response)
        continue;

      if (GTK_RESPONSE_ACCEPT == response_id ||
          GTK_RESPONSE_OK     == response_id ||
          GTK_RESPONSE_YES    == response_id ||
          GTK_RESPONSE_APPLY  == response_id)
        default_response = response_id;
    }

  va_end (args);

  if (GTK_RESPONSE_NONE != default_response)
    gtk_dialog_set_default_response (dialog, default_response);

  return GTK_WIDGET (dialog);
}

void
epc_password_dialog_set_anonymous_allowed (EpcPasswordDialog *self,
					   gboolean           allowed)
{
  g_return_if_fail (EPC_IS_PASSWORD_DIALOG (self));
  g_object_set (self, "anonymous-allowed", allowed, NULL);
}

gboolean
epc_password_dialog_get_anonymous_allowed (EpcPasswordDialog *self)
{
  g_return_val_if_fail (EPC_IS_PASSWORD_DIALOG (self), FALSE);
  return GTK_WIDGET_VISIBLE (self->priv->anonymous);
}

void
epc_password_dialog_set_anonymous (EpcPasswordDialog *self,
                                   gboolean           anonymous)
{
  g_return_if_fail (EPC_IS_PASSWORD_DIALOG (self));
  g_object_set (self, "anonymous", anonymous, NULL);
}

gboolean
epc_password_dialog_is_anonymous (EpcPasswordDialog *self)
{
  return
    epc_password_dialog_get_anonymous_allowed (self) &&
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->anonymous));
}

void
epc_password_dialog_set_username (EpcPasswordDialog *self,
				  const gchar       *username)
{
  g_return_if_fail (EPC_IS_PASSWORD_DIALOG (self));
  g_object_set (self, "username", username, NULL);
}

G_CONST_RETURN gchar*
epc_password_dialog_get_username (EpcPasswordDialog *self)
{
  g_return_val_if_fail (EPC_IS_PASSWORD_DIALOG (self), NULL);

  if (epc_password_dialog_is_anonymous (self))
    return NULL;

  return gtk_entry_get_text (GTK_ENTRY (self->priv->username));
}

void
epc_password_dialog_set_password (EpcPasswordDialog *self,
				  const gchar       *password)
{
  g_return_if_fail (EPC_IS_PASSWORD_DIALOG (self));
  g_object_set (self, "password", password, NULL);
}

G_CONST_RETURN gchar*
epc_password_dialog_get_password (EpcPasswordDialog *self)
{
  g_return_val_if_fail (EPC_IS_PASSWORD_DIALOG (self), NULL);

  if (epc_password_dialog_is_anonymous (self))
    return NULL;

  return gtk_entry_get_text (GTK_ENTRY (self->priv->password));
}

void
epc_password_dialog_set_realm (EpcPasswordDialog *self,
			       const gchar       *realm)
{
  g_return_if_fail (EPC_IS_PASSWORD_DIALOG (self));
  g_object_set (self, "realm", realm, NULL);
}

G_CONST_RETURN gchar*
epc_password_dialog_get_realm (EpcPasswordDialog *self)
{
  g_return_val_if_fail (EPC_IS_PASSWORD_DIALOG (self), NULL);
  return self->priv->realm;
}
