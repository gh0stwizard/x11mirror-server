#include <assert.h>
#include "mhd_log.h"
#include "vlogger.h"


static void
mhd_vlog (vlogger_level_t level,
          struct MHD_Connection *conn,
          const char *fmt, va_list ap)
{
	const union MHD_ConnectionInfo *ci;
	char *ip = NULL;
	in_port_t port = 0;

	ci = MHD_get_connection_info (
		conn,
		MHD_CONNECTION_INFO_CLIENT_ADDRESS);

	if (ci != NULL) {
		struct sockaddr_in *addr_in = *(struct sockaddr_in **) ci;
		ip = inet_ntoa (addr_in->sin_addr);
		port = addr_in->sin_port;
	}
	else {
		ip = "<unknown>";
	}

	ssize_t bufsz = vsnprintf (NULL, 0, fmt, ap);
	char *buf = malloc (bufsz + 1);
	assert (buf != NULL);
	vsnprintf (buf, bufsz + 1, fmt, ap);

	vlogger_log (level, "%s:%u: %s\n", ip, port, buf);
}


extern void
mhd_warn (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	mhd_vlog (VLOGGER_WARN, connection, fmt, ap);
	va_end (ap);
}


extern void
mhd_note (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	mhd_vlog (VLOGGER_NOTE, connection, fmt, ap);
	va_end (ap);
}


extern void
mhd_info (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	mhd_vlog (VLOGGER_INFO, connection, fmt, ap);
	va_end (ap);
}


extern void
mhd_error (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	mhd_vlog (VLOGGER_ERROR, connection, fmt, ap);
	va_end (ap);
}


extern void
mhd_debug (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	mhd_vlog (VLOGGER_DEBUG, connection, fmt, ap);
	va_end (ap);
}
