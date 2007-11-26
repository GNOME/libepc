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
#include "shell.h"

#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <gnutls/gnutls.h>

/**
 * SECTION:shell
 * @short_description: library management functions
 * @include: libepc/shell.h
 * @stability: Private
 *
 * The methods of the #EpcShell singleton are used to manage library resources.
 */

typedef struct _EpcShellWatch EpcShellWatch;

struct _EpcShellWatch
{
  guint          id;
  GCallback      callback;
  gpointer       user_data;
  GDestroyNotify destroy_data;
};

static gpointer epc_shell_progress_begin_default  (const gchar *title,
                                                   const gchar *message,
                                                   gpointer     user_data);
static void     epc_shell_progress_update_default (gpointer     context,
                                                   gdouble      progress,
                                                   const gchar *message);

static EpcShellProgressHooks
epc_shell_default_progress_hooks =
{
  begin: epc_shell_progress_begin_default,
  update: epc_shell_progress_update_default,
  end: g_free
};

static volatile gint  epc_shell_ref_count = 0;
static AvahiGLibPoll *epc_shell_avahi_poll = NULL;
static AvahiClient   *epc_shell_avahi_client = NULL;
static GArray        *epc_shell_watches = NULL;

static void (*epc_shell_threads_enter)(void) = NULL;
static void (*epc_shell_threads_leave)(void) = NULL;

static const EpcShellProgressHooks  *epc_shell_progress_hooks = &epc_shell_default_progress_hooks;
static gpointer                      epc_shell_progress_user_data = NULL;
static GDestroyNotify                epc_shell_progress_destroy_data = NULL;

gboolean _epc_debug = FALSE;

/**
 * epc_shell_ref:
 *
 * Increases the reference count for this library. When called for the first
 * time essential resources are allocated. Each call to this function should 
 * be paired with a call to #epc_shell_unref.
 */
void
epc_shell_ref (void)
{
  if (0 == g_atomic_int_exchange_and_add (&epc_shell_ref_count, 1))
    {
      GModule *module;
      gpointer symbol;

      if (g_getenv ("EPC_DEBUG"))
        _epc_debug = TRUE;

      gnutls_global_init ();
      avahi_set_allocator (avahi_glib_allocator ());

      g_assert (NULL == epc_shell_avahi_poll);
      epc_shell_avahi_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);

      g_assert (NULL == epc_shell_threads_enter);
      g_assert (NULL == epc_shell_threads_leave);

      module = g_module_open (NULL, G_MODULE_BIND_LAZY);

      symbol = NULL;
      g_module_symbol (module, "gdk_threads_enter", &symbol);
      epc_shell_threads_enter = symbol;

      symbol = NULL;
      g_module_symbol (module, "gdk_threads_leave", &symbol);
      epc_shell_threads_leave = symbol;

      if (_epc_debug)
        g_debug ("%s: threads_enter=%p, threads_leave=%p", G_STRLOC,
                 epc_shell_threads_enter, epc_shell_threads_leave);

      g_module_close (module);
    }

  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: %d", G_STRFUNC, epc_shell_ref_count);
}

/**
 * epc_shell_unref:
 *
 * Decreases the reference count for this library. When the reference count
 * reaches zero, global resources allocated by the library are released.
 */
void
epc_shell_unref (void)
{
  if (G_UNLIKELY (_epc_debug))
    g_debug ("%s: %d", G_STRFUNC, epc_shell_ref_count);

  if (g_atomic_int_dec_and_test (&epc_shell_ref_count))
    {
      if (NULL != epc_shell_avahi_client)
        {
          avahi_client_free (epc_shell_avahi_client);
          epc_shell_avahi_client = NULL;
        }

      g_assert (NULL != epc_shell_avahi_poll);
      avahi_glib_poll_free (epc_shell_avahi_poll);
      epc_shell_avahi_poll = NULL;

      epc_shell_threads_enter = NULL;
      epc_shell_threads_leave = NULL;
    }
}

