#include <linux/cpu.h>
#include <linux/memory.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm-generic/errno-base.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "KSU_CONFIG: " fmt
#endif

uintptr_t kprobe_get_addr(const char *symbol_name) {
    int ret;
    struct kprobe kp;
    uintptr_t tmp = 0;
    kp.addr = 0;
    kp.symbol_name = symbol_name;
    ret = register_kprobe(&kp);
    tmp = (uintptr_t)kp.addr;
    if (ret < 0) {
        goto out; // not function, maybe symbol
    }
    unregister_kprobe(&kp);
out:
    return tmp;
}

static unsigned *ksu_expected_hash_ptr;
static unsigned *ksu_expected_size_ptr;

static unsigned expected_hash = 0;
static unsigned expected_size = 0;

static int set_expected_hash(const char *val, const struct kernel_param *kp){
	int rv = param_set_uint(val, kp);
	*ksu_expected_hash_ptr = expected_hash;
	pr_info("ksu_expected_hash set to %x", expected_hash);
	return rv;
}

static int set_expected_size(const char *val, const struct kernel_param *kp){
	int rv = param_set_uint(val, kp);
	*ksu_expected_size_ptr = expected_size;
	pr_info("ksu_expected_size set to %x", expected_size);
	return rv;
}

static struct kernel_param_ops expected_hash_ops = {
	.set = set_expected_hash,
	.get = param_get_uint,
};

static struct kernel_param_ops expected_size_ops = {
	.set = set_expected_size,
	.get = param_get_uint,
};

module_param_cb(expected_hash, &expected_hash_ops, &expected_hash, S_IRUSR | S_IWUSR);
module_param_cb(expected_size, &expected_size_ops, &expected_size, S_IRUSR | S_IWUSR);

int ksu_config_init(void){
	pr_info("ksu_config init");
	ksu_expected_hash_ptr = (unsigned *)kprobe_get_addr("ksu_expected_hash");
	ksu_expected_size_ptr = (unsigned *)kprobe_get_addr("ksu_expected_size");

	pr_info("ksu_expected_hash_ptr: 0x%lX", ksu_expected_hash_ptr);
	pr_info("ksu_expected_size_ptr: 0x%lX", ksu_expected_size_ptr);

	if (ksu_expected_hash_ptr == 0 || ksu_expected_size_ptr == 0) {
		pr_err("Cannot find address of ksu_expected_*");
		return ENOENT;
	}

	expected_hash = *ksu_expected_hash_ptr;
	expected_size = *ksu_expected_size_ptr;

	pr_info("ksu_expected_hash: 0x%X", expected_hash);
	pr_info("ksu_expected_size: 0x%X", expected_size);
	
	return 0;
}

void ksu_config_exit(void){
	pr_info("ksu_config exit");
}

module_init(ksu_config_init);
module_exit(ksu_config_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ylarod");
MODULE_DESCRIPTION("A module to modify ksu_expected_*");