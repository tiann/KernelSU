#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sepol/policydb/services.h>
#include <sepol/sepol.h>


int main(int argc, char *argv[])
{
	FILE *fp;
	sepol_security_id_t ssid, tsid;
	sepol_security_class_t tclass;
	const char *permlist;
	sepol_access_vector_t av;
	struct sepol_av_decision avd;
	unsigned int reason;
	char *reason_buf;
	int i;

	if (argc != 6) {
		printf("usage:  %s policy source_context target_context class permission[,permission2[,...]]\n", argv[0]);
		return 1;
	}

	fp = fopen(argv[1], "r");
	if (!fp) {
		fprintf(stderr, "Can't open policy %s:  %s\n", argv[1], strerror(errno));
		return 1;
	}
	if (sepol_set_policydb_from_file(fp) < 0) {
		fprintf(stderr, "Error while processing policy %s:  %s\n", argv[1], strerror(errno));
		fclose(fp);
		return 1;
	}
	fclose(fp);

	if (sepol_context_to_sid(argv[2], strlen(argv[2]), &ssid) < 0) {
		fprintf(stderr, "Invalid source context %s\n", argv[2]);
		return 1;
	}

	if (sepol_context_to_sid(argv[3], strlen(argv[3]), &tsid) < 0) {
		fprintf(stderr, "Invalid target context %s\n", argv[3]);
		return 1;
	}

	if (sepol_string_to_security_class(argv[4], &tclass) < 0) {
		fprintf(stderr, "Invalid security class %s\n", argv[4]);
		return 1;
	}

	permlist = argv[5];
	do {
		char *tmp = NULL;
		const char *perm;
		const char *delim = strchr(permlist, ',');

		if (delim) {
			tmp = strndup(permlist, delim - permlist);
			if (!tmp) {
				fprintf(stderr, "Failed to allocate memory:  %s\n", strerror(errno));
				return 1;
			}
		}

		perm = tmp ? tmp : permlist;

		if (sepol_string_to_av_perm(tclass, perm, &av) < 0) {
			fprintf(stderr, "Invalid permission %s for security class %s:  %s\n", perm, argv[4], strerror(errno));
			free(tmp);
			return 1;
		}

		free(tmp);

		permlist = strchr(permlist, ',');
	} while (permlist++);

	if (av == 0) {
		fprintf(stderr, "Empty permission set computed from %s\n", argv[5]);
		return 1;
	}

	if (sepol_compute_av_reason_buffer(ssid, tsid, tclass, av, &avd, &reason, &reason_buf, 0) < 0) {
		fprintf(stderr, "Failed to compute av decision:  %s\n", strerror(errno));
		return 1;
	}

	if ((avd.allowed & av) == av) {
		printf("requested permission %s allowed\n", argv[5]);
		free(reason_buf);
		return 0;
	}

	printf("requested permission %s denied by ", argv[5]);
	i = 0;
	if (reason & SEPOL_COMPUTEAV_TE) {
		printf("te-rule");
		i++;
	}
	if (reason & SEPOL_COMPUTEAV_CONS) {
		if (i > 0)
			printf(", ");
		printf("constraint");
		i++;
	}
	if (reason & SEPOL_COMPUTEAV_RBAC) {
		if (i > 0)
			printf(", ");
		printf("transition-constraint");
		i++;
	}
	if (reason & SEPOL_COMPUTEAV_BOUNDS) {
		if (i > 0)
			printf(", ");
		printf("type-bound");
		//i++;
	}

	if ((reason & SEPOL_COMPUTEAV_CONS) && reason_buf)
		printf("; reason:\n%s", reason_buf);

	free(reason_buf);

	printf("\n");

	return 7;
}
