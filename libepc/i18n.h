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

#ifndef __EPC_I18N_H__
#define __EPC_I18N_H__

#include "config.h"
#include <glib.h>

G_BEGIN_DECLS

#ifdef ENABLE_NLS

#define _(MsgId) epc_gettext((MsgId))

gchar* epc_gettext (gchar *msgid);

#else /* ENABLE_NLS */

#define _(MsgId) (MsgId)

#endif /* ENABLE_NLS */

G_END_DECLS

#endif /* __EPC_I18N_H__ */
