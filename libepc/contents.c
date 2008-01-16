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

#include "libepc/contents.h"
#include "libepc/shell.h"

#include <string.h>
#include <unistd.h>

/**
 * SECTION:contents
 * @short_description: custom contents
 * @see_also: #EpcPublisher
 * @include: libepc/contents.h
 * @stability: Unstable
 *
 * #EpcContents is a reference counted structure for storing custom contents.
 * To publish custom content call epc_publisher_add_handler() to register a
 * #EpcContentsHandler like this:
 *
 * <example id="custom-contents-handler">
 *  <title>A custom contents handler</title>
 *  <programlisting>
 *   static EpcContents*
 *   timestamp_handler (EpcPublisher *publisher G_GNUC_UNUSED,
 *                      const gchar  *key G_GNUC_UNUSED,
 *                      gpointer      data)
 *   {
 *     time_t now = time (NULL);
 *     struct tm *tm = localtime (&now);
 *     const gchar *format = data;
 *     gsize length = 60;
 *     gchar *buffer;
 *
 *     buffer = g_malloc (length);
 *     length = strftime (buffer, length, format, tm);
 *
 *     return epc_content_new ("text/plain", buffer, length);
 *   }
 *  </programlisting>
 * </example>
 */

/**
 * EpcContents:
 *
 * A reference counted buffer for storing contents to deliver by the
 * #EpcPublisher. Use epc_contents_new() or #epc_contents_new_dup to create
 * instances of this buffer.
 */
struct _EpcContents
{
  volatile gint       ref_count;
  gchar              *type;

  gpointer            buffer;
  gsize               buffer_size;
  GDestroyNotify      destroy_buffer;

  EpcContentsReadFunc callback;
  gpointer            user_data;
  GDestroyNotify      destroy_data;
};

/**
 * epc_contents_new:
 * @type: the MIME type of this contents, or %NULL
 * @data: static contents for the buffer
 * @length: the contents length in bytes, or -1 if @data is a null-terminated string.
 * @destroy_data: This function will be called to free @data when it is no longer needed.
 *
 * Creates a new #EpcContents buffer, and takes ownership of the @data passed.
 * Passing %NULL for @type is equivalent to passing "application/octet-stream".
 *
 * See also: epc_contents_new_dup, epc_contents_stream_new
 *
 * Returns: The newly created #EpcContents buffer.
 */
EpcContents*
epc_contents_new (const gchar    *type,
                  gpointer        data,
                  gssize          length,
                  GDestroyNotify  destroy_data)
{
  EpcContents *self;

  g_return_val_if_fail (NULL != data, NULL);

  self = g_slice_new0 (EpcContents);
  self->ref_count = 1;

  if (type)
    self->type = g_strdup (type);
  if (-1 == length)
    length = strlen (data);

  self->buffer = data;
  self->buffer_size = length;
  self->destroy_buffer = destroy_data;

  return self;
}

/**
 * epc_contents_new_dup:
 * @type: the MIME type of this contents, or %NULL
 * @data: static contents for the buffer
 * @length: the content's length in bytes, or -1 if @data is a null-terminated string. 
 *
 * Creates a new #EpcContents buffer, and copies the @data passed.
 * Passing %NULL for @type is equivalent to passing "application/octet-stream".
 *
 * See also: epc_contents_new, epc_contents_stream_new
 *
 * Returns: The newly created #EpcContents buffer.
 */
EpcContents*
epc_contents_new_dup (const gchar  *type,
                      gconstpointer data,
                      gssize        length)
{
  gpointer cloned_data;

  g_return_val_if_fail (NULL != data, NULL);

  if (-1 == length)
    length = strlen (data);

  cloned_data = g_malloc (MAX (1, length));
  memcpy (cloned_data, data, length);

  return epc_contents_new (type, cloned_data, length, g_free);
}

/**
 * epc_contents_stream_new:
 * @type: the MIME type of this contents, or %NULL
 * @callback: the function for retrieving chunks
 * @user_data: data which will be passed to @callback
 * @destroy_data:  This function will be called to free @user_data when it is no longer needed.
 *
 * Creates a new #EpcContents buffer for large contents like movie files,
 * which cannot, or should be delivered as solid blob of data.
 *
 * Passing %NULL for @type is equivalent to passing "application/octet-stream".
 *
 * See also: epc_contents_stream_read(), #epc_contents_is_stream
 *
 * Returns: The newly created #EpcContents buffer.
 */
EpcContents*
epc_contents_stream_new (const gchar         *type,
                         EpcContentsReadFunc  callback,
                         gpointer             user_data,
                         GDestroyNotify       destroy_data)
{
  EpcContents *self;

  g_return_val_if_fail (NULL != callback, NULL);

  self = g_slice_new0 (EpcContents);
  self->ref_count = 1;

  if (type)
    self->type = g_strdup (type);

  self->callback = callback;
  self->user_data = user_data;
  self->destroy_data = destroy_data;
  self->destroy_buffer = g_free;

  return self;
}

