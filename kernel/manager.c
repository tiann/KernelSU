#include "linux/cred.h"
#include "linux/gfp.h"
#include "linux/slab.h"
#include "linux/uidgid.h"
#include "linux/version.h"

#include "linux/fdtable.h"
#include "linux/fs.h"
#include "linux/rcupdate.h"

#include "apk_sign.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "manager.h"

uid_t ksu_manager_uid = KSU_INVALID_UID;

bool become_manager(char *pkg)
{
	struct fdtable *files_table;
	int i = 0;
	struct path files_path;
	char *cwd;
	char *buf;
	bool result = false;

#ifdef KSU_MANAGER_PACKAGE
	// pkg is `/<real package>`
	if (strncmp(pkg + 1, KSU_MANAGER_PACKAGE,
		    sizeof(KSU_MANAGER_PACKAGE)) != 0) {
		pr_info("manager package is inconsistent with kernel build: %s\n",
			KSU_MANAGER_PACKAGE);
		return false;
	}
#endif
	// must be zygote's direct child, otherwise any app can fork a new process and
	// open manager's apk
	if (task_uid(current->real_parent).val != 0) {
		pr_info("parent is not zygote!\n");
		return false;
	}

	buf = (char *)kmalloc(PATH_MAX, GFP_ATOMIC);
	if (!buf) {
		pr_err("kalloc path failed.\n");
		return false;
	}

	files_table = files_fdtable(current->files);

	int pkg_len = strlen(pkg);
	// todo: use iterate_fd
	for (i = 0; files_table->fd[i] != NULL; i++) {
		files_path = files_table->fd[i]->f_path;
		if (!d_is_reg(files_path.dentry)) {
			continue;
		}
		cwd = d_path(&files_path, buf, PATH_MAX);
		if (startswith(cwd, "/data/app/") != 0 ||
		    endswith(cwd, "/base.apk") != 0) {
			continue;
		}
		// we have found the apk!
		pr_info("found apk: %s\n", cwd);
		char *pkg_index = strstr(cwd, pkg);
		if (!pkg_index) {
			pr_info("apk path not match package name!\n");
			continue;
		}
		char *next_char = pkg_index + pkg_len;
		// because we ensure the cwd must startswith `/data/app` and endswith `base.apk`
		// we don't need to check if the pointer is out of bounds
		if (*next_char != '-') {
			// from android 8.1: http://aospxref.com/android-8.1.0_r81/xref/frameworks/base/services/core/java/com/android/server/pm/PackageManagerService.java#17612
			// to android 13: http://aospxref.com/android-13.0.0_r3/xref/frameworks/base/services/core/java/com/android/server/pm/PackageManagerServiceUtils.java#1208
			// /data/app/~~[randomStringA]/[packageName]-[randomStringB]
			// the previous char must be `/` and the next char must be `-`
			// because we use strstr instead of equals, this is a strong verfication.
			pr_info("invalid pkg: %s\n", pkg);
			continue;
		}
		if (is_manager_apk(cwd)) {
			// check passed
			uid_t uid = current_uid().val;
			pr_info("manager uid: %d\n", uid);

			ksu_set_manager_uid(uid);

			result = true;
			goto clean;
		} else {
			pr_info("manager signature invalid!\n");
		}

		break;
	}

clean:
	kfree(buf);
	return result;
}
