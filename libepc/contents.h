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
#ifndef __EPC_CONTENTS_H__
#define __EPC_CONTENTS_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _EpcContents EpcContents;

/**
 * EpcContentsReadFunc:
 * @contents: a #EpcContents buffer
 * @buffer: a location for storing the contents, or %NULL
 * @length: a location for passing and storing the contents length in bytes.
 * @user_data: the user_data passed to #epc_contents_stream_new
 *
 * This callback is used to retrieve the next chunk of data for a streaming
 * contents buffer created with #epc_contents_stream_read.
 *
 * Return %FALSE when the buffer has reached its end, and no more data is
 * available. Also return %FALSE, when the buffer size passed in @length is
 * not sufficient. Copy your minimal buffer size to @length in that situation.
 *
 * The library might pass %NULL for @buffer on the first call to start buffer
 * size negotation.
 *
 * See also: #epc_contents_stream_new, #epc_contents_stream_read
 *
 * Returns: Returns %TRUE when the next chunk could be read, and %FALSE on error.
 */
typedef gboolean    (*EpcContentsReadFunc)       (EpcContents         *contents,
                                                  gpointer             buffer,
                                                  gsize               *length,
                                                  gpointer             user_data);

EpcContents*          epc_contents_new           (const gchar         *type,
                                                  gpointer             data,
                                                  gssize               length,
                                                  GDestroyNotify       destroy_data);
EpcContents*          epc_contents_new_dup       (const gchar         *type,
                                                  gconstpointer        data,
                                                  gssize               length);
EpcContents*          epc_contents_stream_new    (const gchar         *type,
                                                  EpcContentsReadFunc  callback,
                                                  gpointer             user_data,
                                                  GDestroyNotify       destroy_data);

EpcContents*          epc_contents_ref           (EpcContents         *contents);
void                  epc_contents_unref         (EpcContents         *contents);

gboolean              epc_contents_is_stream     (EpcContents         *contents);
G_CONST_RETURN gchar* epc_contents_get_mime_type (EpcContents         *contents);

gconstpointer         epc_contents_get_data      (EpcContents         *contents,
                                                  gsize               *length);
gconstpointer         epc_contents_stream_read   (EpcContents         *contents,
                                                  gsize               *length);

G_END_DECLS

#endif /* __EPC_CONTENTS_H__ */
