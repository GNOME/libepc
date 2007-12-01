#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libepc/publisher.h>
#include <libepc-ui/progress-window.h>

#define GET_WIDGET(builder, name)       (GTK_WIDGET (gtk_builder_get_object ((builder), (name))))
#define EPC_TYPE_DEMO_CONTENTS          (epc_demo_item_get_type ())

typedef struct _EpcDemoItem EpcDemoItem;

typedef void (*UpdateItemFunc)  (EpcDemoItem *item,
                                 const gchar *text);
typedef void (*ForeachItemFunc) (EpcDemoItem *item,
                                 gpointer     data);

struct _EpcDemoItem
{
  volatile gint ref_count;

  gchar *name;
  gchar *text;
  gchar *username;
  gchar *password;
  gchar *bookmark;
};

static EpcPublisher *publisher = NULL;
static gboolean updating_selection = FALSE;
static GtkTextBuffer *text_buffer = NULL;
static GtkListStore *item_list = NULL;
static guint auto_save_id = 0;

static GtkBuilder *builder = NULL;
static GtkWidget *item_view = NULL;

static EpcDemoItem*
epc_demo_item_new (const gchar *name,
                   const gchar *text)
{
  EpcDemoItem *self = g_slice_new0 (EpcDemoItem);

  self->ref_count = 1;

  if (name)
    self->name = g_strdup (name);
  if (text)
    self->text = g_strdup (text);

  return self;
}

static gpointer
epc_demo_item_ref (gpointer boxed)
{
  EpcDemoItem *self = boxed;

  g_atomic_int_inc (&self->ref_count);

  return self;
}

static void
epc_demo_item_unref (gpointer boxed)
{
  EpcDemoItem *self = boxed;

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_free (self->name);
      g_free (self->text);
      g_free (self->username);
      g_free (self->password);
      g_free (self->bookmark);

      g_slice_free (EpcDemoItem, self);
    }
}

static EpcDemoItem*
epc_demo_item_load (GKeyFile    *settings,
                    const gchar *group)
{
  EpcDemoItem *self = NULL;

  if (g_key_file_has_group (settings, group))
    {
      self = epc_demo_item_new (NULL, NULL);

      self->name = g_key_file_get_string (settings, group, "Name", NULL);
      self->text = g_key_file_get_string (settings, group, "Text", NULL);

      self->username = g_key_file_get_string (settings, group, "Username", NULL);
      self->password = g_key_file_get_string (settings, group, "Password", NULL);
      self->bookmark = g_key_file_get_string (settings, group, "Bookmark", NULL);
    }

  return self;
}

static void
epc_demo_item_save (EpcDemoItem *self,
                    GKeyFile    *settings)
{
  gchar *group = g_strdup_printf ("Item: %s", self->name);

  g_key_file_set_string (settings, group, "Name", self->name);
  g_key_file_set_string (settings, group, "Text", self->text ? self->text : "");

  if (self->username)
    g_key_file_set_string (settings, group, "Username", self->username);
  if (self->password)
    g_key_file_set_string (settings, group, "Password", self->password);
  if (self->bookmark)
    g_key_file_set_string (settings, group, "Bookmark", self->bookmark);

  g_free (group);
}

static void
epc_demo_item_set_text (EpcDemoItem *self,
                        const gchar *text)
{
  g_return_if_fail (self);

  g_free (self->text);
  self->text = (text && *text ? g_strdup (text) : NULL);
}

static void
epc_demo_item_set_username (EpcDemoItem *self,
                            const gchar *text)
{
  g_return_if_fail (self);

  g_free (self->username);
  self->username = (text && *text ? g_strdup (text) : NULL);
}

static void
epc_demo_item_set_password (EpcDemoItem *self,
                            const gchar *text)
{
  g_return_if_fail (self);

  g_free (self->password);
  self->password = (text && *text ? g_strdup (text) : NULL);
}

static void
epc_demo_item_set_bookmark (EpcDemoItem *self,
                            const gchar *text)
{
  g_return_if_fail (self);

  g_free (self->bookmark);
  self->bookmark = (text && *text ? g_strdup (text) : NULL);
}

static GType
epc_demo_item_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (!type))
    type = g_boxed_type_register_static ("EpcDemoItem",
                                         epc_demo_item_ref,
                                         epc_demo_item_unref);

  return type;
}

