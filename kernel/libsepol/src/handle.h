#ifndef _SEPOL_INTERNAL_HANDLE_H_
#define _SEPOL_INTERNAL_HANDLE_H_

#include <sepol/handle.h>

struct sepol_handle {
	/* Error handling */
	int msg_level;
	const char *msg_channel;
	const char *msg_fname;
#ifdef __GNUC__
	__attribute__ ((format(printf, 3, 4)))
#endif
	void (*msg_callback) (void *varg,
			      sepol_handle_t * handle, const char *fmt, ...);
	void *msg_callback_arg;

	int disable_dontaudit;
	int expand_consume_base;
	int preserve_tunables;
};

#endif
