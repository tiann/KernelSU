/*
 * Policy capability support functions
 */

#include <linux/string.h>
#include <sepol/policydb/polcaps.h>

static const char * const polcap_names[] = {
	"network_peer_controls",	/* POLICYDB_CAP_NETPEER */
	"open_perms",			/* POLICYDB_CAP_OPENPERM */
	"extended_socket_class",	/* POLICYDB_CAP_EXTSOCKCLASS */
	"always_check_network",		/* POLICYDB_CAP_ALWAYSNETWORK */
	"cgroup_seclabel",		/* POLICYDB_CAP_SECLABEL */
	"nnp_nosuid_transition",	/* POLICYDB_CAP_NNP_NOSUID_TRANSITION */
	"genfs_seclabel_symlinks",	/* POLICYDB_CAP_GENFS_SECLABEL_SYMLINKS */
	"ioctl_skip_cloexec",		/* POLICYDB_CAP_IOCTL_SKIP_CLOEXEC */
	NULL
};

int sepol_polcap_getnum(const char *name)
{
	int capnum;

	for (capnum = 0; capnum <= POLICYDB_CAP_MAX; capnum++) {
		if (polcap_names[capnum] == NULL)
			continue;
		if (strcasecmp(polcap_names[capnum], name) == 0)
			return capnum;
	}
	return -1;
}

const char *sepol_polcap_getname(unsigned int capnum)
{
	if (capnum > POLICYDB_CAP_MAX)
		return NULL;

	return polcap_names[capnum];
}
