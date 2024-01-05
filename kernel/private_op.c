// ==================================================================================
//   DO NOT USE THIS FEATURE IN YOUR MODULE, CAUSE THIS IS A INTERNAL DEBUG FEATURE
// ==================================================================================

#include "linux/uaccess.h"
#include "linux/version.h"

#define KSU_CACHE_OP 1
#define KSU_CACHE_OP_READ 1
#define KSU_CACHE_OP_WRITE 2

#define SLOT_COUNT 128
static uint64_t private_cache[SLOT_COUNT]; // 8B * 128 = 1KB

void private_op_init(void){
	int i = 0;
	for (i = 0; i < SLOT_COUNT; i++) {
		private_cache[i] = 0;
	}
}

int ksu_handle_private_op(unsigned long arg3, unsigned long arg4){
	#ifdef CONFIG_KSU_DEBUG
	if (arg3 == KSU_CACHE_OP) {
		struct{
			uint8_t op;
			uint8_t slot;
			unsigned long value;
		}params;
		if (copy_from_user(&params, arg4, sizeof(params))) {
			return -1;
		}
		if (params.op != KSU_CACHE_OP_READ || params.op != KSU_CACHE_OP_WRITE) {
			return -1;
		}
		if (params.slot > 10) {
			return -1;
		}
		if (params.op == KSU_CACHE_OP_READ) {
			copy_to_user((void*)(arg4 + 16), &private_cache[params.slot], 8);
		}
		if (params.op == KSU_CACHE_OP_WRITE) {
			private_cache[params.slot] = params.value;
		}
		return 0;
	}
	#endif
	return -1;
}