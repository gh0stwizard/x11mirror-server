#include "mhd.h"
#include "common.h"


#ifdef _MSC_VER
#ifndef strcasecmp
#define strcasecmp(a,b) _stricmp((a),(b))
#endif /* !strcasecmp */
#else
#include <strings.h>
#endif /* _MSC_VER */


/* From libmicrohttpd manual:
   maximum number of bytes to use for internal buffering (used only for
   the parsing, specifically the parsing of the keys). A tiny value (256-1024)
   should be sufficient; do NOT use a value smaller than 256; for good
   performance, use 32k or 64k (i.e. 65536).
*/
#define POSTBUFFERSIZE (32 * 1024)


extern void
warn (struct MHD_Connection *connection, const char *fmt, ...);

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

	/* HTTP response body we will return, NULL if not yet known. */
	struct MHD_Response *response;

	/* HTTP status code we will return, 0 for undecided. */
	unsigned int status;
} request_info;


static struct MHD_Response *default_response;
static struct MHD_Response *complete_response;

static const char *default_page = "<html>"
"<head><title>x11mirror-server</title></head>"
"<body>"
"<h1>Hello!</h1>"
"</body></html>";

static const char *complete_page = "<html>"
"<head><title>x11mirror-server</title></head>"
"<body>"
"<h1>Upload completed.</h1>"
"</body></html>";

#define DEFAULT_CONTENT_TYPE "text/html; charset=iso-8859-1"


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
	request_info *req;
	(void) cls;
	(void) url;
	(void) version;


	if (NULL == *con_cls) {
		/* initialize our request information */
		req = malloc (sizeof (*req));

		if (req == NULL) {
			warn (connection, "malloc (req) failed");
			return MHD_NO;
		}

		req->status = 0; /* we are not finished yet */
		req->pp = NULL;
		req->fh = NULL;

		if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST)) {
			req->pp = MHD_create_post_processor (
					connection,
					POSTBUFFERSIZE,
					&upload_post_chunk,
					(void *) req);

			if (req->pp == NULL) {
				warn (	connection,
					"failed to create post processor");
				free (req);
				return MHD_NO;
			}

			req->type = POST;
		}
		else
			req->type = GET;

		*con_cls = (void *) req;
	}

	if (req->status != 0) {
		*upload_data_size = 0;
		return MHD_YES;
	}

	if (req->type == POST) {
		if (*upload_data_size > 0) {
			/* uploading data */
			MHD_post_process (
				req->pp,
				upload_data,
				*upload_data_size);
			*upload_data_size = 0;
			return MHD_YES;
		}
		else {
			/* upload has been finished */
			fclose (req->fh);
			req->fh = NULL;
		}

		if (req->status == 0) {
			req->response = complete_response;
			req->status = MHD_HTTP_OK;
		}

		return MHD_queue_response
			(connection, req->status, req->response);
	}
	else {
		/* GET */
		return MHD_queue_response
			(connection, MHD_HTTP_OK, default_response);
	}
}


static int
upload_post_chunk (void *coninfo_cls,
              enum MHD_ValueKind kind,
              const char *key,
              const char *filename,
              const char *content_type,
              const char *transfer_encoding,
              const char *data,
              uint64_t off,
              size_t size)
{
	return MHD_YES;
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
		if (req->pp != NULL)
			MHD_destroy_post_processor (req->pp);
		if (req->fh != NULL)
			fclose (req->fh);
		free (req);
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
warn (struct MHD_Connection *connection, const char *fmt, ...)
{
	va_list args;
	const union MHD_ConnectionInfo *ci;
	char *ip;
	in_port_t port = 0;


	ci = MHD_get_connection_info (
		connection,
		MHD_CONNECTION_INFO_CLIENT_ADDRESS);

	if (ci != NULL) {
		struct sockaddr_in *addr_in = *(struct sockaddr_in **) ci;
		ip = inet_ntoa (addr_in->sin_addr);
		port = addr_in->sin_port;
	}
	else
		ip = "<unknown>";

	va_start (args, fmt);
	fprintf (stderr, "! %s port %u: ", ip, port);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
	va_end (args);
}


extern void
init_mhd_responses (void)
{
	default_response = MHD_create_response_from_buffer (
				strlen (default_page),
				(void *) default_page,
				MHD_RESPMEM_PERSISTENT);

	if (!default_response)
		die ("failed to create default response");

	complete_response = MHD_create_response_from_buffer (
				strlen (complete_page),
				(void *) complete_page,
				MHD_RESPMEM_PERSISTENT);

	if (!complete_response)
		die ("failed to create complete response");

	MHD_add_response_header (
		default_response,
		MHD_HTTP_HEADER_CONTENT_TYPE,
		DEFAULT_CONTENT_TYPE);

	MHD_add_response_header (
		complete_response,
		MHD_HTTP_HEADER_CONTENT_TYPE,
		DEFAULT_CONTENT_TYPE);
}


extern void
free_mhd_responses (void)
{
	MHD_destroy_response (default_response);
	MHD_destroy_response (complete_response);
}
