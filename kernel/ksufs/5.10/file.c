// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2017 Red Hat, Inc.
 */

#include <linux/cred.h>
#include <linux/file.h>
#include <linux/mount.h>
#include <linux/xattr.h>
#include <linux/uio.h>
#include <linux/uaccess.h>
#include <linux/splice.h>
#include <linux/security.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include "overlayfs.h"

#define KSU_OVL_IOCB_MASK (IOCB_DSYNC | IOCB_HIPRI | IOCB_NOWAIT | IOCB_SYNC)

struct ksu_ovl_aio_req {
	struct kiocb iocb;
	refcount_t ref;
	struct kiocb *orig_iocb;
	struct fd fd;
};

static struct kmem_cache *ksu_ovl_aio_request_cachep;

static char ksu_ovl_whatisit(struct inode *inode, struct inode *realinode)
{
	if (realinode != ksu_ovl_inode_upper(inode))
		return 'l';
	if (ksu_ovl_has_upperdata(inode))
		return 'u';
	else
		return 'm';
}

/* No atime modificaton nor notify on underlying */
#define KSU_OVL_OPEN_FLAGS (O_NOATIME | FMODE_NONOTIFY)

static struct file *ksu_ovl_open_realfile(const struct file *file,
				      struct inode *realinode)
{
	struct inode *inode = file_inode(file);
	struct file *realfile;
	const struct cred *old_cred;
	int flags = file->f_flags | KSU_OVL_OPEN_FLAGS;
	int acc_mode = ACC_MODE(flags);
	int err;

	if (flags & O_APPEND)
		acc_mode |= MAY_APPEND;

	old_cred = ksu_ovl_override_creds(inode->i_sb);
	err = inode_permission(realinode, MAY_OPEN | acc_mode);
	if (err) {
		realfile = ERR_PTR(err);
	} else if (old_cred && !inode_owner_or_capable(realinode)) {
		realfile = ERR_PTR(-EPERM);
	} else {
		realfile = open_with_fake_path(&file->f_path, flags, realinode,
					       current_cred());
	}
	ksu_ovl_revert_creds(inode->i_sb, old_cred);

	pr_debug("open(%p[%pD2/%c], 0%o) -> (%p, 0%o)\n",
		 file, file, ksu_ovl_whatisit(inode, realinode), file->f_flags,
		 realfile, IS_ERR(realfile) ? 0 : realfile->f_flags);

	return realfile;
}

#define KSU_OVL_SETFL_MASK (O_APPEND | O_NONBLOCK | O_NDELAY | O_DIRECT)

static int ksu_ovl_change_flags(struct file *file, unsigned int flags)
{
	struct inode *inode = file_inode(file);
	int err;

	flags |= KSU_OVL_OPEN_FLAGS;

	/* If some flag changed that cannot be changed then something's amiss */
	if (WARN_ON((file->f_flags ^ flags) & ~KSU_OVL_SETFL_MASK))
		return -EIO;

	flags &= KSU_OVL_SETFL_MASK;

	if (((flags ^ file->f_flags) & O_APPEND) && IS_APPEND(inode))
		return -EPERM;

	if (flags & O_DIRECT) {
		if (!file->f_mapping->a_ops ||
		    !file->f_mapping->a_ops->direct_IO)
			return -EINVAL;
	}

	if (file->f_op->check_flags) {
		err = file->f_op->check_flags(flags);
		if (err)
			return err;
	}

	spin_lock(&file->f_lock);
	file->f_flags = (file->f_flags & ~KSU_OVL_SETFL_MASK) | flags;
	spin_unlock(&file->f_lock);

	return 0;
}

static int ksu_ovl_real_fdget_meta(const struct file *file, struct fd *real,
			       bool allow_meta)
{
	struct inode *inode = file_inode(file);
	struct inode *realinode;

	real->flags = 0;
	real->file = file->private_data;

	if (allow_meta)
		realinode = ksu_ovl_inode_real(inode);
	else
		realinode = ksu_ovl_inode_realdata(inode);

	/* Has it been copied up since we'd opened it? */
	if (unlikely(file_inode(real->file) != realinode)) {
		real->flags = FDPUT_FPUT;
		real->file = ksu_ovl_open_realfile(file, realinode);

		return PTR_ERR_OR_ZERO(real->file);
	}

	/* Did the flags change since open? */
	if (unlikely((file->f_flags ^ real->file->f_flags) & ~KSU_OVL_OPEN_FLAGS))
		return ksu_ovl_change_flags(real->file, file->f_flags);

	return 0;
}

