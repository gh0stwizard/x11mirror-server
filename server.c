#include "mhd.h"
#include "common.h"


extern int
accept_policy_cb (void *cls, const struct sockaddr *addr, socklen_t addrlen)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;

	/* ???: inet_ntop/WSAAddressToString */
	debug ("* Connection from %s port %d\n",
		inet_ntoa (addr_in->sin_addr),
		addr_in->sin_port);

	return MHD_YES;
}


extern int
answer_cb (	void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	return MHD_YES;
}


extern void
request_completed_cb (	void *cls,
			struct MHD_Connection *connection,
			void **con_cls,
			enum MHD_RequestTerminationCode toe)
{
	char *tdesc;
	const union MHD_ConnectionInfo *ci;
	char *ip_addr;
	in_port_t port;


	ci = MHD_get_connection_info (
		connection,
		MHD_CONNECTION_INFO_CLIENT_ADDRESS);

	if (ci != NULL) {
		struct sockaddr_in *addr_in = *(struct sockaddr_in **) ci;
		ip_addr = inet_ntoa (addr_in->sin_addr);
		port = addr_in->sin_port;
	}
	else {
		fprintf (stderr, "failed to get client info address\n");
		ip_addr = "<unknown>";
		port = 0;
	}

	switch (toe) {
	case MHD_REQUEST_TERMINATED_COMPLETED_OK:
		tdesc = "OK";
		break;
	case MHD_REQUEST_TERMINATED_WITH_ERROR:
		tdesc = "ERROR";
		break;
	case MHD_REQUEST_TERMINATED_TIMEOUT_REACHED:
		tdesc = "TIMED OUT";
		break;
	case MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN:
		tdesc = "SHUTDOWN";
		break;
	case MHD_REQUEST_TERMINATED_READ_ERROR:
		tdesc = "READ ERROR";
		break;
	case MHD_REQUEST_TERMINATED_CLIENT_ABORT:
		tdesc = "CLIENT ABORT";
		break;
	default:
		tdesc = "UNKNOWN";
		break;
	} /* switch (toe) { */

	debug ("* Connection %s port %d closed: %s\n",
		ip_addr, port, tdesc);

	*con_cls = NULL;
}
