#ifndef __EPC_PASSWORD_DIALOG_H__
#define __EPC_PASSWORD_DIALOG_H__

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define EPC_TYPE_PASSWORD_DIALOG           (epc_password_dialog_get_type())
#define EPC_PASSWORD_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_CAST(obj, EPC_TYPE_PASSWORD_DIALOG, EpcPasswordDialog))
#define EPC_PASSWORD_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST(klass, EPC_TYPE_PASSWORD_DIALOG, EpcPasswordDialogClass))
#define EPC_IS_PASSWORD_DIALOG(obj)        (G_TYPE_CHECK_INSTANCE_TYPE(obj, EPC_TYPE_PASSWORD_DIALOG))
#define EPC_IS_PASSWORD_DIALOG_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE(obj, EPC_TYPE_PASSWORD_DIALOG))
#define EPC_PASSWORD_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPC_TYPE_PASSWORD_DIALOG, EpcPasswordDialogClass))

typedef struct _EpcPasswordDialog        EpcPasswordDialog;
typedef struct _EpcPasswordDialogClass   EpcPasswordDialogClass;
typedef struct _EpcPasswordDialogPrivate EpcPasswordDialogPrivate;

struct _EpcPasswordDialog
{
  GtkDialog parent_instance;
  EpcPasswordDialogPrivate *priv;
};

struct _EpcPasswordDialogClass
{
  GtkDialogClass parent_class;
};

GType                 epc_password_dialog_get_type              (void) G_GNUC_CONST;
GtkWidget*            epc_password_dialog_new                   (const gchar       *title,
                                                                 GtkWindow         *parent,
                                                                 const gchar       *realm,
                                                                 const gchar       *first_button_text,
                                                                 ...) G_GNUC_NULL_TERMINATED;

void                  epc_password_dialog_set_anonymous_allowed (EpcPasswordDialog *dialog,
								 gboolean           allowed);
gboolean              epc_password_dialog_get_anonymous_allowed (EpcPasswordDialog *dialog);

void                  epc_password_dialog_set_anonymous         (EpcPasswordDialog *dialog,
                                                                 gboolean           anonymous);
gboolean              epc_password_dialog_is_anonymous          (EpcPasswordDialog *dialog);

void                  epc_password_dialog_set_username          (EpcPasswordDialog *dialog,
								 const gchar       *username);
G_CONST_RETURN gchar* epc_password_dialog_get_username          (EpcPasswordDialog *dialog);

void                  epc_password_dialog_set_password          (EpcPasswordDialog *dialog,
								 const gchar       *password);
G_CONST_RETURN gchar* epc_password_dialog_get_password          (EpcPasswordDialog *dialog);

void                  epc_password_dialog_set_realm             (EpcPasswordDialog *dialog,
								 const gchar       *realm);
G_CONST_RETURN gchar* epc_password_dialog_get_realm             (EpcPasswordDialog *dialog);

G_END_DECLS

#endif /* __EPC_PASSWORD_DIALOG_H__ */
