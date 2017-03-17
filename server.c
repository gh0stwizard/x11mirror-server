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


/* storage location, dirpath */
char *XMS_STORAGE_DIR = NULL;

/* allow only one uploader per a moment */
volatile bool busy = false;

/* From libmicrohttpd manual:
   maximum number of bytes to use for internal buffering (used only for
   the parsing, specifically the parsing of the keys). A tiny value (256-1024)
   should be sufficient; do NOT use a value smaller than 256; for good
   performance, use 32k or 64k (i.e. 65536).
*/
#define POSTBUFFERSIZE (32 * 1024)


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
open_file (	const char *filename,
		struct MHD_Response **response,
		unsigned int *status);

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
		req->filename = NULL;
		req->suspend_index = UINT_MAX;
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
		if (*upload_data_size == 0) {
			/* we can send a response only after reading all
			   headers & data. */
			return MHD_queue_response
				(connection, req->status, req->response);
		}
		else {
			/* do nothing and wait until headers & data */
			*upload_data_size = 0;

			return MHD_YES;
		}
	}

	if (req->type == POST) {
		if (busy && ! req->uploader) {
			/* no need to update upload_data_size, because
			 * overwise we have to store the first data
                         * somewhere and if we don't we will lost filename
                         * header.
                         */
			suspend_connection (connection, req);
			return MHD_YES;
		}

		if (*upload_data_size > 0) {
			/* uploading data */
			(void) MHD_post_process (
				req->pp,
				upload_data,
				*upload_data_size);

			/* we do this only once to stop spam "uploading..." */
			if (! req->uploader && req->filename != NULL) {
				/* seems to be everything is good */
				busy = req->uploader = true;
				warn (	connection, "uploading `%s'",
					req->filename);
			}

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

			char path[PATH_MAX];
			snprintf (path, PATH_MAX - 1,
				"%s/%s", XMS_STORAGE_DIR, req->filename);
			remove (path);

			warn (connection, "uploaded `%s'", req->filename);
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
	request_ctx *req = con_cls;
	(void) kind;
	(void) content_type;
	(void) transfer_encoding;
	(void) off;


	if (       (filename == NULL)
		|| (strlen (filename) == 0)
		|| (strlen (filename) >= 256)
		|| (strncmp (key, "file", 5) != 0))
	{
		req->response = XMS_RESPONSES[XMS_PAGE_BAD_REQUEST];
		req->status = MHD_HTTP_BAD_REQUEST;
		return MHD_YES;
	}

	if (req->fh == NULL) {
		struct MHD_Response *response;
		unsigned int status;

		req->fh = open_file (filename, &response, &status);

		if (req->fh != NULL) {
			req->filename = strdup (filename);
		}
		else {
			req->response = response;
			req->status = status;

			return MHD_YES;
		}
	}

	if (size > 0) {
		if (! fwrite (data, sizeof (char), size, req->fh)) {
			req->response = XMS_RESPONSES[XMS_PAGE_IO_ERROR];
			req->status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
	}

	return MHD_YES;
}


static FILE *
open_file (	const char *filename,
		struct MHD_Response **response,
		unsigned int *status)
{
	static FILE *fh;
	char path[PATH_MAX];
	snprintf (path, PATH_MAX - 1,
		"%s/%s", XMS_STORAGE_DIR, filename);

	/* check if the file exists */
	fh = fopen (path, "rb");

	if (fh == NULL) {
		/* try to create a new file */
		fh = fopen (path, "ab");

		if (fh == NULL) {
			fprintf (stderr, "failed to open file `%s': %s\n",
				filename,
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
	if (req->filename != NULL)
		free (req->filename);

	if (req->pp != NULL)
		MHD_destroy_post_processor (req->pp);

	if (req->fh != NULL)
		fclose (req->fh);

	free (req);
}
