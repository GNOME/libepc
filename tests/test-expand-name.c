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
#include <libepc/publisher.h>
#include <libepc/shell.h>
#include <locale.h>

static gchar*
ascii_strtitle (const gchar *str,
                gssize       len)
{
  gchar *title = g_ascii_strdown (str, len);
  *title = g_ascii_toupper (*title);
  return title;
}

static gint
test_expand_name (void)
{
  GError *error = NULL;

  gchar *host = ascii_strtitle (epc_shell_get_host_name (&error), -1);
  gchar *user = ascii_strtitle (g_get_user_name (), -1);

  gint failures = 0;
  guint i;

  struct {
    const gchar *pattern;
    gchar *expected;
  } tests[] = {
    { NULL,             epc_publisher_expand_name ("%a of %u on %h", &error) },
    { "Plain Text",     g_strdup ("Plain Text") },

    { "%%a: %a",        g_strconcat ("%a: ", g_get_application_name (), NULL) },
    { "%%h: %h",        g_strconcat ("%h: ", host, NULL) },
    { "%%u: %u",        g_strconcat ("%u: ", user, NULL) },
    { "%%U: %U",        g_strconcat ("%U: ", g_get_real_name (), NULL) },

    { "%%x%y",          g_strdup ("%x%y") },
  };

  if (error)
    {
      g_warning ("%s: %s", G_STRLOC, error->message);
      failures += 1;
    }

  for (i = 0; i < G_N_ELEMENTS (tests); ++i)
    {
      gchar *expand = epc_publisher_expand_name (tests[i].pattern, &error);
      gboolean success = g_str_equal (expand, tests[i].expected);

      g_print ("%d: \"%s\" => \"%s\": %s\n", 
               i, tests[i].pattern, expand,
               success ? "pass" : "fail");

      if (!success)
        failures += 1;

      if (error)
        {
          g_warning ("%s: test%d: %s\n", G_STRLOC, i, error->message);
          failures += 1;
        }

      g_clear_error (&error);
      g_free (expand);
    }

  if (failures)
    g_print ("number of failed tests: %d\n", failures);

  return failures;
}

int
main (void)
{
  setlocale (LC_ALL, "C");
  g_set_application_name ("Test Name Expansion");
  return test_expand_name ();
}
