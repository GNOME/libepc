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
#include "libepc/consumer.h"
#include "libepc/publisher.h"

#include "framework.h"

int
main (void)
{
  EpcPublisher *publisher = NULL;
  EpcConsumer *consumer = NULL;
  gboolean running = FALSE;
  GError *error = NULL;
  gchar *value = NULL;
  const gchar *name;
  gint result = 1;

  g_set_prgname (__FILE__);
  g_thread_init (NULL);
  g_type_init ();

  publisher = epc_publisher_new (NULL, NULL, NULL);
  epc_test_goto_if_fail (EPC_IS_PUBLISHER (publisher), out);
  epc_publisher_set_protocol (publisher, EPC_PROTOCOL_HTTP);
  epc_publisher_add (publisher, "maman", "bar", -1);

  running = epc_publisher_run_async (publisher, &error);
  epc_test_goto_if_fail (running, out);

  name = epc_publisher_get_service_name (publisher);
  epc_test_goto_if_fail (NULL != name, out);

  consumer = epc_consumer_new_for_name (name);
  epc_test_goto_if_fail (EPC_IS_CONSUMER (consumer), out);

  value = epc_consumer_lookup (consumer, "maman", NULL, &error);
  epc_test_goto_if_fail (value && g_str_equal (value, "bar"), out);

  result = 0;

out:
  if (error)
    g_warning ("%s: lookup failed: %s", G_STRLOC, error->message);

  g_clear_error (&error);
  g_free (value);

  if (consumer)
    g_object_unref (consumer);

  if (publisher)
    {
      epc_publisher_quit (publisher);
      g_object_unref (publisher);
    }

  return result;
}
