#ifndef VLOGGER_H
#define VLOGGER_H

/* format of date string */
#ifndef VLOGGER_DATE_FMT
#define VLOGGER_DATE_FMT "%z %F %T"
#endif
/* array size where date string resides */
#ifndef VLOGGER_DATE_SIZE
#define VLOGGER_DATE_SIZE 64
#endif
/* amount of digits of nanoseconds to print */
#ifndef VLOGGER_NSEC_DIGITS
#define VLOGGER_NSEC_DIGITS 4
#endif
/* how many digits in tv_nsec */
#ifndef VLOGGER_NSEC_SIZE
#define VLOGGER_NSEC_SIZE 9
#endif


typedef enum {
    VLOGGER_EMERG = 1,
    VLOGGER_FATAL = 1,
    VLOGGER_ALERT = 2,
    VLOGGER_CRITICAL = 3,
    VLOGGER_CRIT = 3,
    VLOGGER_ERROR = 4,
    VLOGGER_ERR = 4,
    VLOGGER_WARNING = 5,
    VLOGGER_WARN = 5,
    VLOGGER_NOTICE = 6,
    VLOGGER_NOTE = 6,
    VLOGGER_INFO = 7,
    VLOGGER_DEBUG = 8,
    VLOGGER_TRACE = 9
} vlogger_level_t;


typedef enum {
    VLOGGER_MODE_NORMAL,
    VLOGGER_MODE_FILE,
    VLOGGER_MODE_SYSLOG,
    VLOGGER_MODE_SILENT
} vlogger_mode_t;


typedef struct vlogger_s {
    vlogger_mode_t mode;
    char *outfile;
    char *errfile;
    char *syslog_ident;
    char *syslog_facility;
} vlogger_t;


extern void
vlogger_open (vlogger_t *vlogger);


extern void
vlogger_reload ();


extern void
vlogger_close ();


extern int
vlogger_log (int level, const char *fmt, ...);


#endif
