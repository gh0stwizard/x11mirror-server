#include "warn.h"


extern void
warn (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list args;
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

	va_start (args, fmt);
	fprintf (stderr, "! %s port %u: ", ip, port);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
}
