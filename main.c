#include "mhd.h"
#include "common.h"
#include "server.h"
#include "suspend.h"
#include <errno.h>
#include <limits.h>

#ifdef _MSC_VER
#ifndef strcasecmp
#define strcasecmp(a,b) _stricmp((a),(b))
#endif /* !strcasecmp */
#endif /* _MSC_VER */

#if defined(_MSC_VER) && _MSC_VER+0 <= 1800
/* Substitution is OK while return value is not used */
#define snprintf _snprintf
#endif

/* Default MHD daemon mode */
#define DEFAULT_HTTPD_MODE MHD_USE_SELECT_INTERNALLY | MHD_USE_SUSPEND_RESUME
/* Listener port */
#define DEFAULT_HTTPD_PORT 8888
/* MHD_OPTION_CONNECTION_TIMEOUT */
#define DEFAULT_HTTPD_CONNECTION_TIMEOUT 15
/* MHD_OPTION_THREAD_POOL_SIZE */
#define DEFAULT_HTTPD_THREAD_POOL_SIZE 1
/* MHD_OPTION_CONNECTION_MEMORY_LIMIT */
#define DEFAULT_HTTPD_CONNECTION_MEMORY_LIMIT (128 * 1024)
/* MHD_OPTION_CONNECTION_MEMORY_INCREMENT */
#define DEFAULT_HTTPD_CONNECTION_MEMORY_INCREMENT (4 * 1024)


typedef struct _httpd_options {
	unsigned int 	mode;
	uint16_t 	port;
	int 		connect_timeout;
	unsigned int	thread_pool_size;
	size_t		memory_limit;
	size_t		memory_increment;
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


/* ------------------------------------------------------------------ */


static struct MHD_Daemon *
start_httpd (httpd_options *ops)
{
	struct MHD_Daemon *daemon;
	enum daemon_options_index {
		CONNECTION_TIMEOUT = 0,
		REQUEST_COMPLETED_CB,
		THREAD_POOL_SIZE,
		MEMORY_LIMIT,
		MEMORY_INCREMENT
	};
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
		{
			/* size_t */
			MHD_OPTION_CONNECTION_MEMORY_LIMIT,
			DEFAULT_HTTPD_CONNECTION_MEMORY_LIMIT,
			NULL
		},
		{
			/* size_t */
			MHD_OPTION_CONNECTION_MEMORY_INCREMENT,
			DEFAULT_HTTPD_CONNECTION_MEMORY_INCREMENT,
			NULL
		},
		{
			MHD_OPTION_NOTIFY_CONNECTION,
			(intptr_t) &notify_connection_cb,
			NULL
		},
		{ 	MHD_OPTION_END, 0, NULL } /* must be always be last */
	};

	daemon_options[CONNECTION_TIMEOUT].value = ops->connect_timeout;
	daemon_options[THREAD_POOL_SIZE].value = ops->thread_pool_size;
	daemon_options[MEMORY_LIMIT].value = ops->memory_limit;
	daemon_options[MEMORY_INCREMENT].value = ops->memory_increment;

	debug ("* Powered by libmicrohttpd version %s (0x%08x)\n",
		MHD_get_version (),
		MHD_VERSION);
	debug ("* Start listener on port %d\n", ops->port);
	debug ("* Connection timeout: %d\n", ops->connect_timeout);
	debug ("* Thread pool size: %d\n", ops->thread_pool_size);
	debug ("* Memory limit per connection: %u\n", ops->memory_limit);
	debug ("* Memory increment per connection: %u\n", ops->memory_increment);

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
	debug ("* Shutdown daemon.\n");

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
		"increment to use for growing the read buffer, default %d",
		DEFAULT_HTTPD_CONNECTION_MEMORY_INCREMENT);
	desc ("-D", "enable MHD debug, disabled by default");
#if defined(__linux__)
	desc ("-E", "enable epoll backend (Linux only)");
#endif
	desc ("-I MEMORY_INCEMENT", buffer);
	snprintf (buffer, 256,
		"max memory size per connection, default %d",
		DEFAULT_HTTPD_CONNECTION_MEMORY_LIMIT);
	desc ("-M MEMORY_LIMIT", buffer);
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
	ops.memory_limit = DEFAULT_HTTPD_CONNECTION_MEMORY_LIMIT;
	ops.memory_increment = DEFAULT_HTTPD_CONNECTION_MEMORY_INCREMENT;

	while ((opt = getopt (argc, argv, "p:t:DEI:M:T:")) != -1) {
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
		case 'D':
			ops.mode |= MHD_USE_DEBUG;
			break;
#if defined(__linux__)
		case 'E':
#if MHD_VERSION >= 0x00095100
			ops.mode |= MHD_USE_EPOLL;
#else
			ops.mode |= MHD_USE_EPOLL_LINUX_ONLY;
#endif
			break;
#endif
		case 'I': {
			int increment;
			sscanf (optarg, "%d", &increment);
			if (increment < 0)
				die ("Invalid memory increment: %s.", optarg);
			ops.memory_increment = increment;
		} break;
		case 'M': {
			int limit;
			sscanf (optarg, "%d", &limit);
			if (limit < 0)
				die ("Invalid memory limit: %s.", optarg);
			ops.memory_limit = limit;
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

	/* initialize MHD default responses */
	init_mhd_responses ();

	/* we store suspended connection in special pool */
	init_suspend_pool ();
	
	daemon = start_httpd (&ops);

	if (daemon == NULL) {
		fprintf (stderr, "failed to start daemon\n");
		return 1;
	}

	/* pass MHD_Daemon handle to server.c */
	setup_daemon_handler (daemon);
	setup_daemon_options (ops.mode, ops.connect_timeout);

#if MHD_VERSION >= 0x00095100
	if (ops.mode & MHD_USE_EPOLL) {
#else
	if (ops.mode & MHD_USE_EPOLL_LINUX_ONLY) {
#endif
		debug ("* Using epoll backend\n");
		(void) getchar ();
	}
	else {
		/* we have to run MHD_run() if using select backend */
		debug ("* Using select backend\n");
		(void) getchar ();
	}


	resume_all_connections (daemon, ops.mode);
	stop_httpd (daemon);
	clear_daemon_handler ();
	free_mhd_responses ();

	return 0;
}
