/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <cil/cil.h>
#include "cil_log.h"

static enum cil_log_level cil_log_level = CIL_ERR;

static void cil_default_log_handler(__attribute__((unused)) int lvl, const char *msg)
{
	fprintf(stderr, "%s", msg);
}

static void (*cil_log_handler)(int lvl, const char *msg) = &cil_default_log_handler;

void cil_set_log_handler(void (*handler)(int lvl, const char *msg))
{
	cil_log_handler = handler;
}

__attribute__ ((format (printf, 2, 0))) void cil_vlog(enum cil_log_level lvl, const char *msg, va_list args)
{
	if (cil_log_level >= lvl) {
		char buff[MAX_LOG_SIZE];
		int n = vsnprintf(buff, MAX_LOG_SIZE, msg, args);
		if (n > 0) {
			(*cil_log_handler)(cil_log_level, buff);
			if (n >= MAX_LOG_SIZE) {
				(*cil_log_handler)(cil_log_level, " <LOG MESSAGE TRUNCATED>");
			}
		}
	}
}

__attribute__ ((format (printf, 2, 3))) void cil_log(enum cil_log_level lvl, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    cil_vlog(lvl, msg, args);
    va_end(args);
}

void cil_set_log_level(enum cil_log_level lvl)
{
	cil_log_level = lvl;
}

enum cil_log_level cil_get_log_level(void)
{
	return cil_log_level;
}