/**
 * epc_shell_leave:
 *
 * Releases the big GDK lock when running unter GDK/GTK+.
 * This must be called before entering GLib main loops to avoid race
 * conditions. See #gdk_threads_leave for details.
 */
void
epc_shell_leave (void)
{
  if (epc_shell_threads_leave)
    epc_shell_threads_leave ();
}

/**
 * epc_shell_enter:
 *
 * Acquires the big GDK lock when running unter GDK/GTK+.
 * This should be called after leaving GLib main loops to avoid race
 * conditions. See #gdk_threads_enter for details.
 */
void
epc_shell_enter (void)
{
  if (epc_shell_threads_enter)
    epc_shell_threads_enter ();
}

static guint
epc_shell_watches_length (void)
{
  if (epc_shell_watches)
    return epc_shell_watches->len;

  return 0;
}

static EpcShellWatch*
epc_shell_watches_get (guint index)
{
  g_return_val_if_fail (index < epc_shell_watches_length (), NULL);
  return &g_array_index (epc_shell_watches, EpcShellWatch, index);
}

static EpcShellWatch*
epc_shell_watches_last (void)
{
  if (epc_shell_watches && epc_shell_watches->len)
    return epc_shell_watches_get (epc_shell_watches->len - 1);

  return NULL;
}

static guint
epc_shell_watch_add (GCallback      callback,
                     gpointer       user_data,
                     GDestroyNotify destroy_data)
{
  EpcShellWatch *last, *self;

  if (NULL == epc_shell_watches)
    epc_shell_watches = g_array_sized_new (TRUE, TRUE, sizeof (EpcShellWatch), 4);

  g_return_val_if_fail (NULL != epc_shell_watches, 0);

  last = epc_shell_watches_last ();

  g_array_set_size (epc_shell_watches, epc_shell_watches->len + 1);

  self = epc_shell_watches_last ();

  self->id = (last ? last->id + 1 : 1);
  self->callback = callback;
  self->user_data = user_data;
  self->destroy_data = destroy_data;

  return self->id;
}

/**
 * epc_shell_watch_remove:
 * @id: identifier of an watching callback
 *
 * Removes the watching callback identified by @id.
 *
 * See also: #epc_shell_watch_avahi_client
 */
void
epc_shell_watch_remove (guint id)
{
  guint idx, len;

  g_return_if_fail (id > 0);

  if (!epc_shell_watches)
    return;

  len = epc_shell_watches_length ();

  for (idx = MIN (id, len) - 1; idx < len; ++idx)
    if (epc_shell_watches_get (idx)->id == id)
      break;

  if (idx < len)
    g_array_remove_index (epc_shell_watches, idx);
}

static void
epc_shell_avahi_client_cb (AvahiClient      *client,
                           AvahiClientState  state,
                           gpointer          user_data G_GNUC_UNUSED)
{
  guint i;

  if (epc_shell_avahi_client)
    g_assert (client == epc_shell_avahi_client);
  else
    epc_shell_avahi_client = client;

  if (epc_shell_watches)
    for (i = 0; i < epc_shell_watches->len; ++i)
      {
        EpcShellWatch *watch = &g_array_index (epc_shell_watches, EpcShellWatch, i);
        ((AvahiClientCallback) watch->callback) (client, state, watch->user_data);
      }
}

static AvahiClient*
epc_shell_get_avahi_client (GError **error)
{
  g_return_val_if_fail (NULL != epc_shell_avahi_client ||
                        NULL != error, NULL);

  if (G_UNLIKELY (NULL == epc_shell_avahi_client))
    {
      gint error_code = AVAHI_OK;

      epc_shell_avahi_client =
        avahi_client_new (avahi_glib_poll_get (epc_shell_avahi_poll),
                          AVAHI_CLIENT_NO_FAIL, epc_shell_avahi_client_cb,
                          NULL, &error_code);

      if (NULL == epc_shell_avahi_client)
        g_set_error (error, EPC_AVAHI_ERROR, error_code,
                     _("Cannot create Avahi client: %s"),
                     avahi_strerror (error_code));
    }

  return epc_shell_avahi_client;
}

