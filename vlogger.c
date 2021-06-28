#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#ifndef VLOOGER_NO_PTHREAD
    #include <pthread.h>
#endif
#ifndef VLOGGER_NO_SYSLOG
    #include <syslog.h>
#endif
#include "vlogger.h"


#define NULL_CHECK(x) do { assert((x) != NULL); } while (0)
#define ZERO_CHECK(x) do { assert((x) != 0); } while (0)


static vlogger_mode_t vlogger_mode;
static char *vlogger_out_file;
static char *vlogger_err_file;
static FILE *vlogger_out_fh;
static FILE *vlogger_err_fh;
static char *vlogger_syslog_ident;
static int  vlogger_syslog_facility;


#ifndef VLOGGER_NO_PTHREAD
pthread_mutex_t lock;
#endif


#ifndef VLOGGER_NO_SYSLOG
const int vlogger_syslog_levels[] = {
    LOG_INFO,
    LOG_EMERG,
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
    LOG_DEBUG
};
#endif


const char *vlogger_level_names[] = {
    "" ,
    "FATAL: ",
    "ALERT: ",
    "CRIT: ",
    "ERROR: ",
    "WARN: ",
    "NOTE: ",
    "INFO: ",
    "DEBUG: ",
    "TRACE: "
};


static int
get_syslog_facility (const char* facility)
{
#ifndef VLOGGER_NO_SYSLOG
    if (strncmp ("local0", facility, 7) == 0) return LOG_LOCAL0;
    if (strncmp ("local1", facility, 7) == 0) return LOG_LOCAL1;
    if (strncmp ("local2", facility, 7) == 0) return LOG_LOCAL2;
    if (strncmp ("local3", facility, 7) == 0) return LOG_LOCAL3;
    if (strncmp ("local4", facility, 7) == 0) return LOG_LOCAL4;
    if (strncmp ("local5", facility, 7) == 0) return LOG_LOCAL5;
    if (strncmp ("local6", facility, 7) == 0) return LOG_LOCAL6;
    if (strncmp ("local7", facility, 7) == 0) return LOG_LOCAL7;
    return LOG_USER;
#else
    (void) facility;
    return 0;
#endif
}


static int
vlogger_get_time_string (char **out, size_t out_len)
{
    struct timespec tp;
    struct tm *tmp;
    size_t len;
    char nsec[VLOGGER_NSEC_SIZE + 1];
    long x;


    assert(clock_gettime (CLOCK_REALTIME, &tp) != -1);
    NULL_CHECK(tmp = localtime (&tp.tv_sec));
    /* fill in a date to 'out' first */
    ZERO_CHECK(strftime (*out, out_len, VLOGGER_DATE_FMT, tmp));

    /* prepend zeros */
    memset (nsec, '0', VLOGGER_NSEC_SIZE - 1);
    for (len = 0, x = tp.tv_nsec;
         x != 0 && len < VLOGGER_NSEC_SIZE;
         len++, x /= 10L);
    snprintf (nsec + VLOGGER_NSEC_SIZE - len, len + 1, "%li", tp.tv_nsec);

    /* cut last digits and append the result */
    len = strlen (*out);
    assert (out_len > len + VLOGGER_NSEC_DIGITS + 2);
    /* NSEC_DIGITS + '.' + '\0' */
    snprintf (*out + len, VLOGGER_NSEC_DIGITS + 2, ".%s", nsec);
    len += VLOGGER_NSEC_DIGITS + 1;

    return len;
}


static void
close_files ()
{
    if (vlogger_out_fh) {
        fclose (vlogger_out_fh);
        vlogger_err_fh = NULL;
    }

    if (vlogger_err_fh) {
        fclose (vlogger_err_fh);
        vlogger_err_fh = NULL;
    }
}


static void
reopen_syslog ()
{
#ifndef VLOGGER_NO_SYSLOG
    closelog ();

    if (vlogger_mode == VLOGGER_MODE_SYSLOG)
        openlog (vlogger_syslog_ident, LOG_PID, vlogger_syslog_facility);
#endif
}


static void
reopen_files ()
{
    close_files ();

    if (vlogger_out_file)
        vlogger_out_fh = fopen (vlogger_out_file, "a");

    if (vlogger_err_file)
        vlogger_err_fh = fopen (vlogger_err_file, "a");
    else if (vlogger_out_fh)
        vlogger_err_fh = vlogger_out_fh;
}


extern void
vlogger_open (vlogger_t *vlogger)
{
    if (vlogger_syslog_ident)
        free (vlogger_syslog_ident);
    vlogger_syslog_ident = strdup(vlogger->syslog_ident);
    vlogger_syslog_facility = get_syslog_facility (vlogger->syslog_facility);
    vlogger_mode = vlogger->mode;


    switch (vlogger_mode) {
    case VLOGGER_MODE_NORMAL:
        vlogger_out_fh = stdout;
        vlogger_err_fh = stderr;
        break;
    case VLOGGER_MODE_FILE:
        vlogger_out_file = vlogger->outfile;
        vlogger_err_file = vlogger->errfile;
        reopen_files ();
        break;
    case VLOGGER_MODE_SYSLOG:
        reopen_syslog ();
        break;
    case VLOGGER_MODE_SILENT:
        vlogger_out_fh = NULL;
        vlogger_err_fh = NULL;
        break;
    }
}


extern void
vlogger_reload ()
{
    reopen_files ();
    reopen_syslog ();
}


extern void
vlogger_close ()
{
    close_files ();
#ifndef VLOGGER_NO_SYSLOG
    closelog ();
#endif
}


static int
vprintf_file (int level, const char *fmt, va_list ap)
{
    FILE *fh;

    switch (level) {
    case VLOGGER_FATAL:
    case VLOGGER_ALERT:
    case VLOGGER_CRIT:
    case VLOGGER_ERROR:
        fh = vlogger_err_fh;
        break;
    default:
        fh = vlogger_out_fh;
        break;
    }

    if (fh)
        return vfprintf (fh, fmt, ap);
    else
        return 0;
}


static int
fprintf_file (int level, const char *fmt, ...)
{
    int len;
    va_list ap;

    va_start(ap, fmt);
    len = vprintf_file (level, fmt, ap);
    va_end(ap);

    return len;
}


extern int
vlogger_log (int level, const char *fmt, ...)
{
    if (vlogger_mode == VLOGGER_MODE_SILENT)
        return 0;

#ifndef VLOGGER_NO_PTHREAD
    pthread_mutex_lock(&lock);
#endif

    va_list ap;
    int len = 0;

    va_start (ap, fmt);

    if (vlogger_mode == VLOGGER_MODE_SYSLOG) {
#ifndef VLOGGER_NO_SYSLOG
        ssize_t bufsz = vsnprintf (NULL, 0, fmt, ap);
        char *buf = malloc (bufsz + 1);
        NULL_CHECK(buf);
        vsnprintf (buf, bufsz + 1, fmt, ap);
        int priority = vlogger_syslog_levels[level] | vlogger_syslog_facility;
        syslog (priority, buf);
        free (buf);
#endif
    }
    else {
        char date[VLOGGER_DATE_SIZE] = "";
        char *datep = date;
        (void) vlogger_get_time_string (&datep, sizeof(date));
        len += fprintf_file (level, "%s %s", date, vlogger_level_names[level]);
        len += vprintf_file (level, fmt, ap);
    }

    va_end (ap);

#ifndef VLOGGER_NO_PTHREAD
    pthread_mutex_unlock(&lock);
#endif

    return len;
}
