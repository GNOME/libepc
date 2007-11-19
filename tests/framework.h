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
void     epc_test_pass                 (gint                         mask);
gint     epc_test_run                  (void);
void     epc_test_quit                 (void);

G_END_DECLS

#endif /* __EPC_TEST_FRAMEWORK_H__ */
