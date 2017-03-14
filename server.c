#include "mhd.h"
#include "common.h"
#include "suspend.h"
#include "warn.h"

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


/* allow only one uploader per a moment */
volatile bool busy = false;

/* we copy daemon handler because of suspend/resume + MHD_run () */
static struct MHD_Daemon *daemon;


/* From libmicrohttpd manual:
   maximum number of bytes to use for internal buffering (used only for
   the parsing, specifically the parsing of the keys). A tiny value (256-1024)
   should be sufficient; do NOT use a value smaller than 256; for good
   performance, use 32k or 64k (i.e. 65536).
*/
#define POSTBUFFERSIZE (32 * 1024)


enum request_type {
	GET 	= 0,
	POST	= 1
};


typedef struct _request_info {
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
} request_info;


enum {
	PAGE_DEFAULT = 0,
	PAGE_COMPLETED,
	PAGE_BAD_REQUEST,
	PAGE_FILE_EXISTS,
	PAGE_IO_ERROR,
	PAGE_MAX
};

static struct MHD_Response *responses[PAGE_MAX];
static const char * pages[PAGE_MAX];


#define DEFAULT_CONTENT_TYPE "text/html; charset=iso-8859-1"

#define _HEAD_TITLE "<head><title>x11mirror-server</title></head>"

#define _DEFAULT "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Hello!</h1>"\
"</body></html>"

#define _COMPLETED "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Upload completed.</h1>"\
"</body></html>"

#define _EXISTS "<html>" _HEAD_TITLE \
"<body>"\
"<h1>File exists.</h1>"\
"</body></html>"

#define _IO_ERROR "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Internal server error: I/O error.</h1>"\
"</body></html>"

#define _BAD_REQUEST "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Bad request.</h1>"\
"</body></html>"


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
destroy_request_info (request_info *req);


/* ------------------------------------------------------------------ */


extern int
accept_policy_cb (void *cls, const struct sockaddr *addr, socklen_t addrlen)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	(void) cls;
	(void) addrlen;


	/* ???: inet_ntop/WSAAddressToString */
	debug ("* Connection from %s port %d\n",
		inet_ntoa (addr_in->sin_addr),
		addr_in->sin_port);

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
	request_info *req = *con_cls;
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

		if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST)) {
			req->pp = MHD_create_post_processor (
					connection,
					POSTBUFFERSIZE,
					&upload_post_chunk,
					(void *) req);

			if (req->pp == NULL) {
				warn (	connection,
					"failed to create post processor");
				destroy_request_info (req);
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
		/* something went wrong */
		if (*upload_data_size == 0) {
			/* we can send a response only after reading all
			   headers & data. */
			return MHD_queue_response
				(connection, req->status, req->response);
		}

		/* do nothing and wait... */
		*upload_data_size = 0;

		return MHD_YES;
	}

	if (req->type == POST) {
		if (! req->uploader) {
			if (busy)
				req->suspend_index
					= suspend_connection (connection);
			else
				resume_connection (req->suspend_index, daemon);
		}

		if (*upload_data_size > 0) {
			/* uploading data */
			(void) MHD_post_process (
				req->pp,
				upload_data,
				*upload_data_size);

			if (req->filename != NULL)
				busy = req->uploader = true;

			*upload_data_size = 0;

			return MHD_YES;
		}

		/* upload has been finished */
		warn (connection, "uploaded `%s'", req->filename);

		if (busy && req->uploader) {
			busy = req->uploader = false;
			resume_all_connections (daemon);
		}

		if (req->fh != NULL) {
			fclose (req->fh);
			req->fh = NULL;
		}

		if (req->status == 0) {
			req->response = responses[PAGE_COMPLETED];
			req->status = MHD_HTTP_OK;
		}

		return MHD_queue_response
			(connection, req->status, req->response);
	}

	/* GET */
	return MHD_queue_response
		(connection, MHD_HTTP_OK, responses[PAGE_DEFAULT]);
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
	request_info *req = con_cls;
	(void) kind;
	(void) content_type;
	(void) transfer_encoding;
	(void) off;


	if (       (filename == NULL)
		|| (strlen (filename) == 0)
		|| (strlen (filename) >= 256)
		|| (strncmp (key, "file", 5) != 0))
	{
		req->response = responses[PAGE_BAD_REQUEST];
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
			req->response = responses[PAGE_IO_ERROR];
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


	/* check if the file exists */
	fh = fopen (filename, "rb");

	if (fh == NULL) {
		fh = fopen (filename, "ab");

		if (fh == NULL) {
			fprintf (stderr, "failed to open %s: %s\n",
				filename,
				strerror (errno));
			*response = responses[PAGE_IO_ERROR];
			*status = MHD_HTTP_INTERNAL_SERVER_ERROR;
		}
	}
	else {
		fclose (fh);
		fh = NULL;
		*response = responses[PAGE_FILE_EXISTS];
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
#if defined(_DEBUG)
	char *tdesc;
	const union MHD_ConnectionInfo *ci;
	char *ip_addr;
	in_port_t port;
#endif
	request_info *req = *con_cls;
	(void) cls;


	if (req != NULL) {
		destroy_request_info (req);
		*con_cls = NULL;
	}

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
		fprintf (stderr, "failed to get client info address\n");
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
}


extern void
init_mhd_responses (void)
{
	pages[PAGE_DEFAULT] = _DEFAULT;
	pages[PAGE_COMPLETED] = _COMPLETED;
	pages[PAGE_FILE_EXISTS] = _EXISTS;
	pages[PAGE_IO_ERROR] = _IO_ERROR;
	pages[PAGE_BAD_REQUEST] = _BAD_REQUEST;

	for (int i = 0; i < PAGE_MAX; i++) {
		responses[i] = MHD_create_response_from_buffer (
				strlen (pages[i]),
				(void *) pages[i],
				MHD_RESPMEM_PERSISTENT);

		if (responses[i] == NULL)
			die ("failed to create page #%d", i);

		MHD_add_response_header (
			responses[i],
			MHD_HTTP_HEADER_CONTENT_TYPE,
			DEFAULT_CONTENT_TYPE);
	}
}


extern void
free_mhd_responses (void)
{
	for (int i = 0; i < PAGE_MAX; i++)
		MHD_destroy_response (responses[i]);
}

static void
destroy_request_info (request_info *req)
{
	if (req->filename != NULL)
		free (req->filename);

	if (req->pp != NULL)
		MHD_destroy_post_processor (req->pp);

	if (req->fh != NULL)
		fclose (req->fh);

	free (req);
}

extern void
setup_daemon_handler (struct MHD_Daemon *d)
{
	if (d != NULL)
		daemon = d;
}

extern void
clear_daemon_handler (void)
{
	daemon = NULL;
}
