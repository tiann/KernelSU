/* Author: Karl MacMillan <kmacmillan@tresys.com>
 *         Jason Tang     <jtang@tresys.com>
 *         Chris PeBenito <cpebenito@tresys.com>
 *
 * Copyright (C) 2004-2005 Tresys Technology, LLC
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

#include "policydb_internal.h"
#include "module_internal.h"
#include <sepol/policydb/link.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/module.h>
#include "debug.h"
#include "private.h"

// #include <stdio.h>
// #include <stdlib.h>
#include <linux/limits.h>
// #include <inttypes.h>

#define SEPOL_PACKAGE_SECTION_FC 0xf97cff90
#define SEPOL_PACKAGE_SECTION_SEUSER 0x97cff91
#define SEPOL_PACKAGE_SECTION_USER_EXTRA 0x97cff92
#define SEPOL_PACKAGE_SECTION_NETFILTER 0x97cff93

static int policy_file_seek(struct policy_file *fp, size_t offset)
{
	switch (fp->type) {
	case PF_USE_STDIO:
#if 0
		if (offset > LONG_MAX) {
			errno = EFAULT;
			return -1;
		}
		return fseek(fp->fp, (long)offset, SEEK_SET);
#else
	return -1;
#endif
	case PF_USE_MEMORY:
		if (offset > fp->size) {
			// errno = EFAULT;
			return -1;
		}
		fp->data -= fp->size - fp->len;
		fp->data += offset;
		fp->len = fp->size - offset;
		return 0;
	default:
		return 0;
	}
}

static int policy_file_length(struct policy_file *fp, size_t *out)
{
	// long prev_offset, end_offset;
	// int rc;
	switch (fp->type) {
	case PF_USE_STDIO:
#if 0
		prev_offset = ftell(fp->fp);
		if (prev_offset < 0)
			return prev_offset;
		rc = fseek(fp->fp, 0L, SEEK_END);
		if (rc < 0)
			return rc;
		end_offset = ftell(fp->fp);
		if (end_offset < 0)
			return end_offset;
		rc = fseek(fp->fp, prev_offset, SEEK_SET);
		if (rc < 0)
			return rc;
		*out = end_offset;
		break;
#else
	return -1;
#endif
	case PF_USE_MEMORY:
		*out = fp->size;
		break;
	default:
		*out = 0;
		break;
	}
	return 0;
}

static int module_package_init(sepol_module_package_t * p)
{
	memset(p, 0, sizeof(sepol_module_package_t));
	if (sepol_policydb_create(&p->policy))
		return -1;

	p->version = 1;
	return 0;
}

static int set_char(char **field, char *data, size_t len)
{
	if (*field) {
		free(*field);
		*field = NULL;
	}
	if (len) {
		*field = malloc(len);
		if (!*field)
			return -1;
		memcpy(*field, data, len);
	}
	return 0;
}

int sepol_module_package_create(sepol_module_package_t ** p)
{
	int rc;

	*p = calloc(1, sizeof(sepol_module_package_t));
	if (!(*p))
		return -1;

	rc = module_package_init(*p);
	if (rc < 0) {
		free(*p);
		*p = NULL;
	}

	return rc;
}


/* Deallocates all memory associated with a module package, including
 * the pointer itself.  Does nothing if p is NULL.
 */
void sepol_module_package_free(sepol_module_package_t * p)
{
	if (p == NULL)
		return;

	sepol_policydb_free(p->policy);
	free(p->file_contexts);
	free(p->seusers);
	free(p->user_extra);
	free(p->netfilter_contexts);
	free(p);
}


char *sepol_module_package_get_file_contexts(sepol_module_package_t * p)
{
	return p->file_contexts;
}

size_t sepol_module_package_get_file_contexts_len(sepol_module_package_t * p)
{
	return p->file_contexts_len;
}

char *sepol_module_package_get_seusers(sepol_module_package_t * p)
{
	return p->seusers;
}

size_t sepol_module_package_get_seusers_len(sepol_module_package_t * p)
{
	return p->seusers_len;
}