static int ksu_ovl_real_fdget(const struct file *file, struct fd *real)
{
	if (d_is_dir(file_dentry(file))) {
		real->flags = 0;
		real->file = ksu_ovl_dir_real_file(file, false);

		return PTR_ERR_OR_ZERO(real->file);
	}

	return ksu_ovl_real_fdget_meta(file, real, false);
}

static int ksu_ovl_open(struct inode *inode, struct file *file)
{
	struct file *realfile;
	int err;

	err = ksu_ovl_maybe_copy_up(file_dentry(file), file->f_flags);
	if (err)
		return err;

	/* No longer need these flags, so don't pass them on to underlying fs */
	file->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);

	realfile = ksu_ovl_open_realfile(file, ksu_ovl_inode_realdata(inode));
	if (IS_ERR(realfile))
		return PTR_ERR(realfile);

	file->private_data = realfile;

	return 0;
}

static int ksu_ovl_release(struct inode *inode, struct file *file)
{
	fput(file->private_data);

	return 0;
}

static loff_t ksu_ovl_llseek(struct file *file, loff_t offset, int whence)
{
	struct inode *inode = file_inode(file);
	struct fd real;
	const struct cred *old_cred;
	loff_t ret;

	/*
	 * The two special cases below do not need to involve real fs,
	 * so we can optimizing concurrent callers.
	 */
	if (offset == 0) {
		if (whence == SEEK_CUR)
			return file->f_pos;

		if (whence == SEEK_SET)
			return vfs_setpos(file, 0, 0);
	}

	ret = ksu_ovl_real_fdget(file, &real);
	if (ret)
		return ret;

	/*
	 * Overlay file f_pos is the master copy that is preserved
	 * through copy up and modified on read/write, but only real
	 * fs knows how to SEEK_HOLE/SEEK_DATA and real fs may impose
	 * limitations that are more strict than ->s_maxbytes for specific
	 * files, so we use the real file to perform seeks.
	 */
	ksu_ovl_inode_lock(inode);
	real.file->f_pos = file->f_pos;

	old_cred = ksu_ovl_override_creds(inode->i_sb);
	ret = vfs_llseek(real.file, offset, whence);
	ksu_ovl_revert_creds(inode->i_sb, old_cred);

	file->f_pos = real.file->f_pos;
	ksu_ovl_inode_unlock(inode);

	fdput(real);

	return ret;
}

static void ksu_ovl_file_accessed(struct file *file)
{
	struct inode *inode, *upperinode;

	if (file->f_flags & O_NOATIME)
		return;

	inode = file_inode(file);
	upperinode = ksu_ovl_inode_upper(inode);

	if (!upperinode)
		return;

	if ((!timespec64_equal(&inode->i_mtime, &upperinode->i_mtime) ||
	     !timespec64_equal(&inode->i_ctime, &upperinode->i_ctime))) {
		inode->i_mtime = upperinode->i_mtime;
		inode->i_ctime = upperinode->i_ctime;
	}

	touch_atime(&file->f_path);
}

static inline void ksu_ovl_aio_put(struct ksu_ovl_aio_req *aio_req)
{
	if (refcount_dec_and_test(&aio_req->ref)) {
		fdput(aio_req->fd);
		kmem_cache_free(ksu_ovl_aio_request_cachep, aio_req);
	}
}

static void ksu_ovl_aio_cleanup_handler(struct ksu_ovl_aio_req *aio_req)
{
	struct kiocb *iocb = &aio_req->iocb;
	struct kiocb *orig_iocb = aio_req->orig_iocb;

	if (iocb->ki_flags & IOCB_WRITE) {
		struct inode *inode = file_inode(orig_iocb->ki_filp);

		/* Actually acquired in ksu_ovl_write_iter() */
		__sb_writers_acquired(file_inode(iocb->ki_filp)->i_sb,
				      SB_FREEZE_WRITE);
		file_end_write(iocb->ki_filp);
		ksu_ovl_copyattr(ksu_ovl_inode_real(inode), inode);
	}

	orig_iocb->ki_pos = iocb->ki_pos;
	ksu_ovl_aio_put(aio_req);
}