/**
 * epc_contents_ref:
 * @contents: a #EpcContents buffer
 *
 * Increases the reference count of @contents.
 *
 * Returns: the same @contents buffer.
 */
EpcContents*
epc_contents_ref (EpcContents *self)
{
  g_return_val_if_fail (NULL != self, NULL);

  g_atomic_int_inc (&self->ref_count);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: self=%p, ref_count=%d", G_STRFUNC, self, self->ref_count);

  return self;
}

/**
 * epc_contents_unref:
 * @contents: a #EpcContents buffer
 *
 * Decreases the reference count of @contents.
 * When its reference count drops to 0, the buffer is released
 * (i.e. its memory is freed).
 */
void
epc_contents_unref (EpcContents *self)
{
  g_return_if_fail (NULL != self);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: self=%p, ref_count=%d", G_STRFUNC, self, self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      if (self->destroy_buffer)
        self->destroy_buffer (self->buffer);
      if (self->destroy_data)
        self->destroy_data (self->user_data);

      g_free (self->type);

      g_slice_free (EpcContents, self);
    }
}

/**
 * epc_contents_is_stream:
 * @contents: a #EpcContents buffer
 *
 * Checks if stream routines can be used for retreiving
 * the contents of the buffer.
 *
 * See also: epc_contents_stream_new(), #epc_contents_stream_read
 *
 * Returns: Returns %TRUE when stream routines have to be used.
 */
gboolean
epc_contents_is_stream (EpcContents *contents)
{
  return contents && contents->callback;
}

/**
 * epc_contents_get_mime_type:
 * @contents: a #EpcContents buffer
 *
 * Queries the MIME type associated with the buffer. Returns the MIME
 * type specified for epc_contents_new() or #epc_contents_stream_new,
 * or "application/octet-stream" when %NULL was passed.
 *
 * Returns: Returns the MIME type of the buffer.
 */
G_CONST_RETURN gchar*
epc_contents_get_mime_type (EpcContents *self)
{
  g_return_val_if_fail (NULL != self, NULL);

  if (self->type)
    return self->type;

  return "application/octet-stream";
}

/**
 * epc_contents_get_data:
 * @contents: a #EpcContents buffer
 * @length: a location for storing the contents length
 *
 * Retrieves the contents of a static contents buffer created with
 * epc_contents_new(). Any other buffer returns %NULL. The data returned
 * is owned by the #EpcContents buffer and must not be freeded.
 *
 * See also: epc_contents_stream_read().
 *
 * Returns: Returns the static buffer contents, or %NULL. This should not be freed or modified.
 */
gconstpointer
epc_contents_get_data (EpcContents *contents,
                       gsize       *length)
{
  g_return_val_if_fail (NULL != contents, NULL);

  if (epc_contents_is_stream (contents))
    return NULL;

  if (length)
    *length = contents->buffer_size;

  return contents->buffer;
}

/**
 * epc_contents_stream_read:
 * @contents: a #EpcContents buffer
 * @length: a location for storing the contents length
 *
 * Retrieves the next chunk of data for a streaming contents buffer created
 * with epc_contents_stream_read(). %NULL is returned, when the buffer has
 * reached its end, or isn't a streaming contents buffer.
 *
 * The data returned is owned by the #EpcContents buffer and must not be
 * freeded by the called. Make sure to copy the returned data before the
 * function again, as repeated calls to the function might return the
 * same buffer, but filled with new data.
 *
 * See also: epc_contents_stream_new(), #epc_contents_is_stream
 *
 * Returns: Returns the next chunk of data, or %NULL. The should not be freed or modified.
 */
gconstpointer
epc_contents_stream_read (EpcContents *self,
                          gsize       *length)
{
  gconstpointer data = NULL;

  g_return_val_if_fail (epc_contents_is_stream (self), NULL);
  g_return_val_if_fail (NULL != length, NULL);


  if (0 == self->buffer_size)
    self->buffer_size = sysconf (_SC_PAGESIZE);

  *length = self->buffer_size;

  if (self->callback (self, self->buffer, length, self->user_data))
    data = self->buffer;
  else if (*length > 0)
    {
      gssize page_size = sysconf (_SC_PAGESIZE);
      gsize page_count = (*length + page_size - 1) / page_size;

      self->buffer_size = page_count * page_size;
      self->buffer = g_realloc (self->buffer, self->buffer_size);

      *length = self->buffer_size;

      if (self->callback (self, self->buffer, length, self->user_data))
        data = self->buffer;
    }

  return data;
}
