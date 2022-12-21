#ifndef _SEPOL_INTERNAL_CONTEXT_H_
#define _SEPOL_INTERNAL_CONTEXT_H_

#include <stddef.h>
#include "context_internal.h"
#include <sepol/policydb/context.h>
#include <sepol/policydb/policydb.h>
#include <sepol/handle.h>

/* Create a context structure from high level representation */
extern int context_from_record(sepol_handle_t * handle,
			       const policydb_t * policydb,
			       context_struct_t ** cptr,
			       const sepol_context_t * data);

extern int context_to_record(sepol_handle_t * handle,
			     const policydb_t * policydb,
			     const context_struct_t * context,
			     sepol_context_t ** record);

/* Create a context structure from string representation */
extern int context_from_string(sepol_handle_t * handle,
			       const policydb_t * policydb,
			       context_struct_t ** cptr,
			       const char *con_str, size_t con_str_len);

/* Check if the provided context is valid for this policy */
extern int context_is_valid(const policydb_t * policydb,
			    const context_struct_t * context);

/* Extract the context as string */
extern int context_to_string(sepol_handle_t * handle,
			     const policydb_t * policydb,
			     const context_struct_t * context,
			     char **result, size_t * result_len);

#endif