char *sepol_module_package_get_user_extra(sepol_module_package_t * p)
{
	return p->user_extra;
}

size_t sepol_module_package_get_user_extra_len(sepol_module_package_t * p)
{
	return p->user_extra_len;
}

char *sepol_module_package_get_netfilter_contexts(sepol_module_package_t * p)
{
	return p->netfilter_contexts;
}

size_t sepol_module_package_get_netfilter_contexts_len(sepol_module_package_t *
						       p)
{
	return p->netfilter_contexts_len;
}

int sepol_module_package_set_file_contexts(sepol_module_package_t * p,
					   char *data, size_t len)
{
	if (set_char(&p->file_contexts, data, len))
		return -1;

	p->file_contexts_len = len;
	return 0;
}

int sepol_module_package_set_seusers(sepol_module_package_t * p,
				     char *data, size_t len)
{
	if (set_char(&p->seusers, data, len))
		return -1;

	p->seusers_len = len;
	return 0;
}

int sepol_module_package_set_user_extra(sepol_module_package_t * p,
					char *data, size_t len)
{
	if (set_char(&p->user_extra, data, len))
		return -1;

	p->user_extra_len = len;
	return 0;
}

int sepol_module_package_set_netfilter_contexts(sepol_module_package_t * p,
						char *data, size_t len)
{
	if (set_char(&p->netfilter_contexts, data, len))
		return -1;

	p->netfilter_contexts_len = len;
	return 0;
}

sepol_policydb_t *sepol_module_package_get_policy(sepol_module_package_t * p)
{
	return p->policy;
}

/* Append each of the file contexts from each module to the base
 * policy's file context.  'base_context' will be reallocated to a
 * larger size (and thus it is an in/out reference
 * variable). 'base_fc_len' is the length of base's file context; it
 * too is a reference variable.  Return 0 on success, -1 if out of
 * memory. */
static int link_file_contexts(sepol_module_package_t * base,
			      sepol_module_package_t ** modules,
			      int num_modules)
{
	size_t fc_len;
	int i;
	char *s;

	fc_len = base->file_contexts_len;
	for (i = 0; i < num_modules; i++) {
		fc_len += modules[i]->file_contexts_len;
	}

	if ((s = (char *)realloc(base->file_contexts, fc_len)) == NULL) {
		return -1;
	}
	base->file_contexts = s;
	for (i = 0; i < num_modules; i++) {
		memcpy(base->file_contexts + base->file_contexts_len,
		       modules[i]->file_contexts,
		       modules[i]->file_contexts_len);
		base->file_contexts_len += modules[i]->file_contexts_len;
	}
	return 0;
}

/* Append each of the netfilter contexts from each module to the base
 * policy's netfilter context.  'base_context' will be reallocated to a
 * larger size (and thus it is an in/out reference
 * variable). 'base_nc_len' is the length of base's netfilter contexts; it
 * too is a reference variable.  Return 0 on success, -1 if out of
 * memory. */
static int link_netfilter_contexts(sepol_module_package_t * base,
				   sepol_module_package_t ** modules,
				   int num_modules)
{
	size_t base_nc_len;
	int i;
	char *base_context;

	base_nc_len = base->netfilter_contexts_len;
	for (i = 0; i < num_modules; i++) {
		base_nc_len += modules[i]->netfilter_contexts_len;
	}

	if ((base_context =
	     (char *)realloc(base->netfilter_contexts, base_nc_len)) == NULL) {
		return -1;
	}
	base->netfilter_contexts = base_context;
	for (i = 0; i < num_modules; i++) {
		if (modules[i]->netfilter_contexts_len > 0) {
			memcpy(base->netfilter_contexts + base->netfilter_contexts_len,
			       modules[i]->netfilter_contexts,
			       modules[i]->netfilter_contexts_len);
			base->netfilter_contexts_len +=
			    modules[i]->netfilter_contexts_len;
		}

	}
	return 0;
}

/* Links the module packages into the base.  Returns 0 on success, -1
 * if a requirement was not met, or -2 for all other errors. */
