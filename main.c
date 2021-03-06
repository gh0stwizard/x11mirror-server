#include "mhd.h"
#include "common.h"
#include "server.h"
#include "suspend.h"
#include "responses.h"
#include "vlogger.h"
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <signal.h>

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
#define DEFAULT_HTTPD_CONNECTION_MEMORY_INCREMENT (1 * 1024)


typedef struct _httpd_options {
	unsigned int    mode;
	uint16_t        port;
	int             connect_timeout;
	unsigned int    thread_pool_size;
	size_t          memory_limit;
	size_t          memory_increment;
	int             daemonize;
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
		{ 	MHD_OPTION_END, 0, NULL } /* must always be the last */
	};

	daemon_options[CONNECTION_TIMEOUT].value = ops->connect_timeout;
	daemon_options[THREAD_POOL_SIZE].value = ops->thread_pool_size;
	daemon_options[MEMORY_LIMIT].value = ops->memory_limit;
	daemon_options[MEMORY_INCREMENT].value = ops->memory_increment;

	info ("* Powered by libmicrohttpd version %s (0x%08x)\n",
		MHD_get_version (),
		MHD_VERSION);
	info ("* Start listener on port %d\n", ops->port);
	info ("* Connection timeout: %d\n", ops->connect_timeout);
	info ("* Thread pool size: %d\n", ops->thread_pool_size);
	info ("* Memory limit per connection: %zu\n", ops->memory_limit);
	info ("* Memory increment per connection: %zu\n", ops->memory_increment);

#if MHD_VERSION >= 0x00095100
	if (ops->mode & MHD_USE_EPOLL)
#else
	if (ops->mode & MHD_USE_EPOLL_LINUX_ONLY)
#endif
		info ("* Poller backend: epoll\n");
	else
		info ("* Poller backend: select\n");

	if (ops->mode & MHD_USE_TCP_FASTOPEN)
		info ("* TCP Fast Open: enabled\n");
	else
		info ("* TCP Fast Open: disabled\n");


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
	info ("* Shutdown complete\n");

	if (daemon != NULL)
		MHD_stop_daemon (daemon);
}


static void
print_usage (const char *argv0)
{
#define BUFFER_SIZE 64
	char buffer[BUFFER_SIZE];


	fprintf (stderr, "Usage: %s [-p PORT] [OPTIONS]\n", argv0);
	fprintf (stderr, "Options:\n");
#define desc(o,d) fprintf (stderr, "  %-24s  %s\n", o, d);
	desc ("-h", "print this help");
	desc ("-d", "daemonize and enables syslog logging");
	desc ("-q", "be quiet, i.e. disables all logging");
	/* listener port */
	snprintf (buffer, BUFFER_SIZE,
		"a port number to listen, default %d",
		DEFAULT_HTTPD_PORT);
	desc ("-p PORT", buffer);
	/* connection timeout */
	snprintf (buffer, BUFFER_SIZE,
		"a connection timeout in seconds, default %d sec.",
		DEFAULT_HTTPD_CONNECTION_TIMEOUT);
	desc ("-t CONNECTION_TIMEOUT", buffer);
	/* memory increment */
	snprintf (buffer, BUFFER_SIZE,
		"increment to use for growing the read buffer, default %d",
		DEFAULT_HTTPD_CONNECTION_MEMORY_INCREMENT);
	desc ("-I MEMORY_INCREMENT", buffer);
	/* debug switch */
	desc ("-D", "enable MHD debug, disabled by default")
	/* epoll switch */
#if defined(__linux__)
	desc ("-E", "enable epoll backend (Linux only)");
	/* TCP Fast Open switch */
	desc ("-F", "enable TCP Fast Open support (Linux only)");
#endif
	/* file store location, dirpath */
	desc ("-L DIR_PATH",
		"a directory where files will be stored, default `.'");
	/* memory limit */
	snprintf (buffer, BUFFER_SIZE,
		"max memory size per connection, default %d",
		DEFAULT_HTTPD_CONNECTION_MEMORY_LIMIT);
	desc ("-M MEMORY_LIMIT", buffer);
	/* an amount of threads */
	snprintf (buffer, BUFFER_SIZE,
		"an amount of threads, default %d",
		DEFAULT_HTTPD_THREAD_POOL_SIZE);
	desc ("-T THREADS_NUM", buffer);
#undef desc
#undef BUFFER_SIZE
}


static void
daemonize (void)
{
	pid_t cpid;
	struct timespec wtime = { .tv_sec = 0, .tv_nsec = 100 * 1000000 };

	while ((cpid = fork ()) == -1)
		nanosleep(&wtime, NULL);
	if (cpid > 0)
		exit (EXIT_SUCCESS);
}


