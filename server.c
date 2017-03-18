#include "mhd.h"
#include "common.h"
#include "suspend.h"
#include "warn.h"
#include "contexts.h"
#include "responses.h"
#include "mhd_utils.h"

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#if defined (_MSC_VER)
#ifndef strcasecmp
#define strcasecmp(a,b) _stricmp((a),(b))
#endif /* !strcasecmp */
#else
#include <strings.h>
#endif /* _MSC_VER */

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif


/* storage location, dirpath (global)*/
char *XMS_STORAGE_DIR = NULL;

/* temporary & target filepaths, see init_server_data () */
#ifndef TEMP_FILENAME
#define TEMP_FILENAME "xms.temp"
#endif
static char *XMS_TEMP_FILE;
#ifndef DEST_FILENAME
#define DEST_FILENAME "xms.dest"
#endif
static char *XMS_DEST_FILE;

/* we allow only one uploader per a moment */
volatile bool busy = false;

/* From libmicrohttpd manual:
   maximum number of bytes to use for internal buffering (used only for
   the parsing, specifically the parsing of the keys). A tiny value (256-1024)
   should be sufficient; do NOT use a value smaller than 256; for good
   performance, use 32k or 64k (i.e. 65536).
*/
#define POSTBUFFERSIZE (64 * 1024)


static int
upload_post_chunk (	void *coninfo_cls,
			enum MHD_ValueKind kind,
			const char *key,
			const char *filename,
			const char *content_type,
			const char *transfer_encoding,
			const char *data,
			uint64_t off,
			size_t size);

static FILE *
open_file (struct MHD_Response **response, unsigned int *status);

static void
destroy_request_ctx (request_ctx *req);


/* ------------------------------------------------------------------ */


extern int
accept_policy_cb (void *cls, const struct sockaddr *addr, socklen_t addrlen)
{
#if defined(_DEBUG)
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	(void) cls;
	(void) addrlen;

	/* ???: inet_ntop/WSAAddressToString */
	debug ("* Connection from %s port %d\n",
		inet_ntoa (addr_in->sin_addr),
		addr_in->sin_port);
#else
	(void) cls;
	(void) addr;
	(void) addrlen;
#endif

	return MHD_YES;
}


extern int
answer_cb (	void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	request_ctx *req = *con_cls;
	(void) cls;
	(void) url;
	(void) version;


	if (req == NULL) {
		/* initialize our request information */
		req = malloc (sizeof (*req));

		if (req == NULL) {
			warn (connection, "malloc (req) failed");
			return MHD_NO;
		}

		req->status = 0; /* we are not finished yet */
		req->pp = NULL;
		req->fh = NULL;
		req->uploader = false;

		/* initialize post processor */
		if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST)) {
			req->pp = MHD_create_post_processor (
					connection,
					POSTBUFFERSIZE,
					&upload_post_chunk,
					(void *) req);

			if (req->pp == NULL) {
				warn (	connection,
					"failed to create post processor");
				destroy_request_ctx (req);
				return MHD_NO;
			}

			req->type = POST;
		}
		else
			req->type = GET;

		*con_cls = (void *) req;

		return MHD_YES;
	}

	if (req->status != 0) {
		/* something went wrong... */
		if (busy && req->uploader) {
			/* we've failed in the middle of upload */
			busy = false;
			req->uploader = false;
			resume_next ();
		}

		if (*upload_data_size == 0) {
			/* we can send a response only after reading all
			   headers & data. */
			return MHD_queue_response
				(connection, req->status, req->response);
		}
		else {
			/* do nothing and wait until all headers & data */
			*upload_data_size = 0;

			return MHD_YES;
		}
	}

	if (req->type == POST) {
		if (busy && ! req->uploader) {
			/* no need to update upload_data_size, because
			 * overwise we have to store the first data chunk
                         * somewhere and if we don't we will lost
			 * the filename header & data too.
                         */
			suspend_connection (connection);
			return MHD_YES;
		}

		if (*upload_data_size > 0) {
			/* uploading data */
			if (MHD_YES == MHD_post_process
				(req->pp, upload_data, *upload_data_size))
			{
				busy = true;

				/* we do this only once to prevent a spam
				 * "uploading..."
				 */
				if (! req->uploader) {
					/* seems to be everything is good */
					req->uploader = true;
					warn (connection, "uploading...");
				}
			}
			else
				warn (connection, "upload has been failed");

			/* a data was proceeded no matter how */
			*upload_data_size = 0;

			return MHD_YES;
		}

		/* there is no more data */

		if (req->fh != NULL) {
			/* close the file ASAP */
			fclose (req->fh);
			req->fh = NULL;
		}

		if (req->status == 0) {
			/* upload successfully finished */
			req->response = XMS_RESPONSES[XMS_PAGE_COMPLETED];
			req->status = MHD_HTTP_OK;

			errno = 0;
			if (rename (XMS_TEMP_FILE, XMS_DEST_FILE) == 0) {
				warn (connection, "uploaded!");
			}
			else {
				/* XXX: fatal error?? */
				/* delete temp file, its not needed */
				remove (XMS_TEMP_FILE);
				warn (	connection,
					"uploaded with error: rename: %s",
					strerror (errno));
			}
		}

		/* job is done:
		 * process a new request ASAP, e.g. before conn. closing
		 */
		if (busy) {
			busy = false;
			resume_next ();
		}

		return MHD_queue_response
			(connection, req->status, req->response);
	}

	/* GET */
	return MHD_queue_response
		(connection, MHD_HTTP_OK, XMS_RESPONSES[XMS_PAGE_DEFAULT]);
}


