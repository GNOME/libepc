#include "libepc/service-type.h"

static gint n_failures = 0;

#define fail(message, ...)              _fail (G_STRLOC, (message), __VA_ARGS__)
#define check_not_null(value)           _check_not_null (G_STRLOC, (value))
#define check_str_eq(expected, value)   _check_str_eq (G_STRLOC, (expected), (value))
#define check_int_eq(expected, value)   _check_int_eq (G_STRLOC, (expected), (value))

G_GNUC_PRINTF(2, 3) static void
_fail (const gchar *strloc,
       const gchar *message,
       ...)
{
  va_list args;
  gchar *tmp;

  va_start (args, message);
  tmp = g_strdup_vprintf (message, args);
  va_end (args);

  g_print ("%s: assertion `%s' failed\n", strloc, tmp);
  g_free (tmp);
}

static gboolean
_check_not_null (const gchar   *strloc,
                 gconstpointer  value)
{
  if (NULL != value)
    return TRUE;

  _fail (strloc, "value is not NULL");
  return FALSE;
}

static gboolean
_check_str_eq (const gchar *strloc,
               const gchar *expected,
               const gchar *value)
{
  if (_check_not_null (strloc, value) &&
      g_str_equal (expected, value))
    return TRUE;

  _fail (strloc, "value is `%s' (got: `%s')", expected, value);
  return FALSE;
}

static gboolean
_check_int_eq (const gchar *strloc,
               gint         expected,
               gint         value)
{
  if (expected == value)
    return TRUE;

  _fail (strloc, "value is `%d' (got: `%d')", expected, value);
  return FALSE;
}

int
main (void)
{
  check_str_eq ("_http._tcp", epc_service_type_get_base ("_http._tcp"));
  check_str_eq ("_http._tcp", epc_service_type_get_base ("_test._sub._http._tcp"));

  check_int_eq (EPC_PROTOCOL_HTTP, epc_service_type_get_protocol (EPC_SERVICE_TYPE_HTTP));
  check_int_eq (EPC_PROTOCOL_HTTPS, epc_service_type_get_protocol (EPC_SERVICE_TYPE_HTTPS));
  check_int_eq (EPC_PROTOCOL_UNKNOWN, epc_service_type_get_protocol ("unknown"));

  return n_failures;
}