int sepol_link_packages(sepol_handle_t * handle,
			sepol_module_package_t * base,
			sepol_module_package_t ** modules, int num_modules,
			int verbose)
{
	policydb_t **mod_pols = NULL;
	int i, retval;

	if ((mod_pols = calloc(num_modules, sizeof(*mod_pols))) == NULL) {
		ERR(handle, "Out of memory!");
		return -2;
	}
	for (i = 0; i < num_modules; i++) {
		mod_pols[i] = &modules[i]->policy->p;
	}

	retval = link_modules(handle, &base->policy->p, mod_pols, num_modules,
			      verbose);
	free(mod_pols);
	if (retval == -3) {
		return -1;
	} else if (retval < 0) {
		return -2;
	}

	if (link_file_contexts(base, modules, num_modules) == -1) {
		ERR(handle, "Out of memory!");
		return -2;
	}

	if (link_netfilter_contexts(base, modules, num_modules) == -1) {
		ERR(handle, "Out of memory!");
		return -2;
	}

	return 0;
}

// http://aospxref.com/android-8.1.0_r81/xref/bionic/libc/include/stdio.h?fi=BUFSIZ#100
#define BUFSIZ 1024
/* buf must be large enough - no checks are performed */
#define _read_helper_bufsize BUFSIZ
static int read_helper(char *buf, struct policy_file *file, uint32_t bytes)
{
	uint32_t offset, nel, read_len;
	int rc;

	offset = 0;
	nel = bytes;

	while (nel) {
		if (nel < _read_helper_bufsize)
			read_len = nel;
		else
			read_len = _read_helper_bufsize;
		rc = next_entry(&buf[offset], file, read_len);
		if (rc < 0)
			return -1;
		offset += read_len;
		nel -= read_len;
	}
	return 0;
}

#define MAXSECTIONS 100

/* Get the section offsets from a package file, offsets will be malloc'd to
 * the appropriate size and the caller must free() them */
static int module_package_read_offsets(sepol_module_package_t * mod,
				       struct policy_file *file,
				       size_t ** offsets, uint32_t * sections)
{
	uint32_t *buf = NULL, nsec;
	unsigned i;
	size_t *off = NULL;
	int rc;

	buf = malloc(sizeof(uint32_t)*3);
	if (!buf) {
		ERR(file->handle, "out of memory");
		goto err;
	}
	  
	rc = next_entry(buf, file, sizeof(uint32_t) * 3);
	if (rc < 0) {
		ERR(file->handle, "module package header truncated");
		goto err;
	}
	if (le32_to_cpu(buf[0]) != SEPOL_MODULE_PACKAGE_MAGIC) {
		ERR(file->handle,
		    "wrong magic number for module package:  expected %#08x, got %#08x",
		    SEPOL_MODULE_PACKAGE_MAGIC, le32_to_cpu(buf[0]));
		goto err;
	}

	mod->version = le32_to_cpu(buf[1]);
	nsec = *sections = le32_to_cpu(buf[2]);

	if (nsec > MAXSECTIONS) {
		ERR(file->handle, "too many sections (%u) in module package",
		    nsec);
		goto err;
	}

	off = (size_t *) calloc(nsec + 1, sizeof(size_t));
	if (!off) {
		ERR(file->handle, "out of memory");
		goto err;
	}

	free(buf);
	buf = calloc(nsec, sizeof(uint32_t));
	if (!buf) {
		ERR(file->handle, "out of memory");
		goto err;
	}
	rc = next_entry(buf, file, sizeof(uint32_t) * nsec);
	if (rc < 0) {
		ERR(file->handle, "module package offset array truncated");
		goto err;
	}

	for (i = 0; i < nsec; i++) {
		off[i] = le32_to_cpu(buf[i]);
		if (i && off[i] < off[i - 1]) {
			ERR(file->handle, "offsets are not increasing (at %u, "
			    "offset %zu -> %zu", i, off[i - 1],
			    off[i]);
			goto err;
		}
	}

	rc = policy_file_length(file, &off[nsec]);
	if (rc < 0)
		goto err;

	if (nsec && off[nsec] < off[nsec-1]) {
		ERR(file->handle, "offset greater than file size (at %u, "
		    "offset %zu -> %zu", nsec, off[nsec - 1],
		    off[nsec]);
		goto err;
	}
	*offsets = off;
	free(buf);
	return 0;

err:
	free(buf);
	free(off);
	return -1;
}

