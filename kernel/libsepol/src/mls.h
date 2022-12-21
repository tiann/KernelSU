/* Author: Stephen Smalley, <sds@tycho.nsa.gov>
 * Updated: Trusted Computer Solutions, Inc. <dgoeddel@trustedcs.com>
 * 
 *      Support for enhanced MLS infrastructure.
 *
 * Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
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

#ifndef _SEPOL_MLS_INTERNAL_H_
#define _SEPOL_MLS_INTERNAL_H_

#include "policydb_internal.h"
#include <sepol/policydb/context.h>
#include "handle.h"

extern int ksu_mls_from_string(sepol_handle_t * handle,
			   const policydb_t * policydb,
			   const char *str, context_struct_t * mls);

extern int mls_to_string(sepol_handle_t * handle,
			 const policydb_t * policydb,
			 const context_struct_t * mls, char **str);

/* Deprecated */
extern int ksu_mls_compute_context_len(const policydb_t * policydb,
				   const context_struct_t * context);

/* Deprecated */
extern void ksu_mls_sid_to_context(const policydb_t * policydb,
			       const context_struct_t * context,
			       char **scontext);

/* Deprecated */
extern int ksu_mls_context_to_sid(const policydb_t * policydb,
			      char oldc,
			      char **scontext, context_struct_t * context);

extern int ksu_mls_context_isvalid(const policydb_t * p,
			       const context_struct_t * c);

extern int ksu_mls_convert_context(policydb_t * oldp,
			       policydb_t * newp, context_struct_t * context);

extern int ksu_mls_compute_sid(policydb_t * policydb,
			   const context_struct_t * scontext,
			   const context_struct_t * tcontext,
			   sepol_security_class_t tclass,
			   uint32_t specified, context_struct_t * newcontext);

extern int ksu_mls_setup_user_range(context_struct_t * fromcon, user_datum_t * user,
				context_struct_t * usercon, int mls);

#endif
