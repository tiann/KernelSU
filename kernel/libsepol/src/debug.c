// #include <stdarg.h>
// #include <stdlib.h>
// #include <stdio.h>
#include "handle.h"
#include "debug.h"

/* Deprecated */
struct sepol_handle sepol_compat_handle = {
	.msg_callback = sepol_msg_default_handler,
	.msg_callback_arg = NULL,
};

void sepol_debug(int on)
{
	sepol_compat_handle.msg_callback = (on) ?
	    sepol_msg_default_handler : NULL;
}

/* End deprecated */

int sepol_msg_get_level(sepol_handle_t * handle)
{
	return handle->msg_level;
}


const char *sepol_msg_get_channel(sepol_handle_t * handle)
{
	return handle->msg_channel;
}


const char *sepol_msg_get_fname(sepol_handle_t * handle)
{
	return handle->msg_fname;
}

#ifdef __GNUC__
    __attribute__ ((format(printf, 3, 4)))
#endif
void sepol_msg_default_handler(void *varg __attribute__ ((unused)),
				      sepol_handle_t * handle,
				      const char *fmt, ...)
{
#if 0
	FILE *stream = NULL;
	va_list ap;

	switch (sepol_msg_get_level(handle)) {

	case SEPOL_MSG_ERR:
	case SEPOL_MSG_WARN:
		stream = stderr;
		break;
	case SEPOL_MSG_INFO:
	default:
		stream = stdout;
		break;
	}

	fprintf(stream, "%s.%s: ",
		sepol_msg_get_channel(handle), sepol_msg_get_fname(handle));

	va_start(ap, fmt);
	vfprintf(stream, fmt, ap);
	va_end(ap);

	fprintf(stream, "\n");
#endif
}

extern void sepol_msg_set_callback(sepol_handle_t * handle,
#ifdef __GNUC__
				   __attribute__ ((format(printf, 3, 4)))
#endif
				   void (*msg_callback) (void *varg,
							 sepol_handle_t *
							 handle,
							 const char *fmt, ...),
				   void *msg_callback_arg)
{

	handle->msg_callback = msg_callback;
	handle->msg_callback_arg = msg_callback_arg;
}
