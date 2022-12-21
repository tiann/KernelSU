#include <sepol/sepol.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void usage(char*) __attribute__((noreturn));

void usage(char *progname)
{
	printf("usage:  %s policy context\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	FILE *fp;

	if (argc != 3)
		usage(argv[0]);

	fp = fopen(argv[1], "r");
	if (!fp) {
		fprintf(stderr, "Can't open '%s':  %s\n",
			argv[1], strerror(errno));
		exit(1);
	}
	if (sepol_set_policydb_from_file(fp) < 0) {
		fprintf(stderr, "Error while processing %s:  %s\n",
			argv[1], strerror(errno));
		exit(1);
	}
	fclose(fp);

	if (sepol_check_context(argv[2]) < 0) {
		fprintf(stderr, "%s is not valid\n", argv[2]);
		exit(1);
	}

	printf("%s is valid\n", argv[2]);
	exit(0);
}