static void ksu_ovl_aio_rw_complete(struct kiocb *iocb, long res, long res2)
{
	struct ksu_ovl_aio_req *aio_req = container_of(iocb,
						   struct ksu_ovl_aio_req, iocb);
	struct kiocb *orig_iocb = aio_req->orig_iocb;

	ksu_ovl_aio_cleanup_handler(aio_req);
	orig_iocb->ki_complete(orig_iocb, res, res2);
}

static ssize_t ksu_ovl_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *file = iocb->ki_filp;
	struct fd real;
	const struct cred *old_cred;
	ssize_t ret;

	if (!iov_iter_count(iter))
		return 0;

	ret = ksu_ovl_real_fdget(file, &real);
	if (ret)
		return ret;

	ret = -EINVAL;
	if (iocb->ki_flags & IOCB_DIRECT &&
	    (!real.file->f_mapping->a_ops ||
	     !real.file->f_mapping->a_ops->direct_IO))
		goto out_fdput;

	old_cred = ksu_ovl_override_creds(file_inode(file)->i_sb);
	if (is_sync_kiocb(iocb)) {
		ret = vfs_iter_read(real.file, iter, &iocb->ki_pos,
				    iocb_to_rw_flags(iocb->ki_flags,
						     KSU_OVL_IOCB_MASK));
	} else {
		struct ksu_ovl_aio_req *aio_req;

		ret = -ENOMEM;
		aio_req = kmem_cache_zalloc(ksu_ovl_aio_request_cachep, GFP_KERNEL);
		if (!aio_req)
			goto out;

		aio_req->fd = real;
		real.flags = 0;
		aio_req->orig_iocb = iocb;
		kiocb_clone(&aio_req->iocb, iocb, real.file);
		aio_req->iocb.ki_complete = ksu_ovl_aio_rw_complete;
		refcount_set(&aio_req->ref, 2);
		ret = vfs_iocb_iter_read(real.file, &aio_req->iocb, iter);
		ksu_ovl_aio_put(aio_req);
		if (ret != -EIOCBQUEUED)
			ksu_ovl_aio_cleanup_handler(aio_req);
	}
out:
	ksu_ovl_revert_creds(file_inode(file)->i_sb, old_cred);

	ksu_ovl_file_accessed(file);
out_fdput:
	fdput(real);

	return ret;
}

static ssize_t ksu_ovl_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file_inode(file);
	struct fd real;
	const struct cred *old_cred;
	ssize_t ret;
	int ifl = iocb->ki_flags;

	if (!iov_iter_count(iter))
		return 0;

	inode_lock(inode);
	/* Update mode */
	ksu_ovl_copyattr(ksu_ovl_inode_real(inode), inode);
	ret = file_remove_privs(file);
	if (ret)
		goto out_unlock;

	ret = ksu_ovl_real_fdget(file, &real);
	if (ret)
		goto out_unlock;

	ret = -EINVAL;
	if (iocb->ki_flags & IOCB_DIRECT &&
	    (!real.file->f_mapping->a_ops ||
	     !real.file->f_mapping->a_ops->direct_IO))
		goto out_fdput;

	if (!ksu_ovl_should_sync(KSU_OVL_FS(inode->i_sb)))
		ifl &= ~(IOCB_DSYNC | IOCB_SYNC);

	old_cred = ksu_ovl_override_creds(file_inode(file)->i_sb);
	if (is_sync_kiocb(iocb)) {
		file_start_write(real.file);
		ret = vfs_iter_write(real.file, iter, &iocb->ki_pos,
				     iocb_to_rw_flags(ifl, KSU_OVL_IOCB_MASK));
		file_end_write(real.file);
		/* Update size */
		ksu_ovl_copyattr(ksu_ovl_inode_real(inode), inode);
	} else {
		struct ksu_ovl_aio_req *aio_req;

		ret = -ENOMEM;
		aio_req = kmem_cache_zalloc(ksu_ovl_aio_request_cachep, GFP_KERNEL);
		if (!aio_req)
			goto out;

		file_start_write(real.file);
		/* Pacify lockdep, same trick as done in aio_write() */
		__sb_writers_release(file_inode(real.file)->i_sb,
				     SB_FREEZE_WRITE);
		aio_req->fd = real;
		real.flags = 0;
		aio_req->orig_iocb = iocb;
		kiocb_clone(&aio_req->iocb, iocb, real.file);
		aio_req->iocb.ki_flags = ifl;
		aio_req->iocb.ki_complete = ksu_ovl_aio_rw_complete;
		refcount_set(&aio_req->ref, 2);
		ret = vfs_iocb_iter_write(real.file, &aio_req->iocb, iter);
		ksu_ovl_aio_put(aio_req);
		if (ret != -EIOCBQUEUED)
			ksu_ovl_aio_cleanup_handler(aio_req);
	}
