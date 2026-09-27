#include <linux/anon_inodes.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "infra/file_guard.h"
#include "infra/event_queue.h"
#include "klog.h" // IWYU pragma: keep
#include "sulog/event.h"

static DEFINE_MUTEX(ksu_sulog_fd_lock);
static bool ksu_sulog_fd_active;

static ssize_t ksu_sulog_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    return ksu_event_queue_read(ksu_sulog_get_queue(), buf, count, file->f_flags);
}

static __poll_t ksu_sulog_poll(struct file *file, poll_table *wait)
{
    return ksu_event_queue_poll(ksu_sulog_get_queue(), file, wait);
}

static int ksu_sulog_release(struct inode *inode, struct file *file)
{
    struct ksu_file *guard = file->private_data;
    if (!guard)
        return 0;

    ksu_file_release(guard);
    kfree(guard);

    mutex_lock(&ksu_sulog_fd_lock);
    ksu_sulog_fd_active = false;
    mutex_unlock(&ksu_sulog_fd_lock);

    pr_info("sulog: fd released\n");
    return 0;
}

static void ksu_sulog_cleanup(struct file *f)
{
    kfree(f->private_data);

    mutex_lock(&ksu_sulog_fd_lock);
    ksu_sulog_fd_active = false;
    mutex_unlock(&ksu_sulog_fd_lock);

    ksu_event_queue_close(ksu_sulog_get_queue());
    pr_info("sulog: fd cleanup\n");

    // TODO: wait for sulog fd release ?
}

static const struct file_operations ksu_sulog_fops = {
    .read = ksu_sulog_read,
    .poll = ksu_sulog_poll,
    .release = ksu_sulog_release,
    .llseek = noop_llseek,
};

int ksu_install_sulog_fd(void)
{
    struct file *filp;
    struct ksu_file *guard;
    int fd;

    mutex_lock(&ksu_sulog_fd_lock);

    if (ksu_sulog_fd_active) {
        fd = -EBUSY;
        goto out_unlock;
    }

    if (READ_ONCE(ksu_sulog_get_queue()->closed)) {
        fd = -EPIPE;
        goto out_unlock;
    }

    guard = kzalloc(sizeof(*guard), GFP_KERNEL);
    if (!guard) {
        fd = -ENOMEM;
        goto out_unlock;
    }
    guard->cleanup = ksu_sulog_cleanup;

    fd = get_unused_fd_flags(O_CLOEXEC);
    if (fd < 0)
        goto out_unlock;

    filp = anon_inode_getfile("[ksu_sulog]", &ksu_sulog_fops, NULL, O_RDONLY | O_CLOEXEC);
    if (IS_ERR(filp)) {
        fd = PTR_ERR(filp);
        goto out_put_fd;
    }

    int ret = ksu_file_add(filp, guard);
    if (ret < 0) {
        fd = ret;
        goto out_put_file;
    }
    filp->private_data = guard;

    ksu_sulog_fd_active = true;
    fd_install(fd, filp);
    pr_info("sulog: fd installed %d for pid %d\n", fd, current->pid);

out_put_file:
    fput(filp);

out_put_fd:
    put_unused_fd(fd);

out_unlock:
    mutex_unlock(&ksu_sulog_fd_lock);
    return fd;
}

void __init ksu_sulog_fd_init(void)
{
    mutex_lock(&ksu_sulog_fd_lock);
    ksu_sulog_fd_active = false;
    mutex_unlock(&ksu_sulog_fd_lock);
}

void __exit ksu_sulog_fd_exit(void)
{
    mutex_lock(&ksu_sulog_fd_lock);
    ksu_sulog_fd_active = false;
    mutex_unlock(&ksu_sulog_fd_lock);

    ksu_event_queue_close(ksu_sulog_get_queue());
}
