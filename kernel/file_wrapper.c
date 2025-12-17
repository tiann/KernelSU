#include <linux/gfp.h>
#include <linux/fdtable.h>
#include <linux/export.h>
#include <linux/anon_inodes.h>
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/mount.h>

#include "objsec.h"

#include "klog.h" // IWYU pragma: keep
#include "selinux/selinux.h"
#include "ksud.h"

#include "file_wrapper.h"

struct ksu_file_wrapper {
    struct file *orig;
    struct file_operations ops;
};

static struct ksu_file_wrapper *ksu_create_file_wrapper(struct file *fp);

static int ksu_wrapper_open(struct inode *ino, struct file *fp)
{
    struct path *orig_path = fp->f_path.dentry->d_fsdata;
    struct file *orig_file =
        dentry_open(orig_path, fp->f_flags, current_cred());
    if (IS_ERR(orig_file)) {
        return PTR_ERR(orig_file);
    }
    struct ksu_file_wrapper *wrapper = ksu_create_file_wrapper(orig_file);
    if (IS_ERR(wrapper)) {
        filp_close(orig_file, current->files);
        return PTR_ERR(wrapper);
    }
    fp->private_data = wrapper;
    const struct file_operations *new_fops = fops_get(&wrapper->ops);
    replace_fops(fp, new_fops);
    return 0;
}

static const struct file_operations ksu_file_wrapper_inode_fops = {
    .owner = THIS_MODULE,
    .open = ksu_wrapper_open
};

static loff_t ksu_wrapper_llseek(struct file *fp, loff_t off, int flags)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->llseek(data->orig, off, flags);
}

static ssize_t ksu_wrapper_read(struct file *fp, char __user *ptr, size_t sz,
                                loff_t *off)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->read(orig, ptr, sz, off);
}

static ssize_t ksu_wrapper_write(struct file *fp, const char __user *ptr,
                                 size_t sz, loff_t *off)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->write(orig, ptr, sz, off);
}

static ssize_t ksu_wrapper_read_iter(struct kiocb *iocb, struct iov_iter *iovi)
{
    struct ksu_file_wrapper *data = iocb->ki_filp->private_data;
    struct file *orig = data->orig;
    iocb->ki_filp = orig;
    return orig->f_op->read_iter(iocb, iovi);
}

static ssize_t ksu_wrapper_write_iter(struct kiocb *iocb, struct iov_iter *iovi)
{
    struct ksu_file_wrapper *data = iocb->ki_filp->private_data;
    struct file *orig = data->orig;
    iocb->ki_filp = orig;
    return orig->f_op->write_iter(iocb, iovi);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static int ksu_wrapper_iopoll(struct kiocb *kiocb, struct io_comp_batch *icb,
                              unsigned int v)
{
    struct ksu_file_wrapper *data = kiocb->ki_filp->private_data;
    struct file *orig = data->orig;
    kiocb->ki_filp = orig;
    return orig->f_op->iopoll(kiocb, icb, v);
}
#else
static int ksu_wrapper_iopoll(struct kiocb *kiocb, bool spin)
{
    struct ksu_file_wrapper *data = kiocb->ki_filp->private_data;
    struct file *orig = data->orig;
    kiocb->ki_filp = orig;
    return orig->f_op->iopoll(kiocb, spin);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static int ksu_wrapper_iterate(struct file *fp, struct dir_context *dc)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->iterate(orig, dc);
}
#endif

static int ksu_wrapper_iterate_shared(struct file *fp, struct dir_context *dc)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->iterate_shared(orig, dc);
}

static __poll_t ksu_wrapper_poll(struct file *fp, struct poll_table_struct *pts)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->poll(orig, pts);
}

static long ksu_wrapper_unlocked_ioctl(struct file *fp, unsigned int cmd,
                                       unsigned long arg)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->unlocked_ioctl(orig, cmd, arg);
}

static long ksu_wrapper_compat_ioctl(struct file *fp, unsigned int cmd,
                                     unsigned long arg)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->compat_ioctl(orig, cmd, arg);
}

