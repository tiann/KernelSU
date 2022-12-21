// #include <stdint.h>

#include <sepol/handle.h>
#include <sepol/policydb/policydb.h>

int value_isvalid(uint32_t value, uint32_t nprim);
int validate_policydb(sepol_handle_t *handle, policydb_t *p);