/* Flags for which sections have been seen during parsing of module package. */
#define SEEN_MOD 1
#define SEEN_FC  2
#define SEEN_SEUSER 4
#define SEEN_USER_EXTRA 8
#define SEEN_NETFILTER 16

int sepol_module_package_read(sepol_module_package_t * mod,
			      struct sepol_policy_file *spf, int verbose)
{
	struct policy_file *file = &spf->pf;
	uint32_t buf[1], nsec;
	size_t *offsets, len;
	int rc;
	unsigned i, seen = 0;

	if (module_package_read_offsets(mod, file, &offsets, &nsec))
		return -1;

	/* we know the section offsets, seek to them and read in the data */

	for (i = 0; i < nsec; i++) {

		if (policy_file_seek(file, offsets[i])) {
			ERR(file->handle, "error seeking to offset %zu for "
			    "module package section %u", offsets[i], i);
			goto cleanup;
		}

		len = offsets[i + 1] - offsets[i];

		if (len < sizeof(uint32_t)) {
			ERR(file->handle, "module package section %u "
			    "has too small length %zu", i, len);
			goto cleanup;
		}

		/* read the magic number, so that we know which function to call */
		rc = next_entry(buf, file, sizeof(uint32_t));
		if (rc < 0) {
			ERR(file->handle,
			    "module package section %u truncated, lacks magic number",
			    i);
			goto cleanup;
		}

		switch (le32_to_cpu(buf[0])) {
		case SEPOL_PACKAGE_SECTION_FC:
			if (seen & SEEN_FC) {
				ERR(file->handle,
				    "found multiple file contexts sections in module package (at section %u)",
				    i);
				goto cleanup;
			}

			mod->file_contexts_len = len - sizeof(uint32_t);
			mod->file_contexts =
			    (char *)malloc(mod->file_contexts_len);
			if (!mod->file_contexts) {
				ERR(file->handle, "out of memory");
				goto cleanup;
			}
			if (read_helper
			    (mod->file_contexts, file,
			     mod->file_contexts_len)) {
				ERR(file->handle,
				    "invalid file contexts section at section %u",
				    i);
				free(mod->file_contexts);
				mod->file_contexts = NULL;
				goto cleanup;
			}
			seen |= SEEN_FC;
			break;
		case SEPOL_PACKAGE_SECTION_SEUSER:
			if (seen & SEEN_SEUSER) {
				ERR(file->handle,
				    "found multiple seuser sections in module package (at section %u)",
				    i);
				goto cleanup;
			}

			mod->seusers_len = len - sizeof(uint32_t);
			mod->seusers = (char *)malloc(mod->seusers_len);
			if (!mod->seusers) {
				ERR(file->handle, "out of memory");
				goto cleanup;
			}
			if (read_helper(mod->seusers, file, mod->seusers_len)) {
				ERR(file->handle,
				    "invalid seuser section at section %u", i);
				free(mod->seusers);
				mod->seusers = NULL;
				goto cleanup;
			}
			seen |= SEEN_SEUSER;
			break;
		case SEPOL_PACKAGE_SECTION_USER_EXTRA:
			if (seen & SEEN_USER_EXTRA) {
				ERR(file->handle,
				    "found multiple user_extra sections in module package (at section %u)",
				    i);
				goto cleanup;
			}

			mod->user_extra_len = len - sizeof(uint32_t);
			mod->user_extra = (char *)malloc(mod->user_extra_len);
			if (!mod->user_extra) {
				ERR(file->handle, "out of memory");
				goto cleanup;
			}
			if (read_helper
			    (mod->user_extra, file, mod->user_extra_len)) {
				ERR(file->handle,
				    "invalid user_extra section at section %u",
				    i);
				free(mod->user_extra);
				mod->user_extra = NULL;
				goto cleanup;
			}
			seen |= SEEN_USER_EXTRA;
			break;
		case SEPOL_PACKAGE_SECTION_NETFILTER:
			if (seen & SEEN_NETFILTER) {
				ERR(file->handle,
				    "found multiple netfilter contexts sections in module package (at section %u)",
				    i);
				goto cleanup;
			}

			mod->netfilter_contexts_len = len - sizeof(uint32_t);
			mod->netfilter_contexts =
			    (char *)malloc(mod->netfilter_contexts_len);
			if (!mod->netfilter_contexts) {
				ERR(file->handle, "out of memory");
				goto cleanup;
			}
			if (read_helper
			    (mod->netfilter_contexts, file,
			     mod->netfilter_contexts_len)) {
				ERR(file->handle,
				    "invalid netfilter contexts section at section %u",
				    i);
				free(mod->netfilter_contexts);
				mod->netfilter_contexts = NULL;
				goto cleanup;
			}
			seen |= SEEN_NETFILTER;
			break;
		case POLICYDB_MOD_MAGIC:
			if (seen & SEEN_MOD) {
				ERR(file->handle,
				    "found multiple module sections in module package (at section %u)",
				    i);
				goto cleanup;
			}

			/* seek back to where the magic number was */
			if (policy_file_seek(file, offsets[i]))
				goto cleanup;

			rc = ksu_policydb_read(&mod->policy->p, file, verbose);
			if (rc < 0) {
				ERR(file->handle,
				    "invalid module in module package (at section %u)",
				    i);
				goto cleanup;
			}
			seen |= SEEN_MOD;
			break;
		default:
			/* unknown section, ignore */
			ERR(file->handle,
			    "unknown magic number at section %u, offset: %zx, number: %x ",
			    i, offsets[i], le32_to_cpu(buf[0]));
			break;
		}
	}

