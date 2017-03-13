/*
   Taken from microhttpd.h

   If generic headers don't work on your platform, include headers
   which define 'va_list', 'size_t', 'ssize_t', 'intptr_t',
   'uint16_t', 'uint32_t', 'uint64_t', 'off_t', 'struct sockaddr',
   'socklen_t', 'fd_set' and "#define MHD_PLATFORM_H" before
   including "microhttpd.h". Then the following "standard"
   includes won't be used (which might be a good idea, especially
   on platforms where they do not exist).
*/
#include <sys/types.h>
#if defined (_WIN32)
#include <winsock2.h>
#else
#define _XOPEN_SOURCE 500 /* strdup */
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