static void
show_error (GtkWidget   *parent,
            const gchar *title,
            GError      *error)
{
  GtkWidget *dialog;

  if (parent)
    parent = gtk_widget_get_toplevel (parent);

  dialog = gtk_message_dialog_new_with_markup (parent ? GTK_WINDOW (parent) : NULL,
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "<big><b>%s</b></big>\n\n%s.",
                                               title, error->message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
load_ui (const gchar *path)
{
  gchar *dirname = g_path_get_dirname (path);

  if (!g_str_has_suffix (dirname, ".libs"))
    {
      GError *error = NULL;
      gchar *filename;

      filename = g_build_filename (dirname, "publisher.ui", NULL);
      gtk_builder_add_from_file (builder, filename, &error);

      if (error)
        {
          show_error (NULL, "Cannot load user interface.", error);
          g_error_free (error);
        }

      g_free (filename);
    }
  else
    load_ui (dirname);

  g_free (dirname);
}

static gchar*
get_settings_filename (void)
{
  return g_build_filename (g_get_user_config_dir (), "libepc",
                           g_get_prgname (), "settings", NULL);
}

static void
load_settings (void)
{
  gchar *filename = get_settings_filename ();
  GKeyFile *settings = g_key_file_new ();
  gchar **groups = NULL;
  gchar *text;
  gint i;

  g_key_file_load_from_file (settings, filename, 0, NULL);
  groups = g_key_file_get_groups (settings, NULL);

  if (!item_list)
    item_list = gtk_list_store_new (2, G_TYPE_STRING, EPC_TYPE_DEMO_CONTENTS);
  else
    gtk_list_store_clear (item_list);

  gtk_toggle_tool_button_set_active (
    GTK_TOGGLE_TOOL_BUTTON (GET_WIDGET (builder, "encryption-state")),
    g_key_file_get_boolean (settings, "Settings", "Encrypted", NULL));

  text = g_key_file_get_string (settings, "Settings", "ServiceName", NULL);

  if (text)
    gtk_entry_set_text (GTK_ENTRY (GET_WIDGET (builder, "service-name")), text);

  g_free (text);

  for (i = 0; groups[i]; ++i)
    if (g_str_has_prefix (groups[i], "Item: "))
      {
        EpcDemoItem *item = epc_demo_item_load (settings, groups[i]);
        GtkTreeIter iter;

        if (!item)
          continue;

        gtk_list_store_append (item_list, &iter);
        gtk_list_store_set (item_list, &iter, 0, item->name, 1, item, -1);

        epc_demo_item_unref (item);
      }

  g_key_file_free (settings);
  g_strfreev (groups);
  g_free (filename);
}

static void
foreach_item (ForeachItemFunc callback,
              gpointer        user_data)
{
  GtkTreeModel *model = GTK_TREE_MODEL (item_list);
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    do
      {
        EpcDemoItem *item = NULL;
        gtk_tree_model_get (model, &iter, 1, &item, -1);

        if (item)
          {
            callback (item, user_data);
            epc_demo_item_unref (item);
          }
      }
    while (gtk_tree_model_iter_next (model, &iter));
}

static gboolean
auto_save_cb (gpointer data G_GNUC_UNUSED)
{
  gchar *filename = get_settings_filename ();
  GKeyFile *settings = g_key_file_new ();
  gchar **groups = NULL;
  GtkWidget *widget;
  gint i;

  gchar *settings_data;
  gsize settings_len;

  g_key_file_load_from_file (settings, filename, 0, NULL);
  groups = g_key_file_get_groups (settings, NULL);

  widget = GET_WIDGET (builder, "encryption-state");
  g_key_file_set_boolean (settings, "Settings", "Encrypted",
                          gtk_toggle_tool_button_get_active (
                          GTK_TOGGLE_TOOL_BUTTON (widget)));

  widget = GET_WIDGET (builder, "service-name");
  g_key_file_set_string (settings, "Settings", "ServiceName",
                         gtk_entry_get_text (GTK_ENTRY (widget)));

  for (i = 0; groups[i]; ++i)
    if (g_str_has_prefix (groups[i], "Item: "))
      g_key_file_remove_group (settings, groups[i], NULL);

  foreach_item ((ForeachItemFunc) epc_demo_item_save, settings);
  settings_data = g_key_file_to_data (settings, &settings_len, NULL);

  if (settings_data)
    {
      g_mkdir_with_parents (filename, 0700);
      g_rmdir (filename);

      g_debug ("%s: filename=%s", G_STRFUNC, filename);
      g_file_set_contents (filename, settings_data, settings_len, NULL);
      g_free (settings_data);
    }

  g_key_file_free (settings);
  g_strfreev (groups);
  g_free (filename);

  auto_save_id = 0;

  return FALSE;
}

static void
schedule_auto_save (void)
{
  if (0 == auto_save_id)
    auto_save_id = g_timeout_add (5 * 1000, auto_save_cb, NULL);
}

static void
url_hook (GtkAboutDialog *dialog,
          const gchar    *link,
          gpointer        data G_GNUC_UNUSED)
{
  gchar *args[] = { "xdg-open", (gchar*) link, NULL };
  GError *error = NULL;

  gdk_spawn_on_screen (gtk_widget_get_screen (GTK_WIDGET (dialog)),
                       NULL, args, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, NULL, &error);

  if (error)
    {
      show_error (GTK_WIDGET (dialog), "Cannot follow link.", error);
      g_error_free (error);
    }
}

void
about_button_clicked_cb (GtkWidget *widget)
{
  GtkWidget *dialog = GET_WIDGET (builder, "about-dialog");
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (widget)));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);
}

