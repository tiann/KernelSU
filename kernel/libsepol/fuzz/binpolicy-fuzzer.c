#include <sepol/debug.h>
#include <sepol/kernel_to_cil.h>
#include <sepol/kernel_to_conf.h>
#include <sepol/policydb/policydb.h>

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int write_binary_policy(policydb_t *p, FILE *outfp)
{
	struct policy_file pf;

	policy_file_init(&pf);
	pf.type = PF_USE_STDIO;
	pf.fp = outfp;
	return ksu_policydb_write(p, &pf);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	policydb_t policydb = {};
	sidtab_t sidtab = {};
	struct policy_file pf;
	FILE *devnull = NULL;

	sepol_debug(0);

	policy_file_init(&pf);
	pf.type = PF_USE_MEMORY;
	pf.data = (char *) data;
	pf.len = size;

	if (policydb_init(&policydb))
		goto exit;

	if (ksu_policydb_read(&policydb, &pf, /*verbose=*/0))
		goto exit;

	if (ksu_policydb_load_isids(&policydb, &sidtab))
		goto exit;

	if (policydb.policy_type == POLICY_KERN)
		(void) policydb_optimize(&policydb);

	devnull = fopen("/dev/null", "w");
	if (!devnull)
		goto exit;

	(void) write_binary_policy(&policydb, devnull);

	(void) sepol_kernel_policydb_to_conf(devnull, &policydb);

	(void) sepol_kernel_policydb_to_cil(devnull, &policydb);

exit:
	if (devnull != NULL)
		fclose(devnull);

	ksu_policydb_destroy(&policydb);
	sepol_sidtab_destroy(&sidtab);

	/* Non-zero return values are reserved for future use. */
	return 0;
}