out:
	ksu_ovl_revert_creds(file_inode(file)->i_sb, old_cred);
out_fdput:
	fdput(real);

out_unlock:
	inode_unlock(inode);

	return ret;
}

/*
 * Calling iter_file_splice_write() directly from overlay's f_op may deadlock
 * due to lock order inversion between pipe->mutex in iter_file_splice_write()
 * and file_start_write(real.file) in ksu_ovl_write_iter().
 *
 * So do everything ksu_ovl_write_iter() does and call iter_file_splice_write() on
 * the real file.
 */
static ssize_t ksu_ovl_splice_write(struct pipe_inode_info *pipe, struct file *out,
				loff_t *ppos, size_t len, unsigned int flags)
{
	struct fd real;
	const struct cred *old_cred;
	struct inode *inode = file_inode(out);
	struct inode *realinode = ksu_ovl_inode_real(inode);
	ssize_t ret;

	inode_lock(inode);
	/* Update mode */
	ksu_ovl_copyattr(realinode, inode);
	ret = file_remove_privs(out);
	if (ret)
		goto out_unlock;

	ret = ksu_ovl_real_fdget(out, &real);
	if (ret)
		goto out_unlock;

	old_cred = ksu_ovl_override_creds(inode->i_sb);
	file_start_write(real.file);

	ret = iter_file_splice_write(pipe, real.file, ppos, len, flags);

	file_end_write(real.file);
	/* Update size */
	ksu_ovl_copyattr(realinode, inode);
	ksu_ovl_revert_creds(inode->i_sb, old_cred);
	fdput(real);

out_unlock:
	inode_unlock(inode);

	return ret;
}

static int ksu_ovl_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	struct fd real;
	const struct cred *old_cred;
	int ret;

	ret = ksu_ovl_sync_status(KSU_OVL_FS(file_inode(file)->i_sb));
	if (ret <= 0)
		return ret;

	ret = ksu_ovl_real_fdget_meta(file, &real, !datasync);
	if (ret)
		return ret;

	/* Don't sync lower file for fear of receiving EROFS error */
	if (file_inode(real.file) == ksu_ovl_inode_upper(file_inode(file))) {
		old_cred = ksu_ovl_override_creds(file_inode(file)->i_sb);
		ret = vfs_fsync_range(real.file, start, end, datasync);
		ksu_ovl_revert_creds(file_inode(file)->i_sb, old_cred);
	}

	fdput(real);

	return ret;
}

static int ksu_ovl_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct file *realfile = file->private_data;
	const struct cred *old_cred;
	int ret;

	if (!realfile->f_op->mmap)
		return -ENODEV;

	if (WARN_ON(file != vma->vm_file))
		return -EIO;

	vma->vm_file = get_file(realfile);

	old_cred = ksu_ovl_override_creds(file_inode(file)->i_sb);
	ret = call_mmap(vma->vm_file, vma);
	ksu_ovl_revert_creds(file_inode(file)->i_sb, old_cred);

	if (ret) {
		/* Drop reference count from new vm_file value */
		fput(realfile);
	} else {
		/* Drop reference count from previous vm_file value */
		fput(file);
	}

	ksu_ovl_file_accessed(file);

	return ret;
}

