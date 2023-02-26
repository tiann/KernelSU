#include "linux/fs.h"
#include "linux/moduleparam.h"

#include "apk_sign.h"
#include "klog.h" // IWYU pragma: keep
#include "kernel_compat.h"

static __always_inline int
check_v2_signature(char *path, unsigned expected_size, unsigned expected_hash)
{
	unsigned char buffer[0x11] = { 0 };
	u32 size4;
	u64 size8, size_of_block;

	loff_t pos;

	int sign = -1;
	int i;
	struct file *fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("open %s error.", path);
		return PTR_ERR(fp);
	}

	// disable inotify for this file
	fp->f_mode |= FMODE_NONOTIFY;

	sign = 1;
	// https://en.wikipedia.org/wiki/Zip_(file_format)#End_of_central_directory_record_(EOCD)
	for (i = 0;; ++i) {
		unsigned short n;
		pos = generic_file_llseek(fp, -i - 2, SEEK_END);
		ksu_kernel_read_compat(fp, &n, 2, &pos);
		if (n == i) {
			pos -= 22;
			ksu_kernel_read_compat(fp, &size4, 4, &pos);
			if ((size4 ^ 0xcafebabeu) == 0xccfbf1eeu) {
				break;
			}
		}
		if (i == 0xffff) {
			pr_info("error: cannot find eocd\n");
			goto clean;
		}
	}

	pos += 12;
	// offset
	ksu_kernel_read_compat(fp, &size4, 0x4, &pos);
	pos = size4 - 0x18;

	ksu_kernel_read_compat(fp, &size8, 0x8, &pos);
	ksu_kernel_read_compat(fp, buffer, 0x10, &pos);
	if (strcmp((char *)buffer, "APK Sig Block 42")) {
		goto clean;
	}

	pos = size4 - (size8 + 0x8);
	ksu_kernel_read_compat(fp, &size_of_block, 0x8, &pos);
	if (size_of_block != size8) {
		goto clean;
	}

	for (;;) {
		uint32_t id;
		uint32_t offset;
		ksu_kernel_read_compat(fp, &size8, 0x8, &pos); // sequence length
		if (size8 == size_of_block) {
			break;
		}
		ksu_kernel_read_compat(fp, &id, 0x4, &pos); // id
		offset = 4;
		pr_info("id: 0x%08x\n", id);
		if ((id ^ 0xdeadbeefu) == 0xafa439f5u ||
		    (id ^ 0xdeadbeefu) == 0x2efed62f) {
			ksu_kernel_read_compat(fp, &size4, 0x4,
				    &pos); // signer-sequence length
			ksu_kernel_read_compat(fp, &size4, 0x4, &pos); // signer length
			ksu_kernel_read_compat(fp, &size4, 0x4,
				    &pos); // signed data length
			offset += 0x4 * 3;

			ksu_kernel_read_compat(fp, &size4, 0x4,
				    &pos); // digests-sequence length
			pos += size4;
			offset += 0x4 + size4;

			ksu_kernel_read_compat(fp, &size4, 0x4,
				    &pos); // certificates length
			ksu_kernel_read_compat(fp, &size4, 0x4,
				    &pos); // certificate length
			offset += 0x4 * 2;
#if 0
			int hash = 1;
			signed char c;
			for (i = 0; i < size4; ++i) {
				ksu_kernel_read_compat(fp, &c, 0x1, &pos);
				hash = 31 * hash + c;
			}
			offset += size4;
			pr_info("    size: 0x%04x, hash: 0x%08x\n", size4, ((unsigned) hash) ^ 0x14131211u);
#else
			if (size4 == expected_size) {
				int hash = 1;
				signed char c;
				for (i = 0; i < size4; ++i) {
					ksu_kernel_read_compat(fp, &c, 0x1, &pos);
					hash = 31 * hash + c;
				}
				offset += size4;
				if ((((unsigned)hash) ^ 0x14131211u) ==
				    expected_hash) {
					sign = 0;
					break;
				}
			}
			// don't try again.
			break;
#endif
		}
		pos += (size8 - offset);
	}

clean:
	filp_close(fp, 0);

	return sign;
}

#ifdef CONFIG_KSU_DEBUG

unsigned ksu_expected_size = EXPECTED_SIZE;
unsigned ksu_expected_hash = EXPECTED_HASH;

#include "manager.h"

static int set_expected_size(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_uint(val, kp);
	ksu_invalidate_manager_uid();
	pr_info("ksu_expected_size set to %x", ksu_expected_size);
	return rv;
}

static int set_expected_hash(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_uint(val, kp);
	ksu_invalidate_manager_uid();
	pr_info("ksu_expected_hash set to %x", ksu_expected_hash);
	return rv;
}

static struct kernel_param_ops expected_size_ops = {
	.set = set_expected_size,
	.get = param_get_uint,
};

static struct kernel_param_ops expected_hash_ops = {
	.set = set_expected_hash,
	.get = param_get_uint,
};

module_param_cb(ksu_expected_size, &expected_size_ops, &ksu_expected_size,
		S_IRUSR | S_IWUSR);
module_param_cb(ksu_expected_hash, &expected_hash_ops, &ksu_expected_hash,
		S_IRUSR | S_IWUSR);

int is_manager_apk(char *path)
{
	return check_v2_signature(path, ksu_expected_size, ksu_expected_hash);
}

#else

int is_manager_apk(char *path)
{
	return check_v2_signature(path, EXPECTED_SIZE, EXPECTED_HASH);
}

#endif
