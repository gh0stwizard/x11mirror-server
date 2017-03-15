#ifndef _CONTEXTS_H
#define _CONTEXTS_H

#include "mhd.h"
#include <stdbool.h>


enum request_type {
	GET 	= 0,
	POST	= 1
};


typedef struct _request_ctx {
	/* Request type: GET, POST, etc */
	enum request_type type;

	/* Handle to the POST processing state. */
	struct MHD_PostProcessor *pp;

	/* File handle where we write uploaded data. */
	FILE *fh;

	/* Filename to upload */
	char *filename;

	/* HTTP response body we will return, NULL if not yet known. */
	struct MHD_Response *response;

	/* HTTP status code we will return, 0 for undecided. */
	unsigned int status;

	/* Suspend index */
	size_t suspend_index;

	/* Is this request current uploader */
	bool uploader;
} request_ctx;


typedef struct _socket_ctx {
	/* previous connection timeout value */
	unsigned int prev_timeout;
} socket_ctx;


#endif /* _CONTEXTS_H */