static int ksu_wrapper_mmap(struct file *fp, struct vm_area_struct *vma)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->mmap(orig, vma);
}

static int ksu_wrapper_flush(struct file *fp, fl_owner_t id)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->flush(orig, id);
}

static int ksu_wrapper_fsync(struct file *fp, loff_t off1, loff_t off2,
                             int datasync)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->fsync(orig, off1, off2, datasync);
}

static int ksu_wrapper_fasync(int arg, struct file *fp, int arg2)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->fasync(arg, orig, arg2);
}

static int ksu_wrapper_lock(struct file *fp, int arg1, struct file_lock *fl)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    return orig->f_op->lock(orig, arg1, fl);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t ksu_wrapper_sendpage(struct file *fp, struct page *pg, int arg1,
                                    size_t sz, loff_t *off, int arg2)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->sendpage) {
        return orig->f_op->sendpage(orig, pg, arg1, sz, off, arg2);
    }
    return -EINVAL;
}
#endif

static unsigned long ksu_wrapper_get_unmapped_area(struct file *fp,
                                                   unsigned long arg1,
                                                   unsigned long arg2,
                                                   unsigned long arg3,
                                                   unsigned long arg4)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->get_unmapped_area) {
        return orig->f_op->get_unmapped_area(orig, arg1, arg2, arg3, arg4);
    }
    return -EINVAL;
}

// static int ksu_wrapper_check_flags(int arg) {}

static int ksu_wrapper_flock(struct file *fp, int arg1, struct file_lock *fl)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->flock) {
        return orig->f_op->flock(orig, arg1, fl);
    }
    return -EINVAL;
}

static ssize_t ksu_wrapper_splice_write(struct pipe_inode_info *pii,
                                        struct file *fp, loff_t *off, size_t sz,
                                        unsigned int arg1)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->splice_write) {
        return orig->f_op->splice_write(pii, orig, off, sz, arg1);
    }
    return -EINVAL;
}

static ssize_t ksu_wrapper_splice_read(struct file *fp, loff_t *off,
                                       struct pipe_inode_info *pii, size_t sz,
                                       unsigned int arg1)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->splice_read) {
        return orig->f_op->splice_read(orig, off, pii, sz, arg1);
    }
    return -EINVAL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
void ksu_wrapper_splice_eof(struct file *fp)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->splice_eof) {
        return orig->f_op->splice_eof(orig);
    }
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static int ksu_wrapper_setlease(struct file *fp, int arg1,
                                struct file_lease **fl, void **p)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->setlease) {
        return orig->f_op->setlease(orig, arg1, fl, p);
    }
    return -EINVAL;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int ksu_wrapper_setlease(struct file *fp, int arg1,
                                struct file_lock **fl, void **p)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->setlease) {
        return orig->f_op->setlease(orig, arg1, fl, p);
    }
    return -EINVAL;
}
#else
static int ksu_wrapper_setlease(struct file *fp, long arg1,
                                struct file_lock **fl, void **p)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->setlease) {
        return orig->f_op->setlease(orig, arg1, fl, p);
    }
    return -EINVAL;
}
#endif

static long ksu_wrapper_fallocate(struct file *fp, int mode, loff_t offset,
                                  loff_t len)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->fallocate) {
        return orig->f_op->fallocate(orig, mode, offset, len);
    }
    return -EINVAL;
}

static void ksu_wrapper_show_fdinfo(struct seq_file *m, struct file *f)
{
    struct ksu_file_wrapper *data = f->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->show_fdinfo) {
        orig->f_op->show_fdinfo(m, orig);
    }
}

// https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/fs/read_write.c;l=1593-1606;drc=398da7defe218d3e51b0f3bdff75147e28125b60
static ssize_t ksu_wrapper_copy_file_range(struct file *file_in, loff_t pos_in,
                                           struct file *file_out,
                                           loff_t pos_out, size_t len,
                                           unsigned int flags)
{
    struct ksu_file_wrapper *data = file_out->private_data;
    struct file *orig = data->orig;
    return orig->f_op->copy_file_range(file_in, pos_in, orig, pos_out, len,
                                       flags);
}