void
add_item_clicked_cb (void)
{
  GtkTreeSelection *selection;
  GtkTreeIter list_iter, iter;
  GtkTreeViewColumn *column;
  GtkTreeModel *model;
  GtkTreePath *path;

  gtk_widget_grab_focus (item_view);
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (item_view), 0);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (item_view));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (item_view), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

  gtk_list_store_append (item_list, &list_iter);
  gtk_list_store_set (item_list, &list_iter, 0, "New Item", -1);

  gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (model),
                                                  &iter, &list_iter);

  path = gtk_tree_model_get_path (model, &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (item_view), path, column, TRUE);
  gtk_tree_path_free (path);
}

static void
remove_item (GtkTreeSelection *selection,
             GtkTreeModel     *model,
             GtkTreeIter      *iter)
{
  EpcDemoItem *item = NULL;
  GtkTreePath *path = NULL;
  GtkTreeIter  child_iter;

  gtk_tree_model_get (model, iter, 1, &item, -1);

  if (item)
    {
      epc_publisher_remove (publisher, item->name);
      epc_demo_item_unref (item);
    }

  path = gtk_tree_model_get_path (model, iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (item_view), path, NULL, FALSE);

  if (GTK_IS_TREE_MODEL_SORT (model))
    gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
                                                    &child_iter, iter);
  else
    child_iter = *iter;

  gtk_list_store_remove (item_list, &child_iter);

  if (gtk_tree_model_get_iter (model, iter, path) ||
     (gtk_tree_path_prev (path) && gtk_tree_model_get_iter (model, iter, path)))
    gtk_tree_selection_select_iter (selection, iter);

  gtk_tree_path_free (path);
}

void
remove_item_clicked_cb (void)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (item_view));
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    remove_item (selection, model, &iter);
}

static EpcDemoItem*
get_selected_item (GtkTreeSelection *selection)
{
  GtkTreeModel *model = NULL;
  EpcDemoItem *item = NULL;
  GtkTreeIter iter;

  if (updating_selection)
    return NULL;

  if (!selection)
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (item_view));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, 1, &item, -1);

  return item;
}

static void
selection_changed_cb (GtkTreeSelection *selection)
{
  EpcDemoItem *item;

  g_return_if_fail (!updating_selection);
  item = get_selected_item (selection);
  updating_selection = TRUE;

  if (item)
    {
      gtk_text_buffer_set_text (text_buffer,
                                item->text ? item->text : "",
                                -1);

      gtk_entry_set_text (GTK_ENTRY (GET_WIDGET (builder, "username")),
                          item->username ? item->username : "");
      gtk_entry_set_text (GTK_ENTRY (GET_WIDGET (builder, "password")),
                          item->password ? item->password : "");
      gtk_entry_set_text (GTK_ENTRY (GET_WIDGET (builder, "bookmark")),
                          item->bookmark ? item->bookmark: "");

      epc_demo_item_unref (item);
    }

  gtk_widget_set_sensitive (GET_WIDGET (builder, "item-vbox"), NULL != item);
  gtk_widget_set_sensitive (GET_WIDGET (builder, "remove-item"), NULL != item);

  updating_selection = FALSE;
}