/**
 * epc_shell_watch_avahi_client_state:
 * @callback: a callback function
 * @user_data: data to pass to @callback
 * @destroy_data: a function for freeing @user_data when removing the watch
 * @error: return location for a #GError, or %NULL
 *
 * Registers a function to watch state changes of the library's #AvahiClient.
 * On success the identifier of the newly created watch is returned. Pass it
 * to #epc_shell_watch_remove to remove the watch. On failure it returns 0 and
 * sets @error. The error domain is #EPC_AVAHI_ERROR. Possible error codes are
 * those of the <citetitle>Avahi</citetitle> library.
 *
 * Returns: The identifier of the newly created watch, or 0 on error.
 */
guint
epc_shell_watch_avahi_client_state (AvahiClientCallback callback,
                                    gpointer            user_data,
                                    GDestroyNotify      destroy_data,
                                    GError            **error)
{
  AvahiClient *client = epc_shell_get_avahi_client (error);
  guint id = 0;

  g_return_val_if_fail (NULL != callback, 0);

  if (NULL != client)
    {
      id = epc_shell_watch_add (G_CALLBACK (callback), user_data, destroy_data);
      callback (client, avahi_client_get_state (client), user_data);
    }

  return id;
}

/**
 * epc_shell_create_avahi_entry_group:
 * @callback: a callback function
 * @user_data: data to pass to @callback
 *
 * Creates a new #AvahiEntryGroup for the library's shared #AvahiClient.
 * On success the newly created #AvahiEntryGroup is returned,
 * and %NULL on failure.
 *
 * Returns: The newly created #AvahiEntryGroup, or %NULL on error.
 */
AvahiEntryGroup*
epc_shell_create_avahi_entry_group (AvahiEntryGroupCallback callback,
                                    gpointer                user_data)
{
  AvahiClient *client = epc_shell_get_avahi_client (NULL);
  AvahiEntryGroup *group = NULL;

  if (NULL != client)
    group = avahi_entry_group_new (client, callback, user_data);

  return group;
}

/**
 * epc_shell_create_service_browser:
 * @interface: the index of the network interface to watch
 * @protocol: the protocol of the services to watch
 * @type: the DNS-SD service type to watch
 * @domain: the DNS domain to watch, or %NULL
 * @flags: flags for creating the service browser
 * @callback: a callback function
 * @user_data: data to pass to @callback
 * @error: return location for a #GError, or %NULL
 *
 * Creates a new #AvahiServiceBrowser for the library's shared #AvahiClient.
 * On success the newly created #AvahiEntryGroup is returned. On failure
 * %NULL is returned and @error is set.The error domain is #EPC_AVAHI_ERROR.
 * Possible error codes are those of the <citetitle>Avahi</citetitle> library.
 *
 * Returns: The newly created #AvahiServiceBrowser, or %NULL on error.
 */
AvahiServiceBrowser*
epc_shell_create_service_browser (AvahiIfIndex                interface,
                                  AvahiProtocol               protocol,
                                  const gchar                *type,
                                  const gchar                *domain,
                                  AvahiLookupFlags            flags,
                                  AvahiServiceBrowserCallback callback,
                                  gpointer                    user_data,
                                  GError                    **error)
{
  AvahiClient *client = epc_shell_get_avahi_client (error);
  AvahiServiceBrowser *browser = NULL;

  if (NULL != client)
    browser = avahi_service_browser_new (client, interface, protocol, type,
                                         domain, flags, callback, user_data);

  return browser;
}

/**
 * epc_shell_get_host_name:
 *
 * Query the official host name of this machine.
 *
 * Returns: The official host name, or %NULL on error.
 */
G_CONST_RETURN gchar*
epc_shell_get_host_name (void)
{
  return avahi_client_get_host_name (epc_shell_get_avahi_client (NULL));
}

