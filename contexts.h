#ifndef XMS_CONTEXTS_H
#define XMS_CONTEXTS_H

#include "mhd.h"
#include <stdbool.h>


enum request_type {
	GET 	= 0,
	POST	= 1
};

typedef struct _request_ctx {
	/* Request type: GET, POST, etc */
	enum request_type type;

	/* POST: Handle to the POST processing state. */
	struct MHD_PostProcessor *pp;

	/* File handle to write uploaded data. */
	FILE *fh;

	/* HTTP response body we will return, NULL if not yet known. */
	struct MHD_Response *response;

	/* HTTP status code we will return, 0 for undecided. */
	unsigned int status;

	/* POST: Is this request current uploader */
	bool uploader;

	/* GET: a client asking for our datafile */
	bool getfile;
} request_ctx;

#endif /* XMS_CONTEXTS_H */
