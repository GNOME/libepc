#include "framework.h"

#include <libepc/shell.h>
#include <libepc-ui/progress-window.h>
#include <gtk/gtk.h>

static gchar test_title[] = "Test";
static gchar test_start_message[] = "Start";
static gchar test_update_message1[] = "Update #1";
static gchar test_update_message2[] = "Update #2";
static gchar test_update_message3[] = "Update #3";
static gchar test_update_message4[] = "Update #4";
static gchar test_update_message5[] = "Update #5";
static gchar test_user_data[] = "User Data";
static gchar test_context[] = "Context";

static gpointer
begin_cb (const gchar *title,
          const gchar *message,
          gpointer     data)
{
  g_return_val_if_fail (title == test_title, NULL);
  g_return_val_if_fail (message == test_start_message, NULL);
  g_return_val_if_fail (data == test_user_data, NULL);

  epc_test_pass_once (1 << 0);

  return test_context;
}

static void
update_cb (gpointer     context,
           gdouble      progress,
           const gchar *message)
{
  g_return_if_fail (context == test_context);

  switch ((int)((progress + 1) * 2))
    {
      case 0:
        g_return_if_fail (message == test_update_message1);
        epc_test_pass_once (1 << 1);
        break;

      case 2:
        g_return_if_fail (message == test_update_message2);
        epc_test_pass_once (1 << 2);
        break;

      case 3:
        g_return_if_fail (message == test_update_message3);
        epc_test_pass_once (1 << 3);
        break;

      case 4:
        g_return_if_fail (message == test_update_message4);
        epc_test_pass_once (1 << 4);
        break;

      case 5:
        g_return_if_fail (message == test_update_message5);
        epc_test_pass_once (1 << 5);
        break;

      default:
        g_assert_not_reached ();
    }
}

static void
end_cb (gpointer context)
{
  g_return_if_fail (context == test_context);
  test_context[0] = '\0';
}

static void
destroy_cb (gpointer data)
{
  g_return_if_fail (data == test_user_data);
  g_return_if_fail ('\0' == test_context[0]);
  epc_test_pass_once (1 << 6);
}

static gboolean
timeout_cb (gpointer data)
{
  g_main_loop_quit (data);
  return FALSE;
}

static void
wait (gboolean have_ui)
{
  if (have_ui)
    {
      GMainLoop *loop = g_main_loop_new (NULL, FALSE);
      g_timeout_add (500, timeout_cb, loop);

      g_main_loop_run (loop);
      g_main_loop_unref (loop);
    }
}

int
main (int   argc,
      char *argv[])
{
  EpcShellProgressHooks hooks = {
    begin:  begin_cb,
    update: update_cb,
    end:    end_cb
  };

  gpointer context;
  gint i;

  epc_test_init (7);

  for (i = 0; i < 4; ++i)
    {
      switch (i)
        {
          case 1:
            epc_shell_set_progress_hooks (&hooks, test_user_data, destroy_cb);
            break;

          case 2:
            epc_shell_set_progress_hooks (NULL, NULL, NULL);
            break;

          case 3:
            if (!gtk_init_check (&argc, &argv))
              {
                g_warning ("Cannot initialize UI");
                continue;
              }

            epc_progress_window_install (NULL);
            break;
        }

      context = epc_shell_progress_begin (test_title, test_start_message);
      wait (3 == i);

      epc_shell_progress_update (context,  -1, test_update_message1);
      wait (3 == i);

      epc_shell_progress_update (context, 0.0, test_update_message2);
      wait (3 == i);

      epc_shell_progress_update (context, 0.5, test_update_message3);
      wait (3 == i);

      epc_shell_progress_update (context, 1.0, test_update_message4);
      wait (3 == i);

      epc_shell_progress_update (context, 1.5, test_update_message5);
      wait (3 == i);

      epc_shell_progress_end (context);
      wait (3 == i);
    }

  return epc_test_quit () & EPC_TEST_MASK_USER;
}
