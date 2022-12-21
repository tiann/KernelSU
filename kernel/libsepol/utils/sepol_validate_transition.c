#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sepol/policydb/services.h>
#include <sepol/sepol.h>


int main(int argc, char *argv[])
{
	FILE *fp;
	sepol_security_id_t oldsid, newsid, tasksid;
	sepol_security_class_t tclass;
	char *reason = NULL;
	int ret;

	if (argc != 6) {
		printf("usage:  %s policy oldcontext newcontext tclass taskcontext\n", argv[0]);
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

	if (sepol_context_to_sid(argv[2], strlen(argv[2]), &oldsid) < 0) {
		fprintf(stderr, "Invalid old context %s\n", argv[2]);
		return 1;
	}

	if (sepol_context_to_sid(argv[3], strlen(argv[3]), &newsid) < 0) {
		fprintf(stderr, "Invalid new context %s\n", argv[3]);
		return 1;
	}

	if (sepol_string_to_security_class(argv[4], &tclass) < 0) {
		fprintf(stderr, "Invalid security class %s\n", argv[4]);
		return 1;
	}

	if (sepol_context_to_sid(argv[5], strlen(argv[5]), &tasksid) < 0) {
		fprintf(stderr, "Invalid task context %s\n", argv[5]);
		return 1;
	}

	ret = sepol_validate_transition_reason_buffer(oldsid, newsid, tasksid, tclass, &reason, SHOW_GRANTED);
	switch (ret) {
	case 0:
		printf("allowed\n");
		ret = 0;
		break;
	case -EPERM:
		printf("denied\n");
		printf("%s\n", reason ? reason : "unknown - possible BUG()");
		ret = 7;
		break;
	default:
		printf("sepol_validate_transition_reason_buffer returned %d errno: %s\n", ret, strerror(errno));
		ret = 1;
	}

	free(reason);

	return ret;
}