static void
update_item (UpdateItemFunc update,
             const gchar   *text)
{
  EpcDemoItem *item = get_selected_item (NULL);

  if (item)
    {
      update (item, text);
      epc_demo_item_unref (item);
      schedule_auto_save ();
    }
}

void
username_changed_cb (GtkEntry *entry)
{
  update_item (epc_demo_item_set_username, gtk_entry_get_text (entry));
}

void
password_changed_cb (GtkEntry *entry)
{
  update_item (epc_demo_item_set_password, gtk_entry_get_text (entry));
}

void
bookmark_changed_cb (GtkEntry *entry)
{
  update_item (epc_demo_item_set_bookmark, gtk_entry_get_text (entry));
}

static void
text_buffer_changed_cb (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;
  gchar *text;

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
  update_item (epc_demo_item_set_text, text);
  g_free (text);
}

static gchar*
get_service_name (GtkEntry *entry)
{
  const gchar *pattern = gtk_entry_get_text (entry);
  GError *error = NULL;
  gchar *name;

  name = epc_publisher_expand_name (pattern, &error);

  if (!name)
    show_error (GTK_WIDGET (entry), "Cannot change service name", error);

  g_clear_error (&error);

  return name;
}

static EpcProtocol
get_protocol (GtkToggleToolButton *button)
{
  gboolean encryption = gtk_toggle_tool_button_get_active (button);
  return (encryption ? EPC_PROTOCOL_HTTPS : EPC_PROTOCOL_HTTP);
}

void
service_name_changed_cb (GtkEntry *entry)
{
  if (publisher)
    {
      gchar *name = get_service_name (entry);

      if (name)
        epc_publisher_set_service_name (publisher, name);

      g_free (name);
    }

  schedule_auto_save ();
}

static EpcContents*
item_contents_handler (EpcPublisher *publisher G_GNUC_UNUSED,
                       const gchar  *key G_GNUC_UNUSED,
                       gpointer      data)
{
  EpcDemoItem *item = data;
  const gchar *type = "text/plain";
  const gchar *text = item->text ? item->text : "";

  if (g_str_has_prefix (text, "<html>"))
    type = "text/html";

  return epc_contents_new_dup (type, text, -1);
}

static gboolean
auth_handler (EpcAuthContext *context,
              const gchar    *username,
              gpointer        data G_GNUC_UNUSED)
{
  EpcPublisher *publisher = epc_auth_context_get_publisher (context);
  const gchar *key = epc_auth_context_get_key (context);
  EpcDemoItem *item = epc_publisher_lookup (publisher, key);

  g_return_val_if_fail (NULL != item, FALSE);

  if (!item->username || !*item->username)
    return TRUE;

  return
    NULL != username &&
    g_str_equal (username, item->username) &&
    epc_auth_context_check_password (context, item->password);
}

static void
publish_item_cb (EpcDemoItem *item,
                 gpointer     data G_GNUC_UNUSED)
{
  epc_publisher_add_handler (publisher, item->name, item_contents_handler,
                             epc_demo_item_ref (item),
                             epc_demo_item_unref);
  epc_publisher_set_auth_handler (publisher, item->name,
                                  auth_handler, NULL, NULL);
}

void
publish_state_toggled_cb (GtkToggleToolButton *button)
{
  gboolean publish = gtk_toggle_tool_button_get_active (button);
  GError *error = NULL;

  if (!publish)
    epc_publisher_quit (publisher);
  else if (!epc_publisher_run_async (publisher, &error))
    show_error (GTK_WIDGET (button), "Cannot start publisher", error);

  g_clear_error (&error);
}

void
encryption_state_toggled_cb (GtkToggleToolButton *button)
{
  if (publisher)
    {
      gboolean was_running;
      GError *error = NULL;

      was_running = epc_publisher_quit (publisher);
      epc_publisher_set_protocol (publisher, get_protocol (button));

      if (was_running && !epc_publisher_run_async (publisher, &error))
        show_error (GTK_WIDGET (button), "Cannot restart publisher", error);

      g_clear_error (&error);
    }

  schedule_auto_save ();
}

