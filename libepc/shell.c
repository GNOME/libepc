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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "libepc/shell.h"

#include <avahi-common/error.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>

#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <gnutls/gnutls.h>

#include <stdlib.h>
#include <string.h>

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

static AvahiGLibPoll *epc_shell_avahi_poll = NULL;
static AvahiClient   *epc_shell_avahi_client = NULL;
static gboolean       epc_shell_restart_avahi_client_allowed = TRUE;
static GArray        *epc_shell_watches = NULL;

static void (*epc_shell_threads_enter)(void) = NULL;
static void (*epc_shell_threads_leave)(void) = NULL;

static const EpcShellProgressHooks  *epc_shell_progress_hooks = NULL;
static gpointer                      epc_shell_progress_user_data = NULL;
static GDestroyNotify                epc_shell_progress_destroy_data = NULL;

/**
 * epc_shell_get_debug_level:
 *
 * Query the library's debug level. The debug level can be modified by setting
 * the external <varname>EPC_DEBUG</varname> environment variable. In most
 * cases the #EPC_DEBUG_LEVEL macro should be used, instead of calling this
 * checking the return value of this function.
 *
 * Returns: The library's current debugging level.
 */
guint
epc_shell_get_debug_level (void)
{
  static gint level = -1;

  if (G_UNLIKELY (-1 == level))
    {
      const gchar *text = g_getenv ("EPC_DEBUG");
      level = text ? MAX (0, atoi (text)) : 0;
    }

  return level;
}

static void
epc_shell_exit (void)
{
  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: releasing libepc resources", G_STRLOC);

  if (NULL != epc_shell_avahi_client)
    {
      avahi_client_free (epc_shell_avahi_client);
      epc_shell_avahi_client = NULL;
    }

  if (epc_shell_avahi_poll)
    {
      avahi_glib_poll_free (epc_shell_avahi_poll);
      epc_shell_avahi_poll = NULL;
    }

  epc_shell_threads_enter = NULL;
  epc_shell_threads_leave = NULL;
}

