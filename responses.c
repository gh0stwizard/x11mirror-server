#include "responses.h"
#include "mhd.h"
#include "common.h"


#define DEFAULT_CONTENT_TYPE "text/html; charset=iso-8859-1"

#define _HEAD_TITLE "<head><title>x11mirror-server</title></head>"

#define _DEFAULT "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Hello!</h1>"\
"</body></html>\r\n"

#define _COMPLETED "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Upload completed.</h1>"\
"</body></html>\r\n"

#define _EXISTS "<html>" _HEAD_TITLE \
"<body>"\
"<h1>File exists.</h1>"\
"</body></html>\r\n"

#define _IO_ERROR "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Internal server error: I/O error.</h1>"\
"</body></html>\r\n"

#define _BAD_REQUEST "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Bad request.</h1>"\
"</body></html>\r\n"

#define _BAD_METHOD "<html>" _HEAD_TITLE \
"<body>"\
"<h1>Bad method.</h1>"\
"</body></html>\r\n"

#define _NOT_FOUND "<html>" _HEAD_TITLE \
"<body>"\
"<h1>File not found.</h1>"\
"</body></html>\r\n"


/* global definition */
struct MHD_Response *XMS_RESPONSES[XMS_PAGE_MAX];
const char * XMS_PAGES[XMS_PAGE_MAX];


extern void
init_mhd_responses (void)
{
	int i;


	XMS_PAGES[XMS_PAGE_DEFAULT] = _DEFAULT;
	XMS_PAGES[XMS_PAGE_COMPLETED] = _COMPLETED;
	XMS_PAGES[XMS_PAGE_FILE_EXISTS] = _EXISTS;
	XMS_PAGES[XMS_PAGE_IO_ERROR] = _IO_ERROR;
	XMS_PAGES[XMS_PAGE_BAD_REQUEST] = _BAD_REQUEST;
	XMS_PAGES[XMS_PAGE_BAD_METHOD] = _BAD_METHOD;
	XMS_PAGES[XMS_PAGE_NOT_FOUND] = _NOT_FOUND;

	for (i = 0; i < XMS_PAGE_MAX; i++) {
		XMS_RESPONSES[i] = MHD_create_response_from_buffer (
				strlen (XMS_PAGES[i]),
				(void *) XMS_PAGES[i],
				MHD_RESPMEM_PERSISTENT);

		if (XMS_RESPONSES[i] == NULL)
			die ("failed to create response #%d", i);

		if (MHD_NO == MHD_add_response_header (
			XMS_RESPONSES[i],
			MHD_HTTP_HEADER_CONTENT_TYPE,
			DEFAULT_CONTENT_TYPE))
		{
			die ("failed to add header to response #%d", i);
		}
	}
}


extern void
free_mhd_responses (void)
{
	int i;


	for (i = 0; i < XMS_PAGE_MAX; i++)
		MHD_destroy_response (XMS_RESPONSES[i]);
}
