// #include <stdlib.h>

#include "debug.h"
#include <sepol/policydb/policydb.h>
#include "policydb_internal.h"

/* Policy file interfaces. */

int sepol_policy_file_create(sepol_policy_file_t ** pf)
{
	*pf = calloc(1, sizeof(sepol_policy_file_t));
	if (!(*pf))
		return -1;
	return 0;
}

void sepol_policy_file_set_mem(sepol_policy_file_t * spf,
			       char *data, size_t len)
{
	struct policy_file *pf = &spf->pf;
	if (!len) {
		pf->type = PF_LEN;
		return;
	}
	pf->type = PF_USE_MEMORY;
	pf->data = data;
	pf->len = len;
	pf->size = len;
	return;
}

void sepol_policy_file_set_fp(sepol_policy_file_t * spf, FILE * fp)
{
	struct policy_file *pf = &spf->pf;
	pf->type = PF_USE_STDIO;
	pf->fp = fp;
	return;
}

int sepol_policy_file_get_len(sepol_policy_file_t * spf, size_t * len)
{
	struct policy_file *pf = &spf->pf;
	if (pf->type != PF_LEN)
		return -1;
	*len = pf->len;
	return 0;
}

void sepol_policy_file_set_handle(sepol_policy_file_t * pf,
				  sepol_handle_t * handle)
{
	pf->pf.handle = handle;
}

void sepol_policy_file_free(sepol_policy_file_t * pf)
{
	free(pf);
}

/* Policydb interfaces. */

int sepol_policydb_create(sepol_policydb_t ** sp)
{
	policydb_t *p;
	*sp = malloc(sizeof(sepol_policydb_t));
	if (!(*sp))
		return -1;
	p = &(*sp)->p;
	if (policydb_init(p)) {
		free(*sp);
		*sp = NULL;
		return -1;
	}
	return 0;
}


void sepol_policydb_free(sepol_policydb_t * p)
{
	if (!p)
		return;
	ksu_policydb_destroy(&p->p);
	free(p);
}


int sepol_policy_kern_vers_min(void)
{
	return POLICYDB_VERSION_MIN;
}

int sepol_policy_kern_vers_max(void)
{
	return POLICYDB_VERSION_MAX;
}

int sepol_policydb_set_typevers(sepol_policydb_t * sp, unsigned int type)
{
	struct policydb *p = &sp->p;
	switch (type) {
	case POLICY_KERN:
		p->policyvers = POLICYDB_VERSION_MAX;
		break;
	case POLICY_BASE:
	case POLICY_MOD:
		p->policyvers = MOD_POLICYDB_VERSION_MAX;
		break;
	default:
		return -1;
	}
	p->policy_type = type;
	return 0;
}

int sepol_policydb_set_vers(sepol_policydb_t * sp, unsigned int vers)
{
	struct policydb *p = &sp->p;
	switch (p->policy_type) {
	case POLICY_KERN:
		if (vers < POLICYDB_VERSION_MIN || vers > POLICYDB_VERSION_MAX)
			return -1;
		break;
	case POLICY_BASE:
	case POLICY_MOD:
		if (vers < MOD_POLICYDB_VERSION_MIN
		    || vers > MOD_POLICYDB_VERSION_MAX)
			return -1;
		break;
	default:
		return -1;
	}
	p->policyvers = vers;
	return 0;
}

int sepol_policydb_set_handle_unknown(sepol_policydb_t * sp,
				      unsigned int handle_unknown)
{
	struct policydb *p = &sp->p;

	switch (handle_unknown) {
	case SEPOL_DENY_UNKNOWN:
	case SEPOL_REJECT_UNKNOWN:
	case SEPOL_ALLOW_UNKNOWN:
		break;
	default:
		return -1;
	}

	p->handle_unknown = handle_unknown;		
	return 0;
}

int sepol_policydb_set_target_platform(sepol_policydb_t * sp,
				      int target_platform)
{
	struct policydb *p = &sp->p;

	switch (target_platform) {
	case SEPOL_TARGET_SELINUX:
	case SEPOL_TARGET_XEN:
		break;
	default:
		return -1;
	}

	p->target_platform = target_platform;		
	return 0;
}

int sepol_policydb_optimize(sepol_policydb_t * p)
{
	return policydb_optimize(&p->p);
}

int sepol_policydb_read(sepol_policydb_t * p, sepol_policy_file_t * pf)
{
	return ksu_policydb_read(&p->p, &pf->pf, 0);
}

int sepol_policydb_write(sepol_policydb_t * p, sepol_policy_file_t * pf)
{
	return ksu_policydb_write(&p->p, &pf->pf);
}

int sepol_policydb_from_image(sepol_handle_t * handle,
			      void *data, size_t len, sepol_policydb_t * p)
{
	return policydb_from_image(handle, data, len, &p->p);
}

int sepol_policydb_to_image(sepol_handle_t * handle,
			    sepol_policydb_t * p, void **newdata,
			    size_t * newlen)
{
	return policydb_to_image(handle, &p->p, newdata, newlen);
}

int sepol_policydb_mls_enabled(const sepol_policydb_t * p)
{

	return p->p.mls;
}

/* 
 * Enable compatibility mode for SELinux network checks iff
 * the packet class is not defined in the policy.
 */
#define PACKET_CLASS_NAME "packet"
int sepol_policydb_compat_net(const sepol_policydb_t * p)
{
	return (hashtab_search(p->p.p_classes.table, PACKET_CLASS_NAME) ==
		NULL);
}
