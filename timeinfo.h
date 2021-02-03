#ifndef XMS_TIMEINFO_H
#define XMS_TIMEINFO_H

#include <sys/types.h>

#define TIME_FMT	"%z %F %T"
#define DATE_SIZE	256


extern int
get_current_time_string (char *out[], size_t out_len);


#endif /* XMS_TIMEINFO_H */