static int
upload_post_chunk (void *con_cls,
              enum MHD_ValueKind kind,
              const char *key,
              const char *filename,
              const char *content_type,
              const char *transfer_encoding,
              const char *data,
              uint64_t off,
              size_t size)
{
	request_ctx *req = con_cls; /* we expect that it is OK */
	struct MHD_Response *response;
	unsigned int status;
	(void) kind;
	(void) content_type;
	(void) transfer_encoding;
	(void) off;
	(void) filename; /* we don't rely on value of `filename' */


	if (strncmp (key, "file", 5) != 0) {
		req->response = XMS_RESPONSES[XMS_PAGE_BAD_REQUEST];
		req->status = MHD_HTTP_BAD_REQUEST;

		return MHD_YES;
	}

	/* open file */
	if (req->fh == NULL) {
		req->fh = open_file (&response, &status);

		if (req->fh == NULL) {
			/* we failed to open a file */
			req->response = response;
			req->status = status;

			return MHD_NO;
		}
	}

	/* write the data */
	if (size > 0) {
		if (! fwrite (data, sizeof (char), size, req->fh)) {
			req->response = XMS_RESPONSES[XMS_PAGE_IO_ERROR];
			req->status = MHD_HTTP_INTERNAL_SERVER_ERROR;

			return MHD_NO;
		}
	}

	return MHD_YES;
}


static FILE *
open_file (struct MHD_Response **response, unsigned int *status)
{
	static FILE *fh;


	/* check if the file exists */
	fh = fopen (XMS_TEMP_FILE, "rb");

	if (fh == NULL) {
		/* try to create a new file */
		fh = fopen (XMS_TEMP_FILE, "ab");

		if (fh == NULL) {
			fprintf (stderr, "failed to open file `%s': %s\n",
				XMS_TEMP_FILE,
				strerror (errno));
			*response = XMS_RESPONSES[XMS_PAGE_IO_ERROR];
			*status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
	}
	else {
		/* file exists */
		fclose (fh);
		fh = NULL;
		*response = XMS_RESPONSES[XMS_PAGE_FILE_EXISTS];
		*status = MHD_HTTP_FORBIDDEN;
	}

	return fh;
}


extern void
request_completed_cb (	void *cls,
			struct MHD_Connection *connection,
			void **con_cls,
			enum MHD_RequestTerminationCode toe)
{
	request_ctx *req = *con_cls;
	(void) cls;
#if defined(_DEBUG)
	char *tdesc;
	const union MHD_ConnectionInfo *ci;
	char *ip_addr;
	in_port_t port;
#else
	(void) connection;
	(void) toe;
#endif


#if defined(_DEBUG)
	ci = MHD_get_connection_info (
		connection,
		MHD_CONNECTION_INFO_CLIENT_ADDRESS);

	if (ci != NULL) {
		struct sockaddr_in *addr_in = *(struct sockaddr_in **) ci;
		ip_addr = inet_ntoa (addr_in->sin_addr);
		port = addr_in->sin_port;
	}
	else {
		ip_addr = "<unknown>";
		port = 0;
	}

	switch (toe) {
	case MHD_REQUEST_TERMINATED_COMPLETED_OK:
		tdesc = "OK";
		break;
	case MHD_REQUEST_TERMINATED_WITH_ERROR:
		tdesc = "ERROR";
		break;
	case MHD_REQUEST_TERMINATED_TIMEOUT_REACHED:
		tdesc = "TIMED OUT";
		break;
	case MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN:
		tdesc = "SHUTDOWN";
		break;
	case MHD_REQUEST_TERMINATED_READ_ERROR:
		tdesc = "READ ERROR";
		break;
	case MHD_REQUEST_TERMINATED_CLIENT_ABORT:
		tdesc = "CLIENT ABORT";
		break;
	default:
		tdesc = "UNKNOWN";
		break;
	} /* switch (toe) { */

	debug ("* Connection %s port %d closed: %s\n",
		ip_addr, port, tdesc);
#endif

	if (req != NULL) {
		destroy_request_ctx (req);
		*con_cls = NULL;
	}
}


static void
destroy_request_ctx (request_ctx *req)
{
	if (req->pp != NULL)
		MHD_destroy_post_processor (req->pp);

	if (req->fh != NULL)
		fclose (req->fh);

	free (req);
}

extern void
init_server_data (void)
{
	char path[PATH_MAX];


#if defined(_WIN32)

#error not implemented yet

#else /* Unixes */
	snprintf (path, PATH_MAX - 1, "%s/" TEMP_FILENAME, XMS_STORAGE_DIR);
	XMS_TEMP_FILE = strdup (path);

	snprintf (path, PATH_MAX - 1, "%s/" DEST_FILENAME, XMS_STORAGE_DIR);
	XMS_DEST_FILE = strdup (path);
#endif
}


extern void
free_server_data (void)
{
	if (XMS_TEMP_FILE != NULL)
		free (XMS_TEMP_FILE);

	if (XMS_DEST_FILE != NULL)
		free (XMS_DEST_FILE);
}
