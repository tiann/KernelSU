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
	struct sepol_av_decision avd;
	int rc;

	if (argc != 5) {
		printf("usage:  %s policy scontext tcontext tclass\n", argv[0]);
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

	rc = sepol_compute_av(ssid, tsid, tclass, 0, &avd);
	switch (rc) {
	case 0:
		printf("allowed:    %s\n", sepol_av_perm_to_string(tclass, avd.allowed));
		printf("decided:    %s\n", sepol_av_perm_to_string(tclass, avd.decided));
		printf("auditallow: %s\n", sepol_av_perm_to_string(tclass, avd.auditallow));
		printf("auditdeny:  %s\n", sepol_av_perm_to_string(tclass, avd.auditdeny));
		break;
	case -EINVAL:
		printf("Invalid request\n");
		break;
	default:
		printf("Failed to compute av decision: %d\n", rc);
	}

	return rc != 0;
}