static long ksu_ovl_fallocate(struct file *file, int mode, loff_t offset, loff_t len)
{
	struct inode *inode = file_inode(file);
	struct fd real;
	const struct cred *old_cred;
	int ret;

	inode_lock(inode);
	/* Update mode */
	ksu_ovl_copyattr(ksu_ovl_inode_real(inode), inode);
	ret = file_remove_privs(file);
	if (ret)
		goto out_unlock;

	ret = ksu_ovl_real_fdget(file, &real);
	if (ret)
		goto out_unlock;

	old_cred = ksu_ovl_override_creds(file_inode(file)->i_sb);
	ret = vfs_fallocate(real.file, mode, offset, len);
	ksu_ovl_revert_creds(file_inode(file)->i_sb, old_cred);

	/* Update size */
	ksu_ovl_copyattr(ksu_ovl_inode_real(inode), inode);

	fdput(real);

out_unlock:
	inode_unlock(inode);

	return ret;
}

static int ksu_ovl_fadvise(struct file *file, loff_t offset, loff_t len, int advice)
{
	struct fd real;
	const struct cred *old_cred;
	int ret;

	ret = ksu_ovl_real_fdget(file, &real);
	if (ret)
		return ret;

	old_cred = ksu_ovl_override_creds(file_inode(file)->i_sb);
	ret = vfs_fadvise(real.file, offset, len, advice);
	ksu_ovl_revert_creds(file_inode(file)->i_sb, old_cred);

	fdput(real);

	return ret;
}

static long ksu_ovl_real_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	struct fd real;
	long ret;

	ret = ksu_ovl_real_fdget(file, &real);
	if (ret)
		return ret;

	ret = security_file_ioctl(real.file, cmd, arg);
	if (!ret) {
		/*
		 * Don't override creds, since we currently can't safely check
		 * permissions before doing so.
		 */
		ret = vfs_ioctl(real.file, cmd, arg);
	}

	fdput(real);

	return ret;
}

static long ksu_ovl_ioctl_set_flags(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long ret;
	struct inode *inode = file_inode(file);

	if (!inode_owner_or_capable(inode))
		return -EACCES;

	ret = mnt_want_write_file(file);
	if (ret)
		return ret;

	inode_lock(inode);

	/*
	 * Prevent copy up if immutable and has no CAP_LINUX_IMMUTABLE
	 * capability.
	 */
	ret = -EPERM;
	if (!ksu_ovl_has_upperdata(inode) && IS_IMMUTABLE(inode) &&
	    !capable(CAP_LINUX_IMMUTABLE))
		goto unlock;

	ret = ksu_ovl_maybe_copy_up(file_dentry(file), O_WRONLY);
	if (ret)
		goto unlock;

	ret = ksu_ovl_real_ioctl(file, cmd, arg);

	ksu_ovl_copyflags(ksu_ovl_inode_real(inode), inode);
unlock:
	inode_unlock(inode);

	mnt_drop_write_file(file);

	return ret;

}

long ksu_ovl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case FS_IOC_GETFLAGS:
	case FS_IOC_FSGETXATTR:
		ret = ksu_ovl_real_ioctl(file, cmd, arg);
		break;

	case FS_IOC_FSSETXATTR:
	case FS_IOC_SETFLAGS:
		ret = ksu_ovl_ioctl_set_flags(file, cmd, arg);
		break;

	default:
		ret = -ENOTTY;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
long ksu_ovl_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FS_IOC32_GETFLAGS:
		cmd = FS_IOC_GETFLAGS;
		break;

	case FS_IOC32_SETFLAGS:
		cmd = FS_IOC_SETFLAGS;
		break;

	default:
		return -ENOIOCTLCMD;
	}

	return ksu_ovl_ioctl(file, cmd, arg);
}
#endif

enum ksu_ovl_copyop {
	KSU_OVL_COPY,
	KSU_OVL_CLONE,
	KSU_OVL_DEDUPE,
};

