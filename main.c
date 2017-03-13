#include "mhd.h"
#include "common.h"
#include "server.h"

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
/* MHD_OPTION_CONNECTION_TIMEOUT */
#define DEFAULT_HTTPD_CONNECTION_TIMEOUT 15
/* MHD_OPTION_THREAD_POOL_SIZE */
#define DEFAULT_HTTPD_THREAD_POOL_SIZE 1


typedef struct _httpd_options {
	unsigned int 	mode;
	uint16_t 	port;
	int 		connect_timeout;
	unsigned int	thread_pool_size;
} httpd_options;


static struct MHD_Daemon *
start_httpd (httpd_options *ops);

static void
stop_httpd (struct MHD_Daemon *daemon);

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
#define THREAD_POOL_SIZE	2
	struct MHD_OptionItem daemon_options[] = {
		{
			/* unsigned int */
			MHD_OPTION_CONNECTION_TIMEOUT,
			DEFAULT_HTTPD_CONNECTION_TIMEOUT,
			NULL
		},
		{
			/* MHD_RequestCompletedCallback */
			MHD_OPTION_NOTIFY_COMPLETED,
			(intptr_t) &request_completed_cb,
			NULL
		},
		{
			/* unsigned int */
			MHD_OPTION_THREAD_POOL_SIZE,
			DEFAULT_HTTPD_THREAD_POOL_SIZE,
			NULL
		},
		{ 	MHD_OPTION_END, 0, NULL } /* must be always be last */
	};

	daemon_options[CONNECTION_TIMEOUT].value = ops->connect_timeout;
	daemon_options[THREAD_POOL_SIZE].value = ops->thread_pool_size;

#undef CONNECTION_TIMEOUT
#undef REQUEST_COMPLETED_CB
#undef THREAD_POOL_SIZE

	debug ("* Powered by libmicrohttpd version %s\n", MHD_get_version ());
	debug ("* Start listener on port %d\n", ops->port);
	debug ("* Connection timeout: %d\n", ops->connect_timeout);
	debug ("* Thread pool size: %d\n", ops->thread_pool_size);

	daemon = MHD_start_daemon (
		ops->mode,
		ops->port,
		&accept_policy_cb, NULL,
		&answer_cb, NULL,
		MHD_OPTION_ARRAY, daemon_options,
		MHD_OPTION_END);

	return daemon;
}

/* ------------------------------------------------------------------ */

static void
stop_httpd (struct MHD_Daemon *daemon)
{
	debug ("shutdown daemon\n");

	if (daemon != NULL)
		MHD_stop_daemon (daemon);
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
	snprintf (buffer, 256,
		"an amount of threads, default %d",
		DEFAULT_HTTPD_THREAD_POOL_SIZE);
	desc ("-T THREADS_NUM", buffer);
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
	ops.thread_pool_size = DEFAULT_HTTPD_THREAD_POOL_SIZE;

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
		case 'T': {
			int num;
			sscanf (optarg, "%d", &num);
			if (num <= 0)
				die ("Invalid thread pool size: %s.", optarg);
			ops.thread_pool_size = num;
		} break;
		default: /* -? */
			print_usage_exit (argv[0]);
			break;
		} /* switch (opt) { */
	} /* while ((opt = getopt (...) */

	daemon = start_httpd (&ops);

	if (daemon == NULL) {
		fprintf (stderr, "failed to start daemon\n");
		return 1;
	}

	(void) getchar ();
	stop_httpd (daemon);

	return 0;
}
