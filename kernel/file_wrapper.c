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

#include "klog.h" // IWYU pragma: keep
#include "selinux/selinux.h"

#include "file_wrapper.h"

static loff_t ksu_wrapper_llseek(struct file *fp, loff_t off, int flags) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->llseek(data->orig, off, flags);
}

static ssize_t ksu_wrapper_read(struct file *fp, char __user *ptr, size_t sz, loff_t *off) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->read(orig, ptr, sz, off);
}

static ssize_t ksu_wrapper_write(struct file *fp, const char __user *ptr, size_t sz, loff_t *off) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->write(orig, ptr, sz, off);
}

static ssize_t ksu_wrapper_read_iter(struct kiocb *iocb, struct iov_iter *iovi) {
	struct ksu_file_wrapper* data = iocb->ki_filp->private_data;
	struct file* orig = data->orig;
	iocb->ki_filp = orig;
	return orig->f_op->read_iter(iocb, iovi);
}

static ssize_t ksu_wrapper_write_iter(struct kiocb *iocb, struct iov_iter *iovi) {
	struct ksu_file_wrapper* data = iocb->ki_filp->private_data;
	struct file* orig = data->orig;
	iocb->ki_filp = orig;
	return orig->f_op->write_iter(iocb, iovi);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static int ksu_wrapper_iopoll(struct kiocb *kiocb, struct io_comp_batch* icb, unsigned int v) {
	struct ksu_file_wrapper* data = kiocb->ki_filp->private_data;
	struct file* orig = data->orig;
	kiocb->ki_filp = orig;
	return orig->f_op->iopoll(kiocb, icb, v);
}
#else
static int ksu_wrapper_iopoll(struct kiocb *kiocb, bool spin) {
	struct ksu_file_wrapper* data = kiocb->ki_filp->private_data;
	struct file* orig = data->orig;
	kiocb->ki_filp = orig;
	return orig->f_op->iopoll(kiocb, spin);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static int ksu_wrapper_iterate (struct file *fp, struct dir_context *dc) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->iterate(orig, dc);
}
#endif

static int ksu_wrapper_iterate_shared(struct file *fp, struct dir_context *dc) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->iterate_shared(orig, dc);
}

static __poll_t ksu_wrapper_poll(struct file *fp, struct poll_table_struct *pts) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->poll(orig, pts);
}

static long ksu_wrapper_unlocked_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->unlocked_ioctl(orig, cmd, arg);
}

static long ksu_wrapper_compat_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->compat_ioctl(orig, cmd, arg);
}

static int ksu_wrapper_mmap(struct file *fp, struct vm_area_struct * vma) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->mmap(orig, vma);
}

// static unsigned long mmap_supported_flags {}

static int ksu_wrapper_open(struct inode *ino, struct file *fp) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	struct inode *orig_ino = file_inode(orig);
	return orig->f_op->open(orig_ino, orig);
}

static int ksu_wrapper_flush(struct file *fp, fl_owner_t id) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->flush(orig, id);
}


static int ksu_wrapper_fsync(struct file *fp, loff_t off1, loff_t off2, int datasync) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->fsync(orig, off1, off2, datasync);
}

static int ksu_wrapper_fasync(int arg, struct file *fp, int arg2) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->fasync(arg, orig, arg2);
}

static int ksu_wrapper_lock(struct file *fp, int arg1, struct file_lock *fl) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	return orig->f_op->lock(orig, arg1, fl);
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
static ssize_t ksu_wrapper_sendpage(struct file *fp, struct page *pg, int arg1, size_t sz, loff_t *off, int arg2) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->sendpage) {
		return orig->f_op->sendpage(orig, pg, arg1, sz, off, arg2);
	}
	return -EINVAL;
}
#endif

static unsigned long ksu_wrapper_get_unmapped_area(struct file *fp, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->get_unmapped_area) {
		return orig->f_op->get_unmapped_area(orig, arg1, arg2, arg3, arg4);
	}
	return -EINVAL;
}

// static int ksu_wrapper_check_flags(int arg) {}

static int ksu_wrapper_flock(struct file *fp, int arg1, struct file_lock *fl) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->flock) {
		return orig->f_op->flock(orig, arg1, fl);
	}
	return -EINVAL;
}

static ssize_t ksu_wrapper_splice_write(struct pipe_inode_info * pii, struct file *fp, loff_t *off, size_t sz, unsigned int arg1) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->splice_write) {
		return orig->f_op->splice_write(pii, orig, off, sz, arg1);
	}
	return -EINVAL;
}

static ssize_t ksu_wrapper_splice_read(struct file *fp, loff_t *off, struct pipe_inode_info *pii, size_t sz, unsigned int arg1) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->splice_read) {
		return orig->f_op->splice_read(orig, off, pii, sz, arg1);
	}
	return -EINVAL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
void ksu_wrapper_splice_eof(struct file *fp) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->splice_eof) {
		return orig->f_op->splice_eof(orig);
	}
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static int ksu_wrapper_setlease(struct file *fp, int arg1, struct file_lease **fl, void **p) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->setlease) {
		return orig->f_op->setlease(orig, arg1, fl, p);
	}
	return -EINVAL;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int ksu_wrapper_setlease(struct file *fp, int arg1, struct file_lock **fl, void **p) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->setlease) {
		return orig->f_op->setlease(orig, arg1, fl, p);
	}
	return -EINVAL;
}
#else
static int ksu_wrapper_setlease(struct file *fp, long arg1, struct file_lock **fl, void **p) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->setlease) {
		return orig->f_op->setlease(orig, arg1, fl, p);
	}
	return -EINVAL;
}
#endif

