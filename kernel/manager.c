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

uid_t ksu_manager_uid = INVALID_UID;

bool become_manager(char *pkg)
{
	struct fdtable *files_table;
	int i = 0;
	struct path files_path;
	char *cwd;
	char *buf;
	bool result = false;

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

	// todo: use iterate_fd
	while (files_table->fd[i] != NULL) {
		files_path = files_table->fd[i]->f_path;
		if (!d_is_reg(files_path.dentry)) {
			i++;
			continue;
		}
		cwd = d_path(&files_path, buf, PATH_MAX);
		if (startswith(cwd, "/data/app/") == 0 &&
		    endswith(cwd, "/base.apk") == 0) {
			// we have found the apk!
			pr_info("found apk: %s", cwd);
			if (!strstr(cwd, pkg)) {
				pr_info("apk path not match package name!\n");
				i++;
				continue;
			}
			if (is_manager_apk(cwd) == 0) {
				// check passed
				uid_t uid = current_uid().val;
				pr_info("manager uid: %d\n", uid);

				ksu_set_manager_uid(uid);

				result = true;
				goto clean;
			} else {
				pr_info("manager signature invalid!");
			}

			break;
		}
		i++;
	}

clean:
	kfree(buf);
	return result;
}
