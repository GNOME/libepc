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
#ifndef __EPC_TEST_FRAMEWORK_H__
#define __EPC_TEST_FRAMEWORK_H__

#include <avahi-client/lookup.h>
#include <glib.h>

G_BEGIN_DECLS

#define epc_test_pass_many(mask) (_epc_test_pass (G_STRLOC, FALSE, (mask)))
#define epc_test_pass_once(mask) (_epc_test_pass (G_STRLOC, TRUE, (mask)))
#define epc_test_quit()          (_epc_test_quit (G_STRLOC))

#define epc_test_goto_if_fail(Test, Label) G_STMT_START{        \
  if (!(Test))                                                  \
    {                                                           \
      g_warning ("%s: Assertion failed: %s", G_STRLOC, #Test);  \
      goto Label;                                               \
    }                                                           \
}G_STMT_END

enum
{
  EPC_TEST_MASK_INIT = 128,
  EPC_TEST_MASK_USER = 127,
  EPC_TEST_MASK_ALL = 255
};

gboolean epc_test_init                 (gint                         tests);
gboolean epc_test_init_service_browser (const gchar                 *service,
                                        AvahiServiceBrowserCallback  callback,
                                        gpointer                     data);
gint     epc_test_run                  (void);
gint    _epc_test_quit                 (const gchar                 *strloc);

void    _epc_test_pass                 (const gchar                 *strloc,
                                        gboolean                     once,
                                        gint                         mask);

G_END_DECLS

#endif /* __EPC_TEST_FRAMEWORK_H__ */