	if ((seen & SEEN_MOD) == 0) {
		ERR(file->handle, "missing module in module package");
		goto cleanup;
	}

	free(offsets);
	return 0;

      cleanup:
	free(offsets);
	return -1;
}

int sepol_module_package_info(struct sepol_policy_file *spf, int *type,
			      char **name, char **version)
{
	struct policy_file *file = &spf->pf;
	sepol_module_package_t *mod = NULL;
	uint32_t buf[5], len, nsec;
	size_t *offsets = NULL;
	unsigned i, seen = 0;
	char *id;
	int rc;

	if (sepol_module_package_create(&mod))
		return -1;

	if (module_package_read_offsets(mod, file, &offsets, &nsec)) {
		goto cleanup;
	}

	for (i = 0; i < nsec; i++) {

		if (policy_file_seek(file, offsets[i])) {
			ERR(file->handle, "error seeking to offset "
			    "%zu for module package section %u", offsets[i], i);
			goto cleanup;
		}

		len = offsets[i + 1] - offsets[i];

		if (len < sizeof(uint32_t)) {
			ERR(file->handle,
			    "module package section %u has too small length %u",
			    i, len);
			goto cleanup;
		}

		/* read the magic number, so that we know which function to call */
		rc = next_entry(buf, file, sizeof(uint32_t) * 2);
		if (rc < 0) {
			ERR(file->handle,
			    "module package section %u truncated, lacks magic number",
			    i);
			goto cleanup;
		}

		switch (le32_to_cpu(buf[0])) {
		case SEPOL_PACKAGE_SECTION_FC:
			/* skip file contexts */
			if (seen & SEEN_FC) {
				ERR(file->handle,
				    "found multiple file contexts sections in module package (at section %u)",
				    i);
				goto cleanup;
			}
			seen |= SEEN_FC;
			break;
		case SEPOL_PACKAGE_SECTION_SEUSER:
			/* skip seuser */
			if (seen & SEEN_SEUSER) {
				ERR(file->handle,
				    "found seuser sections in module package (at section %u)",
				    i);
				goto cleanup;
			}
			seen |= SEEN_SEUSER;
			break;
		case SEPOL_PACKAGE_SECTION_USER_EXTRA:
			/* skip user_extra */
			if (seen & SEEN_USER_EXTRA) {
				ERR(file->handle,
				    "found user_extra sections in module package (at section %u)",
				    i);
				goto cleanup;
			}
			seen |= SEEN_USER_EXTRA;
			break;
		case SEPOL_PACKAGE_SECTION_NETFILTER:
			/* skip netfilter contexts */
			if (seen & SEEN_NETFILTER) {
				ERR(file->handle,
				    "found multiple netfilter contexts sections in module package (at section %u)",
				    i);
				goto cleanup;
			}
			seen |= SEEN_NETFILTER;
			break;
		case POLICYDB_MOD_MAGIC:
			if (seen & SEEN_MOD) {
				ERR(file->handle,
				    "found multiple module sections in module package (at section %u)",
				    i);
				goto cleanup;
			}
			len = le32_to_cpu(buf[1]);
			if (len != strlen(POLICYDB_MOD_STRING)) {
				ERR(file->handle,
				    "module string length is wrong (at section %u)",
				    i);
				goto cleanup;
			}

			/* skip id */
			id = malloc(len + 1);
			if (!id) {
				ERR(file->handle,
				    "out of memory (at section %u)",
				    i);
				goto cleanup;				
			}
			rc = next_entry(id, file, len);
			free(id);
			if (rc < 0) {
				ERR(file->handle,
				    "cannot get module string (at section %u)",
				    i);
				goto cleanup;
			}
			
			rc = next_entry(buf, file, sizeof(uint32_t) * 5);
			if (rc < 0) {
				ERR(file->handle,
				    "cannot get module header (at section %u)",
				    i);
				goto cleanup;
			}

			*type = le32_to_cpu(buf[0]);
			/* if base - we're done */
			if (*type == POLICY_BASE) {
				*name = NULL;
				*version = NULL;
				seen |= SEEN_MOD;
				break;
			} else if (*type != POLICY_MOD) {
				ERR(file->handle,
				    "module has invalid type %d (at section %u)",
				    *type, i);
				goto cleanup;
			}

			/* read the name and version */
			rc = next_entry(buf, file, sizeof(uint32_t));
			if (rc < 0) {
				ERR(file->handle,
				    "cannot get module name len (at section %u)",
				    i);
				goto cleanup;
			}

			len = le32_to_cpu(buf[0]);
			if (str_read(name, file, len)) {
				ERR(file->handle,
				    "cannot read module name (at section %u): %m",
				    i);
				goto cleanup;
			}

			rc = next_entry(buf, file, sizeof(uint32_t));
			if (rc < 0) {
				ERR(file->handle,
				    "cannot get module version len (at section %u)",
				    i);
				goto cleanup;
			}
			len = le32_to_cpu(buf[0]);
			if (str_read(version, file, len)) {
				ERR(file->handle,
				    "cannot read module version (at section %u): %m",
				i);
				goto cleanup;
			}
			seen |= SEEN_MOD;
			break;
		default:
			break;
		}

	}

	if ((seen & SEEN_MOD) == 0) {
		ERR(file->handle, "missing module in module package");
		goto cleanup;
	}

	sepol_module_package_free(mod);
	free(offsets);
	return 0;

      cleanup:
	sepol_module_package_free(mod);
	free(offsets);
	return -1;
}