static void
epc_shell_progress_update_default (gpointer     context,
                                   gdouble      progress,
                                   const gchar *message)
{
  const gchar *title = context;

  if (NULL == title)
    title = _("Progress Changed");
  if (NULL == message)
    message = _("No details known");

  if (progress >= 0 && progress <= 1)
    g_message ("%s: %s (%.1f %%)", title, message, progress * 100);
  else
    g_message ("%s: %s", title, message);
}

static gpointer
epc_shell_progress_begin_default (const gchar *title,
                                  const gchar *message,
                                  gpointer     user_data G_GNUC_UNUSED)
{
  gpointer context = NULL;

  if (title)
    context = g_strdup (title);

  epc_shell_progress_update_default (context, -1, message);

  return context;
}

/**
 * epc_shell_set_progress_hooks:
 * @hooks: the function table to install, or %NULL
 * @user_data: custom data which shall be passed to the start hook
 * @destroy_data: function to call on @user_data when the hooks are replaced
 *
 * Installs functions which are called during processing, such as generating 
 * server keys. This allows the application to indicate progress and generally 
 * keep its UI responsive. If no progress callbacks are provided,
 * or when %NULL is passed for @hooks, progress messages are written to the
 * console.
 *
 * See also: #EpcEntropyProgress
 */
void
epc_shell_set_progress_hooks (const EpcShellProgressHooks *hooks,
                              gpointer                     user_data,
                              GDestroyNotify               destroy_data)
{
  if (epc_shell_progress_destroy_data)
    epc_shell_progress_destroy_data (epc_shell_progress_user_data);

  if (NULL == hooks)
    hooks = &epc_shell_default_progress_hooks;

  epc_shell_progress_hooks = hooks;
  epc_shell_progress_user_data = user_data;
  epc_shell_progress_destroy_data = destroy_data;
}

/**
 * epc_shell_progress_begin:
 * @title: the title of the lengthy operation
 * @message: description of the lengthy operation
 *
 * Call this function before starting a lengthy operation to allow the
 * application tp provide some visual feedback during the operation, 
 * and to generally keep its UI responsive.
 *
 * See also: #epc_shell_set_progress_hooks, #epc_progress_window_install,
 * #epc_shell_progress_update, #epc_shell_progress_end
 *
 * Returns: Context information that is passed to the other progress functions.
 */
gpointer
epc_shell_progress_begin (const gchar *title,
                          const gchar *message)
{
  if (epc_shell_progress_hooks->begin)
    return epc_shell_progress_hooks->begin (title, message, epc_shell_progress_user_data);

  return NULL;
}

/**
 * epc_shell_progress_update:
 * @context: the context information returned by #epc_shell_progress_begin
 * @percentage: current progress of the operation, or -1
 * @message: a description of the current progress
 *
 * Called this function to inform about progress of a lengthy operation.
 * The progress is expressed as @percentage in the range [0..1], or -1 if the
 * progress cannot be estimated.
 *
 * See also: #epc_shell_set_progress_hooks, #epc_progress_window_install,
 * epc_shell_progress_begin, #epc_shell_progress_end
 */
void
epc_shell_progress_update (gpointer     context,
                           gdouble      percentage,
                           const gchar *message)
{
  g_assert (NULL != epc_shell_progress_hooks);

  if (epc_shell_progress_hooks->update)
    epc_shell_progress_hooks->update (context, percentage, message);
}

/**
 * epc_shell_progress_end:
 * @context: the context information returned by #epc_shell_progress_begin
 *
 * Call this function when your lengthy operation has finished.
 *
 * See also: #epc_shell_set_progress_hooks, #epc_progress_window_install,
 * #epc_shell_progress_begin, #epc_shell_progress_update
 */
void
epc_shell_progress_end (gpointer context)
{
  g_assert (NULL != epc_shell_progress_hooks);

  if (epc_shell_progress_hooks->end)
    epc_shell_progress_hooks->end (context);
}

GQuark
epc_avahi_error_quark (void)
{
  return g_quark_from_static_string ("epc-avahi-error-quark");
}