// no REMAP_FILE_DEDUP: use file_in
// https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/fs/read_write.c;l=1598-1599;drc=398da7defe218d3e51b0f3bdff75147e28125b60
// https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/fs/remap_range.c;l=403-404;drc=398da7defe218d3e51b0f3bdff75147e28125b60
// REMAP_FILE_DEDUP: use file_out
// https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/fs/remap_range.c;l=483-484;drc=398da7defe218d3e51b0f3bdff75147e28125b60
static loff_t ksu_wrapper_remap_file_range(struct file *file_in, loff_t pos_in,
                                           struct file *file_out,
                                           loff_t pos_out, loff_t len,
                                           unsigned int remap_flags)
{
    if (remap_flags & REMAP_FILE_DEDUP) {
        struct ksu_file_wrapper *data = file_out->private_data;
        struct file *orig = data->orig;
        return orig->f_op->remap_file_range(file_in, pos_in, orig, pos_out, len,
                                            remap_flags);
    } else {
        struct ksu_file_wrapper *data = file_in->private_data;
        struct file *orig = data->orig;
        return orig->f_op->remap_file_range(orig, pos_in, file_out, pos_out,
                                            len, remap_flags);
    }
}

static int ksu_wrapper_fadvise(struct file *fp, loff_t off1, loff_t off2,
                               int flags)
{
    struct ksu_file_wrapper *data = fp->private_data;
    struct file *orig = data->orig;
    if (orig->f_op->fadvise) {
        return orig->f_op->fadvise(orig, off1, off2, flags);
    }
    return -EINVAL;
}

static void ksu_release_file_wrapper(struct ksu_file_wrapper *data);

static int ksu_wrapper_release(struct inode *inode, struct file *filp)
{
    // https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/fs/file_table.c;l=467-473;drc=3be0b283b562eabbc2b1f3bb534dc8903079bbaa
    // f_op->release is called before fops_put(f_op), so we put it manually.
    fops_put(filp->f_op);
    // prevent it from being put again
    filp->f_op = NULL;
    ksu_release_file_wrapper(filp->private_data);
    return 0;
}

static struct ksu_file_wrapper *ksu_create_file_wrapper(struct file *fp)
{
    struct ksu_file_wrapper *p =
        kcalloc(1, sizeof(struct ksu_file_wrapper), GFP_KERNEL);
    if (!p) {
        return ERR_PTR(-ENOMEM);
    }

    get_file(fp);