static int write_helper(char *data, size_t len, struct policy_file *file)
{
	int idx = 0;
	size_t len2;

	while (len) {
		if (len > BUFSIZ)
			len2 = BUFSIZ;
		else
			len2 = len;

		if (put_entry(&data[idx], 1, len2, file) != len2) {
			return -1;
		}
		len -= len2;
		idx += len2;
	}
	return 0;
}

int sepol_module_package_write(sepol_module_package_t * p,
			       struct sepol_policy_file *spf)
{
	struct policy_file *file = &spf->pf;
	policy_file_t polfile;
	uint32_t buf[5], offsets[5], len, nsec = 0;
	int i;

	if (p->policy) {
		/* compute policy length */
		policy_file_init(&polfile);
		polfile.type = PF_LEN;
		polfile.handle = file->handle;
		if (ksu_policydb_write(&p->policy->p, &polfile))
			return -1;
		len = polfile.len;
		if (!polfile.len)
			return -1;
		nsec++;

	} else {
		/* We don't support writing a package without a module at this point */
		return -1;
	}

	/* seusers and user_extra only supported in base at the moment */
	if ((p->seusers || p->user_extra)
	    && (p->policy->p.policy_type != SEPOL_POLICY_BASE)) {
		ERR(file->handle,
		    "seuser and user_extra sections only supported in base");
		return -1;
	}