static long ksu_wrapper_fallocate(struct file *fp, int mode, loff_t offset, loff_t len) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->fallocate) {
		return orig->f_op->fallocate(orig, mode, offset, len);
	}
	return -EINVAL;
}

static void ksu_wrapper_show_fdinfo(struct seq_file *m, struct file *f) {
	struct ksu_file_wrapper* data = m->file->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->show_fdinfo) {
		orig->f_op->show_fdinfo(m, orig);
	}
}

static ssize_t ksu_wrapper_copy_file_range(struct file *f1, loff_t off1, struct file *f2,
		loff_t off2, size_t sz, unsigned int flags) {
	// TODO: determine which file to use
	struct ksu_file_wrapper* data = f1->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->copy_file_range) {
		return orig->f_op->copy_file_range(orig, off1, f2, off2, sz, flags);
	}
	return -EINVAL;
}

static loff_t ksu_wrapper_remap_file_range(struct file *file_in, loff_t pos_in,
				struct file *file_out, loff_t pos_out,
				loff_t len, unsigned int remap_flags) {
	// TODO: determine which file to use
	struct ksu_file_wrapper* data = file_in->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->remap_file_range) {
		return orig->f_op->remap_file_range(orig, pos_in, file_out, pos_out, len, remap_flags);
	}
	return -EINVAL;
}

static int ksu_wrapper_fadvise(struct file *fp, loff_t off1, loff_t off2, int flags) {
	struct ksu_file_wrapper* data = fp->private_data;
	struct file* orig = data->orig;
	if (orig->f_op->fadvise) {
		return orig->f_op->fadvise(orig, off1, off2, flags);
	}
	return -EINVAL;
}

static int ksu_wrapper_release(struct inode *inode, struct file *filp) {
	ksu_delete_file_wrapper(filp->private_data);
    return 0;
}

struct ksu_file_wrapper* ksu_create_file_wrapper(struct file* fp) {
	struct ksu_file_wrapper* p = kcalloc(sizeof(struct ksu_file_wrapper), 1, GFP_KERNEL);
	if (!p) {
		return NULL;
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
	p->ops.iterate_shared = fp->f_op->iterate_shared ? ksu_wrapper_iterate_shared : NULL;
	p->ops.poll = fp->f_op->poll ? ksu_wrapper_poll : NULL;
	p->ops.unlocked_ioctl = fp->f_op->unlocked_ioctl ? ksu_wrapper_unlocked_ioctl : NULL;
	p->ops.compat_ioctl = fp->f_op->compat_ioctl ? ksu_wrapper_compat_ioctl : NULL;
	p->ops.mmap = fp->f_op->mmap ? ksu_wrapper_mmap : NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
	p->ops.fop_flags = fp->f_op->fop_flags;
#else
	p->ops.mmap_supported_flags = fp->f_op->mmap_supported_flags;
#endif
	p->ops.open = fp->f_op->open ? ksu_wrapper_open : NULL;
	p->ops.flush = fp->f_op->flush ? ksu_wrapper_flush : NULL;
	p->ops.release = ksu_wrapper_release;
	p->ops.fsync = fp->f_op->fsync ? ksu_wrapper_fsync : NULL;
	p->ops.fasync = fp->f_op->fasync ? ksu_wrapper_fasync : NULL;
	p->ops.lock = fp->f_op->lock ? ksu_wrapper_lock : NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
	p->ops.sendpage = fp->f_op->sendpage ? ksu_wrapper_sendpage : NULL;
#endif
	p->ops.get_unmapped_area = fp->f_op->get_unmapped_area ? ksu_wrapper_get_unmapped_area : NULL;
	p->ops.check_flags = fp->f_op->check_flags;
	p->ops.flock = fp->f_op->flock ? ksu_wrapper_flock : NULL;
	p->ops.splice_write = fp->f_op->splice_write ? ksu_wrapper_splice_write : NULL;
	p->ops.splice_read = fp->f_op->splice_read ? ksu_wrapper_splice_read : NULL;
	p->ops.setlease = fp->f_op->setlease ? ksu_wrapper_setlease : NULL;
	p->ops.fallocate = fp->f_op->fallocate ? ksu_wrapper_fallocate : NULL;
	p->ops.show_fdinfo = fp->f_op->show_fdinfo ? ksu_wrapper_show_fdinfo : NULL;
	p->ops.copy_file_range = fp->f_op->copy_file_range ? ksu_wrapper_copy_file_range : NULL;
	p->ops.remap_file_range = fp->f_op->remap_file_range ? ksu_wrapper_remap_file_range : NULL;
	p->ops.fadvise = fp->f_op->fadvise ? ksu_wrapper_fadvise : NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	p->ops.splice_eof = fp->f_op->splice_eof ? ksu_wrapper_splice_eof : NULL;
#endif

	return p;
}

void ksu_delete_file_wrapper(struct ksu_file_wrapper* data) {
	fput((struct file*) data->orig);
	kfree(data);
}
