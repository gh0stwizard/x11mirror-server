#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#if (defined IM_VERSION) && IM_VERSION >= 7
	#include <MagickWand/MagickWand.h>
#else
	#include <wand/MagickWand.h>
#endif
#include "common.h"


#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#if defined(_MSC_VER) && _MSC_VER+0 <= 1800
/* Substitution is OK while return value is not used */
#define snprintf _snprintf
#endif

#ifndef IM_IN_FMT
#define IM_IN_FMT	"xwd:%s"
#endif
#ifndef IM_OUT_FMT
#define IM_OUT_FMT	"jpg:%s"
#endif


static void
error_handler ( const ExceptionType severity,
		const char *reason,
		const char *description)
{
	(void) severity;
	error ("imagemagick: %s (%s)\n", reason, description);
}


extern bool
convert (const char *in, const char *out)
{
	MagickBooleanType status = MagickFalse;
	ImageInfo *image_info;
	ExceptionInfo *exception;
	char im_in[PATH_MAX - 5];
	char im_out[PATH_MAX - 5];
	char *argv[] = {
		"x11mirror-server",
		NULL,
		NULL
	};

	snprintf (im_in, PATH_MAX - 5, IM_IN_FMT, in);
	snprintf (im_out, PATH_MAX - 5, IM_OUT_FMT, out);

	argv[1] = im_in;
	argv[2] = im_out;

	image_info = AcquireImageInfo ();
	exception = AcquireExceptionInfo ();

	if (image_info && exception) {
		/* XXX: do we need always set the error handler?? */
		(void) SetErrorHandler (error_handler);

		status = ConvertImageCommand (
			image_info,
			(sizeof (argv) / sizeof (argv[0])),
			argv, 
			(char **)NULL, exception);

		if (status == MagickFalse) {
			if (exception->severity != UndefinedException)
				CatchException (exception);
		}
	}
	else {
		die ("failed to initialize IM\n");
	}

	DestroyImageInfo (image_info);
	DestroyExceptionInfo (exception);

	return (status == MagickTrue) ? true : false;
}
