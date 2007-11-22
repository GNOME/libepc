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
#ifndef __EPC_CONTENTS_H__
#define __EPC_CONTENTS_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _EpcContents EpcContents;

/**
 * EpcContentsReadFunc:
 * @contents: a #EpcContents buffer
 * @length: a location for storing the contents length
 * @user_data: the user_data passed to #epc_contents_stream_new
 *
 * This callback is used to retrieve the next chunk of data for a streaming
 * contents buffer created with #epc_contents_stream_read. Return %NULL when
 * the buffer has reached its end, and no more data is available.
 *
 * See also: #epc_contents_stream_new, #epc_contents_stream_read
 *
 * Returns: Returns the next chunk of data, or %NULL.
 */
typedef gpointer    (*EpcContentsReadFunc)       (EpcContents         *contents,
                                                  gsize               *length,
                                                  gpointer             user_data);

EpcContents*          epc_contents_new           (const gchar         *type,
                                                  gpointer             data,
                                                  gsize                length);
EpcContents*          epc_contents_stream_new    (const gchar         *type,
                                                  EpcContentsReadFunc  callback,
                                                  gpointer             user_data,
                                                  GDestroyNotify       destroy_data);
gpointer              epc_contents_stream_read   (EpcContents         *contents,
                                                  gsize               *length);

gboolean              epc_contents_is_stream     (EpcContents         *contents);
G_CONST_RETURN gchar* epc_contents_get_mime_type (EpcContents         *contents);
gpointer              epc_contents_get_data      (EpcContents         *contents,
                                                  gsize               *length);

EpcContents*          epc_contents_ref           (EpcContents         *contents);
void                  epc_contents_unref         (EpcContents         *contents);

G_END_DECLS

#endif /* __EPC_CONTENTS_H__ */