    p->orig = fp;
    p->ops.owner = THIS_MODULE;
    p->ops.llseek = fp->f_op->llseek ? ksu_wrapper_llseek : NULL;
    p->ops.read = fp->f_op->read ? ksu_wrapper_read : NULL;
    p->ops.write = fp->f_op->write ? ksu_wrapper_write : NULL;
    p->ops.read_iter = fp->f_op->read_iter ? ksu_wrapper_read_iter : NULL;
    p->ops.write_iter = fp->f_op->write_iter ? ksu_wrapper_write_iter : NULL;
    p->ops.iopoll = fp->f_op->iopoll ? ksu_wrapper_iopoll : NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
    p->ops.iterate = fp->f_op->iterate ? ksu_wrapper_iterate : NULL;
#endif
    p->ops.iterate_shared =
        fp->f_op->iterate_shared ? ksu_wrapper_iterate_shared : NULL;
    p->ops.poll = fp->f_op->poll ? ksu_wrapper_poll : NULL;
    p->ops.unlocked_ioctl =
        fp->f_op->unlocked_ioctl ? ksu_wrapper_unlocked_ioctl : NULL;
    p->ops.compat_ioctl =
        fp->f_op->compat_ioctl ? ksu_wrapper_compat_ioctl : NULL;
    p->ops.mmap = fp->f_op->mmap ? ksu_wrapper_mmap : NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    p->ops.fop_flags = fp->f_op->fop_flags;
#else
    p->ops.mmap_supported_flags = fp->f_op->mmap_supported_flags;
#endif
    p->ops.flush = fp->f_op->flush ? ksu_wrapper_flush : NULL;
    p->ops.release = ksu_wrapper_release;
    p->ops.fsync = fp->f_op->fsync ? ksu_wrapper_fsync : NULL;
    p->ops.fasync = fp->f_op->fasync ? ksu_wrapper_fasync : NULL;
    p->ops.lock = fp->f_op->lock ? ksu_wrapper_lock : NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
    p->ops.sendpage = fp->f_op->sendpage ? ksu_wrapper_sendpage : NULL;
#endif
    p->ops.get_unmapped_area =
        fp->f_op->get_unmapped_area ? ksu_wrapper_get_unmapped_area : NULL;
    p->ops.check_flags = fp->f_op->check_flags;
    p->ops.flock = fp->f_op->flock ? ksu_wrapper_flock : NULL;
    p->ops.splice_write =
        fp->f_op->splice_write ? ksu_wrapper_splice_write : NULL;
    p->ops.splice_read = fp->f_op->splice_read ? ksu_wrapper_splice_read : NULL;
    p->ops.setlease = fp->f_op->setlease ? ksu_wrapper_setlease : NULL;
    p->ops.fallocate = fp->f_op->fallocate ? ksu_wrapper_fallocate : NULL;
    p->ops.show_fdinfo = fp->f_op->show_fdinfo ? ksu_wrapper_show_fdinfo : NULL;
    p->ops.copy_file_range =
        fp->f_op->copy_file_range ? ksu_wrapper_copy_file_range : NULL;
    p->ops.remap_file_range =
        fp->f_op->remap_file_range ? ksu_wrapper_remap_file_range : NULL;
    p->ops.fadvise = fp->f_op->fadvise ? ksu_wrapper_fadvise : NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    p->ops.splice_eof = fp->f_op->splice_eof ? ksu_wrapper_splice_eof : NULL;
#endif

    return p;
}

static void ksu_release_file_wrapper(struct ksu_file_wrapper *data)
{
    fput((struct file *)data->orig);
    kfree(data);
}

static char *ksu_wrapper_d_dname(struct dentry *dentry, char *buffer,
                                 int buflen)
{
    struct path *orig_path = dentry->d_fsdata;
    return d_path(orig_path, buffer, buflen);
}

static void ksu_wrapper_d_release(struct dentry *dentry)
{
    struct path *orig_path = dentry->d_fsdata;
    path_put(orig_path);
    kfree(orig_path);
}

static const struct dentry_operations ksu_file_wrapper_d_ops = {
    .d_dname = ksu_wrapper_d_dname,
    .d_release = ksu_wrapper_d_release
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
#define ksu_anon_inode_create_getfile_compat anon_inode_create_getfile
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)
#define ksu_anon_inode_create_getfile_compat anon_inode_getfile_secure
#else
// There is no anon_inode_create_getfile before 5.16, but it's not difficult to implement it.
// https://cs.android.com/android/kernel/superproject/+/common-android12-5.10:common/fs/anon_inodes.c;l=58-125;drc=0d34ce8aa78e38affbb501690bcabec4df88620e

// Borrow kernel's anon_inode_mnt, so that we don't need to mount one by ourselves.
static struct vfsmount *anon_inode_mnt __read_mostly;

static struct inode *
ksu_anon_inode_make_secure_inode(const char *name,
                                 const struct inode *context_inode)
{
    struct inode *inode;
    const struct qstr qname = QSTR_INIT(name, strlen(name));
    int error;

    if (unlikely(!anon_inode_mnt)) {
        return ERR_PTR(-ENODEV);
    }

    inode = alloc_anon_inode(anon_inode_mnt->mnt_sb);
    if (IS_ERR(inode))
        return inode;
    inode->i_flags &= ~S_PRIVATE;
    error = security_inode_init_security_anon(inode, &qname, context_inode);
    if (error) {
        iput(inode);
        return ERR_PTR(error);
    }
    return inode;
}