static loff_t ksu_ovl_copyfile(struct file *file_in, loff_t pos_in,
			    struct file *file_out, loff_t pos_out,
			    loff_t len, unsigned int flags, enum ksu_ovl_copyop op)
{
	struct inode *inode_out = file_inode(file_out);
	struct fd real_in, real_out;
	const struct cred *old_cred;
	loff_t ret;

	inode_lock(inode_out);
	if (op != KSU_OVL_DEDUPE) {
		/* Update mode */
		ksu_ovl_copyattr(ksu_ovl_inode_real(inode_out), inode_out);
		ret = file_remove_privs(file_out);
		if (ret)
			goto out_unlock;
	}

	ret = ksu_ovl_real_fdget(file_out, &real_out);
	if (ret)
		goto out_unlock;

	ret = ksu_ovl_real_fdget(file_in, &real_in);
	if (ret) {
		fdput(real_out);
		goto out_unlock;
	}

	old_cred = ksu_ovl_override_creds(file_inode(file_out)->i_sb);
	switch (op) {
	case KSU_OVL_COPY:
		ret = vfs_copy_file_range(real_in.file, pos_in,
					  real_out.file, pos_out, len, flags);
		break;

	case KSU_OVL_CLONE:
		ret = vfs_clone_file_range(real_in.file, pos_in,
					   real_out.file, pos_out, len, flags);
		break;

	case KSU_OVL_DEDUPE:
		ret = vfs_dedupe_file_range_one(real_in.file, pos_in,
						real_out.file, pos_out, len,
						flags);
		break;
	}
	ksu_ovl_revert_creds(file_inode(file_out)->i_sb, old_cred);

	/* Update size */
	ksu_ovl_copyattr(ksu_ovl_inode_real(inode_out), inode_out);

	fdput(real_in);
	fdput(real_out);

out_unlock:
	inode_unlock(inode_out);

	return ret;
}

static ssize_t ksu_ovl_copy_file_range(struct file *file_in, loff_t pos_in,
				   struct file *file_out, loff_t pos_out,
				   size_t len, unsigned int flags)
{
	return ksu_ovl_copyfile(file_in, pos_in, file_out, pos_out, len, flags,
			    KSU_OVL_COPY);
}

static loff_t ksu_ovl_remap_file_range(struct file *file_in, loff_t pos_in,
				   struct file *file_out, loff_t pos_out,
				   loff_t len, unsigned int remap_flags)
{
	enum ksu_ovl_copyop op;

	if (remap_flags & ~(REMAP_FILE_DEDUP | REMAP_FILE_ADVISORY))
		return -EINVAL;

	if (remap_flags & REMAP_FILE_DEDUP)
		op = KSU_OVL_DEDUPE;
	else
		op = KSU_OVL_CLONE;

	/*
	 * Don't copy up because of a dedupe request, this wouldn't make sense
	 * most of the time (data would be duplicated instead of deduplicated).
	 */
	if (op == KSU_OVL_DEDUPE &&
	    (!ksu_ovl_inode_upper(file_inode(file_in)) ||
	     !ksu_ovl_inode_upper(file_inode(file_out))))
		return -EPERM;

	return ksu_ovl_copyfile(file_in, pos_in, file_out, pos_out, len,
			    remap_flags, op);
}

const struct file_operations ksu_ovl_file_operations = {
	.open		= ksu_ovl_open,
	.release	= ksu_ovl_release,
	.llseek		= ksu_ovl_llseek,
	.read_iter	= ksu_ovl_read_iter,
	.write_iter	= ksu_ovl_write_iter,
	.fsync		= ksu_ovl_fsync,
	.mmap		= ksu_ovl_mmap,
	.fallocate	= ksu_ovl_fallocate,
	.fadvise	= ksu_ovl_fadvise,
	.unlocked_ioctl	= ksu_ovl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ksu_ovl_compat_ioctl,
#endif
	.splice_read    = generic_file_splice_read,
	.splice_write   = ksu_ovl_splice_write,

	.copy_file_range	= ksu_ovl_copy_file_range,
	.remap_file_range	= ksu_ovl_remap_file_range,
};

int __init ksu_ovl_aio_request_cache_init(void)
{
	ksu_ovl_aio_request_cachep = kmem_cache_create("ksu_ovl_aio_req",
						   sizeof(struct ksu_ovl_aio_req),
						   0, SLAB_HWCACHE_ALIGN, NULL);
	if (!ksu_ovl_aio_request_cachep)
		return -ENOMEM;

	return 0;
}

void ksu_ovl_aio_request_cache_destroy(void)
{
	kmem_cache_destroy(ksu_ovl_aio_request_cachep);
}
