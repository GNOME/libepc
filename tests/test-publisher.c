#include "libepc/publisher.h"

static void
report_error (const gchar  *strloc,
              GError      **error)
{
  g_assert (NULL != strloc);
  g_assert (NULL != error);

  if (!(*error))
    return;

  g_warning ("%s: %s", strloc, (*error)->message);
  g_clear_error (error);
}

int
main (int   argc,
      char *argv[])
{
  EpcPublisher *publisher;
  GError *error = NULL;

  g_thread_init (NULL);
  g_type_init ();

  publisher = epc_publisher_new ("Easy Publisher Test");

  if (1 == argc)
    {
      epc_publisher_add (publisher, "test", "value", -1);
      epc_publisher_add_file (publisher, "code", __FILE__, &error);
      report_error (G_STRLOC, &error);
    }
  else
    {
      int i;

      for (i = 1; i < argc; ++i)
        {
          char **pair = g_strsplit (argv[i], "=", 2);
          const char *key = pair[0], *value = pair[1];

          if (g_str_has_prefix (key, "file:"))
            {
              epc_publisher_add_file (publisher, key + 5, value, &error);
              report_error (G_STRLOC, &error);
            }
          else
            epc_publisher_add (publisher, key, value, -1);

          g_strfreev (pair);
        }
    }

  epc_publisher_run (publisher);
  g_object_unref (publisher);

  return 0;
}