static struct file *ksu_anon_inode_create_getfile_compat(
    const char *name, const struct file_operations *fops, void *priv, int flags,
    const struct inode *context_inode)
{
    struct inode *inode;
    struct file *file;

    if (fops->owner && !try_module_get(fops->owner))
        return ERR_PTR(-ENOENT);

    inode = ksu_anon_inode_make_secure_inode(name, context_inode);
    if (IS_ERR(inode)) {
        file = ERR_CAST(inode);
        goto err;
    }

    file = alloc_file_pseudo(inode, anon_inode_mnt, name,
                             flags & (O_ACCMODE | O_NONBLOCK), fops);
    if (IS_ERR(file))
        goto err_iput;

    file->f_mapping = inode->i_mapping;

    file->private_data = priv;

    return file;

err_iput:
    iput(inode);
err:
    module_put(fops->owner);
    return file;
}
#endif

int ksu_install_file_wrapper(int fd)
{
    int out_fd, ret;
    struct file *orig_file = fget(fd);
    if (!orig_file) {
        return -EBADF;
    }

    out_fd = get_unused_fd_flags(O_CLOEXEC);
    if (out_fd < 0) {
        ret = out_fd;
        goto done;
    }

    struct ksu_file_wrapper *file_wrapper_data =
        ksu_create_file_wrapper(orig_file);
    if (IS_ERR(file_wrapper_data)) {
        ret = PTR_ERR(file_wrapper_data);
        goto out_put_fd;
    }

    struct file *wrapper_file = ksu_anon_inode_create_getfile_compat(
        "[ksu_fdwrapper]", &file_wrapper_data->ops, file_wrapper_data,
        orig_file->f_flags, NULL);
    if (IS_ERR(wrapper_file)) {
        pr_err("ksu_fdwrapper: getfile failed: %ld\n", PTR_ERR(wrapper_file));
        ret = PTR_ERR(wrapper_file);
        goto out_release_wrapper;
    }

    // Now do magic on inode and dentry.
    // It should be safe to modify them since the file hasn't been published.

    struct inode *wrapper_inode = file_inode(wrapper_file);
    // libc's stdio relies on the fstat() result of the fd to determine its buffer type.
    wrapper_inode->i_mode = file_inode(orig_file)->i_mode;
    struct inode_security_struct *wrapper_sec = selinux_inode(wrapper_inode);
    // Use ksu_file_sid to bypass SELinux check.
    // When we call `su` from terminal app, this is useful.
    if (wrapper_sec) {
        wrapper_sec->sid = ksu_file_sid;
    }
    // Install open file operation for inode.
    wrapper_inode->i_fop = &ksu_file_wrapper_inode_fops;

    struct path *orig_path = kmalloc(sizeof(struct path), GFP_KERNEL);
    if (!orig_path) {
        ret = -ENOMEM;
        goto out_put_wrapper_file;
    }
    *orig_path = orig_file->f_path;
    path_get(orig_path);
    // Some applications (such as screen) won't work if the tty's path is weird,
    // Therefore, we use d_dname to spoof it to return the path to the original file.
    wrapper_file->f_path.dentry->d_fsdata = orig_path;
    wrapper_file->f_path.dentry->d_op = &ksu_file_wrapper_d_ops;

    fd_install(out_fd, wrapper_file);
    ret = out_fd;
    goto done;

out_put_wrapper_file:
    fput(wrapper_file);
    // file_wrapper will be released by fput
    goto out_put_fd;
out_release_wrapper:
    ksu_release_file_wrapper(file_wrapper_data);
out_put_fd:
    put_unused_fd(out_fd);
done:
    fput(orig_file);

    return ret;
}

void ksu_file_wrapper_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    static const struct file_operations tmp = { .owner = THIS_MODULE };
    struct file *dummy = anon_inode_getfile("dummy", &tmp, NULL, 0);
    if (IS_ERR(dummy)) {
        pr_err(
            "file_wrapper: initialize anon_inode_mnt failed, can't get file: %ld\n",
            PTR_ERR(dummy));
        return;
    }
    anon_inode_mnt = dummy->f_path.mnt;
    if (unlikely(!anon_inode_mnt)) {
        pr_err("file_wrapper: initialize anon_inode_mnt failed, got NULL\n");
    }
    fput(dummy);
#endif
}
