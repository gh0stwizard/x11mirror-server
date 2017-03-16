#ifndef _TIMEINFO_H
#define _TIMEINFO_H


#define TIME_FMT	"%z %F %T"
#define DATE_SIZE	256


extern int
get_current_time_string (char *out[], size_t out_len);


#endif /* _TIMEINFO_H */
