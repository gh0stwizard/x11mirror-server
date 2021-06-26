#include <assert.h>
#include "warn.h"
#include "vlogger.h"


extern void
warn (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list ap;
	const union MHD_ConnectionInfo *ci;
	char *ip;
	in_port_t port = 0;


	ci = MHD_get_connection_info (
		connection,
		MHD_CONNECTION_INFO_CLIENT_ADDRESS);

	if (ci != NULL) {
		struct sockaddr_in *addr_in = *(struct sockaddr_in **) ci;
		ip = inet_ntoa (addr_in->sin_addr);
		port = addr_in->sin_port;
	}
	else
		ip = "<unknown>";

	va_start (ap, fmt);
	ssize_t bufsz = vsnprintf (NULL, 0, fmt, ap);
	char *buf = malloc (bufsz + 1);
	assert (buf != NULL);
	vsnprintf (buf, bufsz + 1, fmt, ap);
	va_end (ap);

	vlogger_log (VLOGGER_ERROR, "! %s port %u: %s\n", ip, port, buf);
}