static volatile sig_atomic_t sigflag;
static sigset_t newmask, oldmask, zeromask;


static void
sig_cb (int signo)
{
	(void) signo;
	sigflag = 1;
}


static void
init_signal_handlers (void)
{
	if (signal (SIGTERM, sig_cb) == SIG_ERR)
		die ("signal\n");
	if (signal (SIGINT, sig_cb) == SIG_ERR)
		die ("signal\n");
	sigemptyset (&zeromask);
	sigemptyset (&newmask);
	sigaddset (&newmask, SIGTERM);
	sigaddset (&newmask, SIGINT);
	if (sigprocmask (SIG_BLOCK, &newmask, &oldmask) < 0)
		die ("sig_block\n");
}


extern int
main (int argc, char *argv[])
{
	struct MHD_Daemon *daemon;
	httpd_options ops;
	vlogger_t vlogger;
	int opt;
	const struct timespec ts_wait = { 2, 0 };


	ops.daemonize = 0;
	ops.mode = DEFAULT_HTTPD_MODE;
	ops.port = DEFAULT_HTTPD_PORT;
	ops.connect_timeout = DEFAULT_HTTPD_CONNECTION_TIMEOUT;
	ops.thread_pool_size = DEFAULT_HTTPD_THREAD_POOL_SIZE;
	ops.memory_limit = DEFAULT_HTTPD_CONNECTION_MEMORY_LIMIT;
	ops.memory_increment = DEFAULT_HTTPD_CONNECTION_MEMORY_INCREMENT;

	vlogger.syslog_ident = "x11mirror-server";
	vlogger.syslog_facility = "";
	vlogger.mode = VLOGGER_MODE_NORMAL;
	vlogger.outfile = NULL;
	vlogger.errfile = NULL;

	while ((opt = getopt (argc, argv, "dqhp:t:DEFI:L:M:T:")) != -1) {
		switch (opt) {
		case 'h': print_usage_exit (argv[0]);
		case 'p': {
			int port;
			sscanf (optarg, "%d", &port);
			if (port <= 0 || port > 65536)
				die ("Valid port range 1-65536: %s.\n", optarg);
			ops.port = port;
		} break;
		case 't': {
			int timeout;
			sscanf (optarg, "%d", &timeout);
			if (timeout < 0)
				die ("Invalid timeout: %s.\n", optarg);
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
		case 'F':
			ops.mode |= MHD_USE_TCP_FASTOPEN;
			break;
#endif
		case 'I': {
			int increment;
			sscanf (optarg, "%d", &increment);
			if (increment < 0)
				die ("Invalid memory increment: %s.\n", optarg);
			ops.memory_increment = increment;
		} break;
		case 'L': {
			/* see server.c */
			XMS_STORAGE_DIR = optarg;
			break;
		} break;
		case 'M': {
			int limit;
			sscanf (optarg, "%d", &limit);
			if (limit < 0)
				die ("Invalid memory limit: %s.\n", optarg);
			ops.memory_limit = limit;
		} break;
		case 'T': {
			int num;
			sscanf (optarg, "%d", &num);
			if (num <= 0)
				die ("Invalid thread pool size: %s.\n", optarg);
			ops.thread_pool_size = num;
		} break;
		case 'q':
			vlogger.mode = VLOGGER_MODE_SILENT;
			break;
		case 'd':
			if (vlogger.mode != VLOGGER_MODE_SILENT)
				vlogger.mode = VLOGGER_MODE_SYSLOG;
			ops.daemonize = 1;
			break;
		default: /* -? */
			print_usage_exit (argv[0]);
			break;
		} /* switch (opt) { */
	} /* while ((opt = getopt (...) */

	/* default storage location */
	if (XMS_STORAGE_DIR == NULL)
		XMS_STORAGE_DIR = ".";

	if (ops.daemonize)
		daemonize ();

	init_signal_handlers ();

	vlogger_open (&vlogger);

	/* initialize server internal data (server.c) */
	init_server_data ();

	/* initialize MHD default responses (responses.c) */
	init_mhd_responses ();

	/* we store suspended connections in special pool (suspend.c) */
	init_suspend_pool ();
	
	daemon = start_httpd (&ops);

	if (daemon == NULL) {
		fatal ("failed to start daemon\n");
		return 1;
	}

	while (sigflag == 0)
		sigsuspend (&zeromask);
	sigflag = 0;

	note ("* Shutting down the daemon...\n");

	resume_all_connections ();
	/* we have to wait a bit, to get a chance MHD resume connections properly */
	nanosleep (&ts_wait, NULL);

	/* FIXME: replace default callbacks to stubs */
	stop_httpd (daemon);
	free_mhd_responses ();
	free_suspend_pool ();
	free_server_data ();

	vlogger_close ();

	return 0;
}