	if (p->file_contexts)
		nsec++;

	if (p->seusers)
		nsec++;

	if (p->user_extra)
		nsec++;

	if (p->netfilter_contexts)
		nsec++;

	buf[0] = cpu_to_le32(SEPOL_MODULE_PACKAGE_MAGIC);
	buf[1] = cpu_to_le32(p->version);
	buf[2] = cpu_to_le32(nsec);
	if (put_entry(buf, sizeof(uint32_t), 3, file) != 3)
		return -1;

	/* calculate offsets */
	offsets[0] = (nsec + 3) * sizeof(uint32_t);
	buf[0] = cpu_to_le32(offsets[0]);

	i = 1;
	if (p->file_contexts) {
		offsets[i] = offsets[i - 1] + len;
		buf[i] = cpu_to_le32(offsets[i]);
		/* add a uint32_t to compensate for the magic number */
		len = p->file_contexts_len + sizeof(uint32_t);
		i++;
	}
	if (p->seusers) {
		offsets[i] = offsets[i - 1] + len;
		buf[i] = cpu_to_le32(offsets[i]);
		len = p->seusers_len + sizeof(uint32_t);
		i++;
	}
	if (p->user_extra) {
		offsets[i] = offsets[i - 1] + len;
		buf[i] = cpu_to_le32(offsets[i]);
		len = p->user_extra_len + sizeof(uint32_t);
		i++;
	}
	if (p->netfilter_contexts) {
		offsets[i] = offsets[i - 1] + len;
		buf[i] = cpu_to_le32(offsets[i]);
		len = p->netfilter_contexts_len + sizeof(uint32_t);
		i++;
	}
	if (put_entry(buf, sizeof(uint32_t), nsec, file) != nsec)
		return -1;

	/* write sections */

	if (ksu_policydb_write(&p->policy->p, file))
		return -1;

	if (p->file_contexts) {
		buf[0] = cpu_to_le32(SEPOL_PACKAGE_SECTION_FC);
		if (put_entry(buf, sizeof(uint32_t), 1, file) != 1)
			return -1;
		if (write_helper(p->file_contexts, p->file_contexts_len, file))
			return -1;
	}
	if (p->seusers) {
		buf[0] = cpu_to_le32(SEPOL_PACKAGE_SECTION_SEUSER);
		if (put_entry(buf, sizeof(uint32_t), 1, file) != 1)
			return -1;
		if (write_helper(p->seusers, p->seusers_len, file))
			return -1;

	}
	if (p->user_extra) {
		buf[0] = cpu_to_le32(SEPOL_PACKAGE_SECTION_USER_EXTRA);
		if (put_entry(buf, sizeof(uint32_t), 1, file) != 1)
			return -1;
		if (write_helper(p->user_extra, p->user_extra_len, file))
			return -1;
	}
	if (p->netfilter_contexts) {
		buf[0] = cpu_to_le32(SEPOL_PACKAGE_SECTION_NETFILTER);
		if (put_entry(buf, sizeof(uint32_t), 1, file) != 1)
			return -1;
		if (write_helper
		    (p->netfilter_contexts, p->netfilter_contexts_len, file))
			return -1;
	}
	return 0;
}

int sepol_link_modules(sepol_handle_t * handle,
		       sepol_policydb_t * base,
		       sepol_policydb_t ** modules, size_t len, int verbose)
{
	return link_modules(handle, &base->p, (policydb_t **) modules, len,
			    verbose);
}

int sepol_expand_module(sepol_handle_t * handle,
			sepol_policydb_t * base,
			sepol_policydb_t * out, int verbose, int check)
{
	return expand_module(handle, &base->p, &out->p, verbose, check);
}
