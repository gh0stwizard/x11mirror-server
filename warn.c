#include "warn.h"
#include "timeinfo.h"


extern void
warn (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list args;
	const union MHD_ConnectionInfo *ci;
	char *ip;
	in_port_t port = 0;
	char date[DATE_SIZE];
	char *datep = date;


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

	if (get_current_time_string (&datep, sizeof(date))) {
		fprintf (stderr, "%s ! %s port %u: ", date, ip, port);
	}
	else {
		fprintf (stderr, "! %s port %u: ", ip, port);
	}
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
}
