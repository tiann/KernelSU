#ifndef __KSU_H_SUPERCALL
#define __KSU_H_SUPERCALL

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>

// IOCTL handler types
typedef int (*ksu_ioctl_handler_t)(void __user *arg);
typedef bool (*ksu_perm_check_t)(void);

// IOCTL command mapping
struct ksu_ioctl_cmd_map {
    unsigned int cmd;
    const char *name;
    ksu_ioctl_handler_t handler;
    ksu_perm_check_t perm_check; // Permission check function
    bool allow_su_session;
};

// Install KSU fd to current process
int ksu_install_fd(void);
// Install a KSU fd that authorizes operations required while starting su.
int ksu_install_su_fd(void);
bool ksu_is_su_session_fd(const struct file *filp);

void ksu_supercalls_init(void);
void ksu_supercalls_exit(void);
#endif // __KSU_H_SUPERCALL