static void
cell_editing_canceled_cb (void)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (item_view));
  GtkTreeModel *model = NULL;
  EpcDemoItem *item = NULL;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, 1, &item, -1);

  if (!item)
    remove_item (selection, model, &iter);
  else
    epc_demo_item_unref (item);
}

static void
cell_item_edited_cb (GtkCellRendererText *cell G_GNUC_UNUSED,
                     const gchar         *path,
                     const gchar         *text)
{
  GtkTreeIter iter, child_iter;
  GtkTreeModel *model;
  EpcDemoItem *item;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (item_view));

  if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
      const gchar *base_name = text;
      gchar *unique_name = NULL;
      gint unique_index = 1;

      gtk_tree_model_get (model, &iter, 1, &item, -1);

      if (item && item->name)
        epc_publisher_remove (publisher, item->name);

      while (epc_publisher_has_key (publisher, text))
        {
          g_free (unique_name);

          unique_name = g_strdup_printf ("%s #%d", base_name, unique_index);
          unique_index += 1;

          text = unique_name;
        }

      if (item)
        {
          g_free (item->name);
          item->name = g_strdup (text);
        }
      else
        item = epc_demo_item_new (text, NULL);

      publish_item_cb (item, NULL);

      gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
                                                      &child_iter, &iter);
      gtk_list_store_set (item_list, &child_iter, 0, text, 1, item, -1);

      selection_changed_cb (NULL);
      epc_demo_item_unref (item);
      schedule_auto_save ();
    }
}

void
main_window_destroy_cb (void)
{
  if (auto_save_id)
    {
      g_source_remove (auto_save_id);
      auto_save_cb (NULL);
    }

  gtk_main_quit ();
}

static void
create_publisher (void)
{
  GtkToggleToolButton *encryption = NULL;
  GtkEntry *service_name_entry = NULL;
  gchar *service_name = NULL;

  encryption = GTK_TOGGLE_TOOL_BUTTON (GET_WIDGET (builder, "encryption-state"));
  service_name_entry = GTK_ENTRY (GET_WIDGET (builder, "service-name"));
  service_name = get_service_name (service_name_entry);

  g_assert (NULL == publisher);
  publisher = epc_publisher_new (service_name, "test-publisher", NULL);
  epc_publisher_set_protocol (publisher, get_protocol (encryption));
  foreach_item (publish_item_cb, NULL);

  g_free (service_name);
}

int
main (int   argc,
      char *argv[])
{
  GtkCellRenderer *cell;
  GtkTreeModel *sorted;
  GtkTreeSelection *selection;

  GtkWidget *text_view;
  GtkWidget *main_window;

  g_thread_init (NULL);
  g_set_application_name ("Easy Publisher Example");

  gtk_init (&argc, &argv);
  gtk_about_dialog_set_url_hook (url_hook, NULL, NULL);

  builder = gtk_builder_new ();
  load_ui (argv[0]);
  load_settings ();

  create_publisher ();
  gtk_builder_connect_signals (builder, NULL);

  item_view = GET_WIDGET (builder, "item-view");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (item_view));
  g_signal_connect (selection, "changed", G_CALLBACK (selection_changed_cb), NULL);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "editable", TRUE, NULL);

  g_object_connect (cell,
                    "signal::editing-canceled", cell_editing_canceled_cb, NULL,
                    "signal::edited", cell_item_edited_cb, NULL,
                    NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (item_view),
                               gtk_tree_view_column_new_with_attributes (
                               NULL, cell, "text", 0, NULL));

  sorted = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (item_list));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sorted), 0, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (item_view), sorted);

  text_view = GET_WIDGET (builder, "text-view");
  text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  g_signal_connect (text_buffer, "changed", G_CALLBACK (text_buffer_changed_cb), NULL);

  main_window = GET_WIDGET (builder, "main-window");
  epc_progress_window_install (GTK_WINDOW (main_window));
  gtk_widget_show (main_window);

  gtk_main ();

  if (publisher)
    g_object_unref (publisher);

  g_object_unref (builder);

  return 0;
}
