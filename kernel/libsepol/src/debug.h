/*
 * Copyright (C) 2006 Tresys Technology, LLC
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _SEPOL_INTERNAL_DEBUG_H_
#define _SEPOL_INTERNAL_DEBUG_H_

// #include <stdio.h>
#include <sepol/debug.h>
#include "handle.h"
#include "kernel.h"

#define STATUS_SUCCESS 0
#define STATUS_ERR -1
#define STATUS_NODATA 1

/* FIXME: this needs to become a real function. Declaring variables
 * in a macro is _evil_ as it can shadow other variables in local scope.
 * The variable h has been renamed to _sepol_h to reduce this chance, but
 * it is still wrong.
 */
#define msg_write(handle_arg, level_arg,			   \
		  channel_arg, func_arg, ...) do {		   \
		sepol_handle_t *_sepol_h = (handle_arg) ?: &sepol_compat_handle; \
		if (_sepol_h->msg_callback) {			   \
			_sepol_h->msg_fname = func_arg;		   \
			_sepol_h->msg_channel = channel_arg;	   \
			_sepol_h->msg_level = level_arg;	   \
								   \
			_sepol_h->msg_callback(			   \
				_sepol_h->msg_callback_arg,	   \
				_sepol_h, __VA_ARGS__);		   \
		}                                                  \
	} while(0)

#define ERR(handle, ...) \
	msg_write(handle, SEPOL_MSG_ERR, "libsepol", \
	__FUNCTION__, __VA_ARGS__)

#define INFO(handle, ...) \
	msg_write(handle, SEPOL_MSG_INFO, "libsepol", \
	__FUNCTION__, __VA_ARGS__)

#ifdef WARN
#undef WARN
#endif
#define WARN(handle, ...) \
	msg_write(handle, SEPOL_MSG_WARN, "libsepol", \
	__FUNCTION__, __VA_ARGS__)

// #ifdef __GNUC__
// __attribute__ ((format(printf, 3, 4)))
// #endif
extern void sepol_msg_default_handler(void *varg,
					     sepol_handle_t * msg,
					     const char *fmt, ...);

extern struct sepol_handle sepol_compat_handle;

#endif
