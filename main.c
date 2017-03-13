#include <sys/types.h>
#if defined (_WIN32)
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <microhttpd.h>
#include "common.h"

#ifdef _MSC_VER
#ifndef strcasecmp
#define strcasecmp(a,b) _stricmp((a),(b))
#endif /* !strcasecmp */
#endif /* _MSC_VER */

#if defined(_MSC_VER) && _MSC_VER+0 <= 1800
/* Substitution is OK while return value is not used */
#define snprintf _snprintf
#endif


#if defined (__linux__)
/* GNU/Linux try to use epoll */
#if MHD_VERSION <= 0x00095000
#define DEFAULT_HTTPD_MODE MHD_USE_EPOLL_INTERNALLY_LINUX_ONLY
#else
#define DEFAULT_HTTPD_MODE MHD_USE_SELECT_INTERNALLY | MHD_USE_EPOLL_LINUX_ONLY
#endif
#else
/* Rest OS use select */
#define DEFAULT_HTTPD_MODE MHD_USE_SELECT_INTERNALLY
#endif

/* listener port */
#define DEFAULT_HTTPD_PORT 8888

#define DEFAULT_HTTPD_CONNECTION_TIMEOUT 15


typedef struct _httpd_options {
	unsigned int 	mode;
	uint16_t 	port;
	int 		connect_timeout;
} httpd_options;


static struct MHD_Daemon *
start_httpd (httpd_options *ops);

static void
stop_httpd (struct MHD_Daemon *daemon);

static int
accept_policy_cb (void *cls, const struct sockaddr *addr, socklen_t addrlen);

static int
answer_cb (	void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls);

static void
request_completed_cb (	void *cls,
			struct MHD_Connection *connection,
			void **con_cls,
			enum MHD_RequestTerminationCode toe);

static void
print_usage (const char *argv0);

#define print_usage_exit(program_name) do {\
	print_usage (program_name);\
	exit (EXIT_FAILURE);\
} while (0)


static struct MHD_Daemon *
start_httpd (httpd_options *ops)
{
	struct MHD_Daemon *daemon;

#define CONNECTION_TIMEOUT 	0
#define REQUEST_COMPLETED_CB	1
	struct MHD_OptionItem daemon_options[] = {
		{ 	MHD_OPTION_CONNECTION_TIMEOUT,
			DEFAULT_HTTPD_CONNECTION_TIMEOUT,
			NULL
		},
		{
			/* MHD_RequestCompletedCallback */
			MHD_OPTION_NOTIFY_COMPLETED,
			(intptr_t) &request_completed_cb,
			NULL
		},
		{ 	MHD_OPTION_END, 0, NULL } /* must be always be last */
	};

	daemon_options[CONNECTION_TIMEOUT].value = ops->connect_timeout;

#undef CONNECTION_TIMEOUT
#undef REQUEST_COMPLETED_CB

	debug ("* Start listener on port %d\n", ops->port);
	debug ("* Connection timeout: %d\n", ops->connect_timeout);

	daemon = MHD_start_daemon (
		ops->mode,
		ops->port,
		&accept_policy_cb, NULL,
		&answer_cb, NULL,
		MHD_OPTION_ARRAY, daemon_options,
		MHD_OPTION_END);

	return daemon;
}


static void
stop_httpd (struct MHD_Daemon *daemon)
{
	if (daemon != NULL)
		MHD_stop_daemon (daemon);
}


static int
accept_policy_cb (void *cls, const struct sockaddr *addr, socklen_t addrlen)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;

	/* ???: inet_ntop/WSAAddressToString */
	debug ("* Connection from %s port %d\n",
		inet_ntoa (addr_in->sin_addr),
		addr_in->sin_port);

	return MHD_YES;
}


static int
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


static void
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
	}

	debug ("* Connection %s port %d closed: %s\n",
		ip_addr, port, tdesc);

	*con_cls = NULL;
}


static void
print_usage (const char *argv0)
{
	char buffer[256];

	fprintf (stderr, "Usage: %s [-p PORT] [OPTIONS]\n", argv0);
	fprintf (stderr, "Options:\n");
#define desc(o,d) fprintf (stderr, "  %-24s  %s\n", o, d);
	snprintf (buffer, 256,
		"a port number to listen, default %d", DEFAULT_HTTPD_PORT);
	desc ("-p PORT", buffer);
	snprintf (buffer, 256,
		"a connection timeout in seconds, default %d sec.",
		DEFAULT_HTTPD_CONNECTION_TIMEOUT);
	desc ("-t CONNECTION_TIMEOUT", buffer);
#undef desc
}


extern int
main (int argc, char *argv[])
{
	struct MHD_Daemon *daemon;
	httpd_options ops;
	int opt;

	ops.mode = DEFAULT_HTTPD_MODE;
	ops.port = DEFAULT_HTTPD_PORT;
	ops.connect_timeout = DEFAULT_HTTPD_CONNECTION_TIMEOUT;

	while ((opt = getopt (argc, argv, "p:t:")) != -1) {
		switch (opt) {
		case 'p': {
			int port;
			sscanf (optarg, "%d", &port);
			if (port <= 0 || port > 65536)
				die ("Valid port range 1-65536: %s.", optarg);
			ops.port = port;
		} break;
		case 't': {
			int timeout;
			sscanf (optarg, "%d", &timeout);
			if (timeout < 0)
				die ("Invalid timeout: %s.", optarg);
			ops.connect_timeout = timeout;
		} break;
		default: /* -? */
			print_usage_exit (argv[0]);
			break;
		} /* switch (opt) { */
	}

	daemon = start_httpd (&ops);

	if (daemon == NULL) {
		fprintf (stderr, "failed to start daemon\n");
		return 1;
	}

	(void) getchar ();
	stop_httpd (daemon);

	return 0;
}
