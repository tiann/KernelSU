#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sepol/policydb/services.h>
#include <sepol/sepol.h>


int main(int argc, char *argv[])
{
	FILE *fp;
	sepol_security_id_t ssid, tsid, out_sid;
	sepol_security_class_t tclass;
	char *context;
	size_t context_len;

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

	if (sepol_member_sid(ssid, tsid, tclass, &out_sid) < 0) {
		fprintf(stderr, "Failed to compute member sid:  %s\n", strerror(errno));
		return 1;
	}

	if (sepol_sid_to_context(out_sid, &context, &context_len) < 0) {
		fprintf(stderr, "Failed to convert sid %u:  %s\n", out_sid, strerror(errno));
		return 1;
	}

	printf("%s\n", context);
	free(context);

	return 0;
}