static void
epc_shell_init (void)
{
  if (G_UNLIKELY (NULL == epc_shell_avahi_poll))
    {
      gnutls_global_init ();
      avahi_set_allocator (avahi_glib_allocator ());
      g_atexit (epc_shell_exit);

      epc_shell_avahi_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
      g_assert (NULL != epc_shell_avahi_poll);

      bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    }
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
 * See also: epc_shell_watch_avahi_client()
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
    {
      epc_shell_restart_avahi_client_allowed = FALSE;

      for (i = 0; i < epc_shell_watches->len; ++i)
        {
          EpcShellWatch *watch = &g_array_index (epc_shell_watches, EpcShellWatch, i);
          ((AvahiClientCallback) watch->callback) (client, state, watch->user_data);
        }

      epc_shell_restart_avahi_client_allowed = TRUE;
    }

  if (AVAHI_CLIENT_FAILURE == state)
    {
      gint error_code = avahi_client_errno (client);

      g_warning ("%s: Avahi client failed: %s.",
                 G_STRFUNC, avahi_strerror (error_code));

      /* Restart the client __after__ notifying all watchers to give them
       * a chance to react. Restarting the client before notification would
       * risk that watchers deal with stale AvahiClient references. */
      epc_shell_restart_avahi_client (G_STRLOC);
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

      epc_shell_init ();

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
 * to epc_shell_watch_remove() to remove the watch. On failure it returns 0 and
 * sets @error. The error domain is #EPC_AVAHI_ERROR. Possible error codes are
 * those of the <citetitle>Avahi</citetitle> library.
 *
 * <note><para>
 *  Usually there is no need in handling the #AVAHI_CLIENT_FAILURE state
 *  for @callback, as the callback dispatcher takes care already. This
 *  state is dispatched mostly for notification purposes.
 * </para></note>
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
 * %NULL is returned and @error is set. The error domain is #EPC_AVAHI_ERROR.
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

  if (error && *error)
    return NULL;

  if (NULL != client)
    browser = avahi_service_browser_new (client, interface, protocol, type,
                                         domain, flags, callback, user_data);

  if (G_UNLIKELY (!browser))
    g_set_error (error, EPC_AVAHI_ERROR, AVAHI_ERR_FAILURE,
                 _("Cannot create Avahi service browser."));

  return browser;
}

/**
 * epc_shell_restart_avahi_client:
 * @strloc: code location of the callee (#G_STRLOC)
 *
 * Requests restart of the builtin Avahi client. Use this function to react on
 * critical failures (like #AVAHI_ENTRY_GROUP_FAILURE) reported to your Avahi
 * callbacks. The @strloc argument is used to prevent endless restart cycles.
 *
 * <note><para>
 *  Do not call this function to react on #AVAHI_CLIENT_FAILURE in a
 *  #AvahiClientCallback. The builtin callback dispatcher deals with
 *  that situation.
 * </para></note>
 */
void
epc_shell_restart_avahi_client (const gchar *strloc G_GNUC_UNUSED)
{
  GError *error = NULL;

  g_return_if_fail (epc_shell_restart_avahi_client_allowed);
  g_warning ("%s: Restarting Avahi client.", G_STRFUNC);

  /* TODO: Use strloc to prevent endless restart cycles. */

  if (epc_shell_avahi_client)
    {
      avahi_client_free (epc_shell_avahi_client);
      epc_shell_avahi_client = NULL;
    }

  if (!epc_shell_get_avahi_client (&error))
    {
      g_warning ("%s: %s", G_STRFUNC, error->message);
      g_clear_error (&error);
    }
}

/**
 * epc_shell_get_host_name:
 * @error: return location for a #GError, or %NULL
 *
 * Retrieves the official host name of this machine. On failure the function
 * returns %NULL and sets @error. The error domain is #EPC_AVAHI_ERROR.
 * Possible error codes are those of the <citetitle>Avahi</citetitle> library.
 *
 * Returns: The official host name, or %NULL on error.
 */
const gchar*
epc_shell_get_host_name (GError **error)
{
  AvahiClient *client = epc_shell_get_avahi_client (error);

  if (client)
    return avahi_client_get_host_name (client);

  return NULL;
}

static void
epc_shell_progress_begin_default (const gchar *title,
                                  gpointer     user_data)
{
  gchar **context = user_data;
  g_assert (NULL != context);

  if (title)
    *context = g_strdup (title);
}

static void
epc_shell_progress_update_default (gdouble      progress,
                                   const gchar *message,
                                   gpointer     user_data)
{
  const gchar **context = user_data;
  const gchar *title;

  g_assert (NULL != context);
  title = *context;

  if (NULL == title)
  {
    // Translators: This is just a generic default message for a progress bar.
    title = _("Operation in Progress");
  }

  if (NULL == message)
    message = _("No details known");

  if (progress >= 0 && progress <= 1)
    g_message ("%s: %s (%.1f %%)", title, message, progress * 100);
  else
    g_message ("%s: %s", title, message);
}

static void
epc_shell_progress_end_default (gpointer     user_data)
{
  gchar **context = user_data;
  g_assert (NULL != context);
  g_free (*context);
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
    {
      static EpcShellProgressHooks default_hooks =
      {
        begin: epc_shell_progress_begin_default,
        update: epc_shell_progress_update_default,
        end: epc_shell_progress_end_default,
      };

      g_return_if_fail (NULL == user_data);
      g_return_if_fail (NULL == destroy_data);

      hooks = &default_hooks;
      user_data = g_new0 (gchar*, 1);
      destroy_data = g_free;
    }

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
 * This function calls your #EpcShellProgressHooks::begin hook with @title
 * as argument and #EpcShellProgressHooks::update with @message.
 *
 * See also: epc_shell_set_progress_hooks(), #epc_progress_window_install,
 * epc_shell_progress_update(), #epc_shell_progress_end
 */
void
epc_shell_progress_begin (const gchar *title,
                          const gchar *message)
{
  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: %s", G_STRFUNC, title);

  if (!epc_shell_progress_hooks)
    epc_shell_set_progress_hooks (NULL, NULL, NULL);
  if (epc_shell_progress_hooks->begin)
    epc_shell_progress_hooks->begin (title, epc_shell_progress_user_data);

  epc_shell_progress_update (-1, message);
}

/**
 * epc_shell_progress_update:
 * @percentage: current progress of the operation, or -1
 * @message: a description of the current progress
 *
 * Called this function to inform about progress of a lengthy operation.
 * The progress is expressed as @percentage in the range [0..1], or -1 if the
 * progress cannot be estimated.
 *
 * See also: epc_shell_set_progress_hooks(), #epc_progress_window_install,
 * epc_shell_progress_begin, epc_shell_progress_end()
 */
void
epc_shell_progress_update (gdouble      percentage,
                           const gchar *message)
{
  g_assert (NULL != epc_shell_progress_hooks);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s: %s", G_STRFUNC, message);

  if (epc_shell_progress_hooks->update)
    epc_shell_progress_hooks->update (percentage, message, epc_shell_progress_user_data);
}

/**
 * epc_shell_progress_end:
 *
 * Call this function when your lengthy operation has finished.
 *
 * See also: epc_shell_set_progress_hooks(), #epc_progress_window_install,
 * epc_shell_progress_begin(), #epc_shell_progress_update
 */
void
epc_shell_progress_end (void)
{
  g_assert (NULL != epc_shell_progress_hooks);

  if (EPC_DEBUG_LEVEL (1))
    g_debug ("%s", G_STRFUNC);

  if (epc_shell_progress_hooks->end)
    epc_shell_progress_hooks->end (epc_shell_progress_user_data);
}

GQuark
epc_avahi_error_quark (void)
{
  return g_quark_from_static_string ("epc-avahi-error-quark");
}
