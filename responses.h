#ifndef __RESPONSES_H
#define __RESPONSES_H


enum {
	XMS_PAGE_DEFAULT = 0,
	XMS_PAGE_COMPLETED,
	XMS_PAGE_BAD_REQUEST,
	XMS_PAGE_FILE_EXISTS,
	XMS_PAGE_IO_ERROR,
	XMS_PAGE_BAD_METHOD,
	XMS_PAGE_NOT_FOUND,
	XMS_PAGE_MAX
};
/* the values defined in responses.c */
extern struct MHD_Response *XMS_RESPONSES[];
extern const char *XMS_PAGES[];


extern void
init_mhd_responses (void);

extern void
free_mhd_responses (void);

#endif /* __RESPONSES_H */
