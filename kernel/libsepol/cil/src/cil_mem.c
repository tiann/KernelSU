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

#include "cil_log.h"
#include "cil_mem.h"

void *cil_malloc(size_t size)
{
	void *mem = malloc(size);
	if (mem == NULL){
		if (size == 0) {
			return NULL;
		}
		cil_log(CIL_ERR, "Failed to allocate memory\n");
		exit(1);
	}

	return mem;
}

void *cil_calloc(size_t num_elements, size_t element_size)
{
	void *mem = calloc(num_elements, element_size);
	if (mem == NULL){
		cil_log(CIL_ERR, "Failed to allocate memory\n");
		exit(1);
	}

	return mem;
}

void *cil_realloc(void *ptr, size_t size)
{
	void *mem = realloc(ptr, size);
	if (mem == NULL){
		if (size == 0) {
			return NULL;
		}
		cil_log(CIL_ERR, "Failed to allocate memory\n");
		exit(1);
	}

	return mem;
}


char *cil_strdup(const char *str)
{
	char *mem = NULL;

	if (str == NULL) {
		return NULL;
	}

	mem = strdup(str);
	if (mem == NULL) {
		cil_log(CIL_ERR, "Failed to allocate memory\n");
		exit(1);
	}

	return mem;
}

__attribute__ ((format (printf, 2, 3))) int cil_asprintf(char **strp, const char *fmt, ...)
{
	int rc;
	va_list ap;

	va_start(ap, fmt);
	rc = vasprintf(strp, fmt, ap);
	va_end(ap);

	if (rc == -1) {
		cil_log(CIL_ERR, "Failed to allocate memory\n");
		exit(1);
	}

	return rc;
}
