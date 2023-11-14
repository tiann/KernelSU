// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * Copyright (C) 2011 Novell Inc.
 */

#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/xattr.h>
#include <linux/security.h>
#include <linux/cred.h>
#include <linux/module.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>
#include <linux/atomic.h>
#include <linux/ratelimit.h>
#include "overlayfs.h"

static unsigned short ksu_ovl_redirect_max = 256;
module_param_named(redirect_max, ksu_ovl_redirect_max, ushort, 0644);
MODULE_PARM_DESC(redirect_max,
		 "Maximum length of absolute redirect xattr value");

static int ksu_ovl_set_redirect(struct dentry *dentry, bool samedir);

int ksu_ovl_cleanup(struct inode *wdir, struct dentry *wdentry)
{
	int err;

	dget(wdentry);
	if (d_is_dir(wdentry))
		err = ksu_ovl_do_rmdir(wdir, wdentry);
	else
		err = ksu_ovl_do_unlink(wdir, wdentry);
	dput(wdentry);

	if (err) {
		pr_err("cleanup of '%pd2' failed (%i)\n",
		       wdentry, err);
	}

	return err;
}

struct dentry *ksu_ovl_lookup_temp(struct dentry *workdir)
{
	struct dentry *temp;
	char name[20];
	static atomic_t temp_id = ATOMIC_INIT(0);

	/* counter is allowed to wrap, since temp dentries are ephemeral */
	snprintf(name, sizeof(name), "#%x", atomic_inc_return(&temp_id));

	temp = lookup_one_len(name, workdir, strlen(name));
	if (!IS_ERR(temp) && temp->d_inode) {
		pr_err("workdir/%s already exists\n", name);
		dput(temp);
		temp = ERR_PTR(-EIO);
	}

	return temp;
}

/* caller holds i_mutex on workdir */
static struct dentry *ksu_ovl_whiteout(struct ksu_ovl_fs *ofs)
{
	int err;
	struct dentry *whiteout;
	struct dentry *workdir = ofs->workdir;
	struct inode *wdir = workdir->d_inode;

	if (!ofs->whiteout) {
		whiteout = ksu_ovl_lookup_temp(workdir);
		if (IS_ERR(whiteout))
			goto out;

		err = ksu_ovl_do_whiteout(wdir, whiteout);
		if (err) {
			dput(whiteout);
			whiteout = ERR_PTR(err);
			goto out;
		}
		ofs->whiteout = whiteout;
	}

	if (ofs->share_whiteout) {
		whiteout = ksu_ovl_lookup_temp(workdir);
		if (IS_ERR(whiteout))
			goto out;

		err = ksu_ovl_do_link(ofs->whiteout, wdir, whiteout);
		if (!err)
			goto out;

		if (err != -EMLINK) {
			pr_warn("Failed to link whiteout - disabling whiteout inode sharing(nlink=%u, err=%i)\n",
				ofs->whiteout->d_inode->i_nlink, err);
			ofs->share_whiteout = false;
		}
		dput(whiteout);
	}
	whiteout = ofs->whiteout;
	ofs->whiteout = NULL;
out:
	return whiteout;
}

/* Caller must hold i_mutex on both workdir and dir */
int ksu_ovl_cleanup_and_whiteout(struct ksu_ovl_fs *ofs, struct inode *dir,
			     struct dentry *dentry)
{
	struct inode *wdir = ofs->workdir->d_inode;
	struct dentry *whiteout;
	int err;
	int flags = 0;

	whiteout = ksu_ovl_whiteout(ofs);
	err = PTR_ERR(whiteout);
	if (IS_ERR(whiteout))
		return err;

	if (d_is_dir(dentry))
		flags = RENAME_EXCHANGE;

	err = ksu_ovl_do_rename(wdir, whiteout, dir, dentry, flags);
	if (err)
		goto kill_whiteout;
	if (flags)
		ksu_ovl_cleanup(wdir, dentry);

out:
	dput(whiteout);
	return err;

kill_whiteout:
	ksu_ovl_cleanup(wdir, whiteout);
	goto out;
}

int ksu_ovl_mkdir_real(struct inode *dir, struct dentry **newdentry, umode_t mode)
{
	int err;
	struct dentry *d, *dentry = *newdentry;

	err = ksu_ovl_do_mkdir(dir, dentry, mode);
	if (err)
		return err;

	if (likely(!d_unhashed(dentry)))
		return 0;

	/*
	 * vfs_mkdir() may succeed and leave the dentry passed
	 * to it unhashed and negative. If that happens, try to
	 * lookup a new hashed and positive dentry.
	 */
	d = lookup_one_len(dentry->d_name.name, dentry->d_parent,
			   dentry->d_name.len);
	if (IS_ERR(d)) {
		pr_warn("failed lookup after mkdir (%pd2, err=%i).\n",
			dentry, err);
		return PTR_ERR(d);
	}
	dput(dentry);
	*newdentry = d;

	return 0;
}

struct dentry *ksu_ovl_create_real(struct inode *dir, struct dentry *newdentry,
			       struct ksu_ovl_cattr *attr)
{
	int err;

	if (IS_ERR(newdentry))
		return newdentry;

	err = -ESTALE;
	if (newdentry->d_inode)
		goto out;

	if (attr->hardlink) {
		err = ksu_ovl_do_link(attr->hardlink, dir, newdentry);
	} else {
		switch (attr->mode & S_IFMT) {
		case S_IFREG:
			err = ksu_ovl_do_create(dir, newdentry, attr->mode);
			break;

		case S_IFDIR:
			/* mkdir is special... */
			err =  ksu_ovl_mkdir_real(dir, &newdentry, attr->mode);
			break;

		case S_IFCHR:
		case S_IFBLK:
		case S_IFIFO:
		case S_IFSOCK:
			err = ksu_ovl_do_mknod(dir, newdentry, attr->mode,
					   attr->rdev);
			break;

		case S_IFLNK:
			err = ksu_ovl_do_symlink(dir, newdentry, attr->link);
			break;

		default:
			err = -EPERM;
		}
	}
	if (!err && WARN_ON(!newdentry->d_inode)) {
		/*
		 * Not quite sure if non-instantiated dentry is legal or not.
		 * VFS doesn't seem to care so check and warn here.
		 */
		err = -EIO;
	}
out:
	if (err) {
		dput(newdentry);
		return ERR_PTR(err);
	}
	return newdentry;
}

struct dentry *ksu_ovl_create_temp(struct dentry *workdir, struct ksu_ovl_cattr *attr)
{
	return ksu_ovl_create_real(d_inode(workdir), ksu_ovl_lookup_temp(workdir),
			       attr);
}

static int ksu_ovl_set_opaque_xerr(struct dentry *dentry, struct dentry *upper,
			       int xerr)
{
	int err;

	err = ksu_ovl_check_setxattr(dentry, upper, KSU_OVL_XATTR_OPAQUE, "y", 1, xerr);
	if (!err)
		ksu_ovl_dentry_set_opaque(dentry);

	return err;
}

static int ksu_ovl_set_opaque(struct dentry *dentry, struct dentry *upperdentry)
{
	/*
	 * Fail with -EIO when trying to create opaque dir and upper doesn't
	 * support xattrs. ksu_ovl_rename() calls ksu_ovl_set_opaque_xerr(-EXDEV) to
	 * return a specific error for noxattr case.
	 */
	return ksu_ovl_set_opaque_xerr(dentry, upperdentry, -EIO);
}

/*
 * Common operations required to be done after creation of file on upper.
 * If @hardlink is false, then @inode is a pre-allocated inode, we may or
 * may not use to instantiate the new dentry.
 */
static int ksu_ovl_instantiate(struct dentry *dentry, struct inode *inode,
			   struct dentry *newdentry, bool hardlink)
{
	struct ksu_ovl_inode_params oip = {
		.upperdentry = newdentry,
		.newinode = inode,
	};

	ksu_ovl_dir_modified(dentry->d_parent, false);
	ksu_ovl_dentry_set_upper_alias(dentry);
	ksu_ovl_dentry_update_reval(dentry, newdentry,
			DCACHE_OP_REVALIDATE | DCACHE_OP_WEAK_REVALIDATE);

	if (!hardlink) {
		/*
		 * ksu_ovl_obtain_alias() can be called after ksu_ovl_create_real()
		 * and before we get here, so we may get an inode from cache
		 * with the same real upperdentry that is not the inode we
		 * pre-allocated.  In this case we will use the cached inode
		 * to instantiate the new dentry.
		 *
		 * XXX: if we ever use ksu_ovl_obtain_alias() to decode directory
		 * file handles, need to use ksu_ovl_get_inode_locked() and
		 * d_instantiate_new() here to prevent from creating two
		 * hashed directory inode aliases.
		 */
		inode = ksu_ovl_get_inode(dentry->d_sb, &oip);
		if (IS_ERR(inode))
			return PTR_ERR(inode);
		if (inode == oip.newinode)
			ksu_ovl_set_flag(KSU_OVL_UPPERDATA, inode);
	} else {
		WARN_ON(ksu_ovl_inode_real(inode) != d_inode(newdentry));
		dput(newdentry);
		inc_nlink(inode);
	}

	d_instantiate(dentry, inode);
	if (inode != oip.newinode) {
		pr_warn_ratelimited("newly created inode found in cache (%pd2)\n",
				    dentry);
	}

	/* Force lookup of new upper hardlink to find its lower */
	if (hardlink)
		d_drop(dentry);

	return 0;
}

static bool ksu_ovl_type_merge(struct dentry *dentry)
{
	return KSU_OVL_TYPE_MERGE(ksu_ovl_path_type(dentry));
}

static bool ksu_ovl_type_origin(struct dentry *dentry)
{
	return KSU_OVL_TYPE_ORIGIN(ksu_ovl_path_type(dentry));
}

static int ksu_ovl_create_upper(struct dentry *dentry, struct inode *inode,
			    struct ksu_ovl_cattr *attr)
{
	struct dentry *upperdir = ksu_ovl_dentry_upper(dentry->d_parent);
	struct inode *udir = upperdir->d_inode;
	struct dentry *newdentry;
	int err;

	if (!attr->hardlink && !IS_POSIXACL(udir))
		attr->mode &= ~current_umask();

	inode_lock_nested(udir, I_MUTEX_PARENT);
	newdentry = ksu_ovl_create_real(udir,
				    lookup_one_len(dentry->d_name.name,
						   upperdir,
						   dentry->d_name.len),
				    attr);
	err = PTR_ERR(newdentry);
	if (IS_ERR(newdentry))
		goto out_unlock;

	if (ksu_ovl_type_merge(dentry->d_parent) && d_is_dir(newdentry)) {
		/* Setting opaque here is just an optimization, allow to fail */
		ksu_ovl_set_opaque(dentry, newdentry);
	}

	err = ksu_ovl_instantiate(dentry, inode, newdentry, !!attr->hardlink);
	if (err)
		goto out_cleanup;
out_unlock:
	inode_unlock(udir);
	return err;

out_cleanup:
	ksu_ovl_cleanup(udir, newdentry);
	dput(newdentry);
	goto out_unlock;
}

static struct dentry *ksu_ovl_clear_empty(struct dentry *dentry,
				      struct list_head *list)
{
	struct dentry *workdir = ksu_ovl_workdir(dentry);
	struct inode *wdir = workdir->d_inode;
	struct dentry *upperdir = ksu_ovl_dentry_upper(dentry->d_parent);
	struct inode *udir = upperdir->d_inode;
	struct path upperpath;
	struct dentry *upper;
	struct dentry *opaquedir;
	struct kstat stat;
	int err;

	if (WARN_ON(!workdir))
		return ERR_PTR(-EROFS);

	err = ksu_ovl_lock_rename_workdir(workdir, upperdir);
	if (err)
		goto out;

	ksu_ovl_path_upper(dentry, &upperpath);
	err = vfs_getattr(&upperpath, &stat,
			  STATX_BASIC_STATS, AT_STATX_SYNC_AS_STAT);
	if (err)
		goto out_unlock;

	err = -ESTALE;
	if (!S_ISDIR(stat.mode))
		goto out_unlock;
	upper = upperpath.dentry;
	if (upper->d_parent->d_inode != udir)
		goto out_unlock;

	opaquedir = ksu_ovl_create_temp(workdir, KSU_OVL_CATTR(stat.mode));
	err = PTR_ERR(opaquedir);
	if (IS_ERR(opaquedir))
		goto out_unlock;

	err = ksu_ovl_copy_xattr(dentry->d_sb, upper, opaquedir);
	if (err)
		goto out_cleanup;

	err = ksu_ovl_set_opaque(dentry, opaquedir);
	if (err)
		goto out_cleanup;

	inode_lock(opaquedir->d_inode);
	err = ksu_ovl_set_attr(opaquedir, &stat);
	inode_unlock(opaquedir->d_inode);
	if (err)
		goto out_cleanup;

	err = ksu_ovl_do_rename(wdir, opaquedir, udir, upper, RENAME_EXCHANGE);
	if (err)
		goto out_cleanup;

	ksu_ovl_cleanup_whiteouts(upper, list);
	ksu_ovl_cleanup(wdir, upper);
	unlock_rename(workdir, upperdir);

	/* dentry's upper doesn't match now, get rid of it */
	d_drop(dentry);

	return opaquedir;

out_cleanup:
	ksu_ovl_cleanup(wdir, opaquedir);
	dput(opaquedir);
out_unlock:
	unlock_rename(workdir, upperdir);
out:
	return ERR_PTR(err);
}

static int ksu_ovl_set_upper_acl(struct dentry *upperdentry, const char *name,
			     const struct posix_acl *acl)
{
	void *buffer;
	size_t size;
	int err;

	if (!IS_ENABLED(CONFIG_FS_POSIX_ACL) || !acl)
		return 0;

	size = posix_acl_xattr_size(acl->a_count);
	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	err = posix_acl_to_xattr(&init_user_ns, acl, buffer, size);
	if (err < 0)
		goto out_free;

	err = vfs_setxattr(upperdentry, name, buffer, size, XATTR_CREATE);
out_free:
	kfree(buffer);
	return err;
}

static int ksu_ovl_create_over_whiteout(struct dentry *dentry, struct inode *inode,
				    struct ksu_ovl_cattr *cattr)
{
	struct dentry *workdir = ksu_ovl_workdir(dentry);
	struct inode *wdir = workdir->d_inode;
	struct dentry *upperdir = ksu_ovl_dentry_upper(dentry->d_parent);
	struct inode *udir = upperdir->d_inode;
	struct dentry *upper;
	struct dentry *newdentry;
	int err;
	struct posix_acl *acl, *default_acl;
	bool hardlink = !!cattr->hardlink;

	if (WARN_ON(!workdir))
		return -EROFS;

	if (!hardlink) {
		err = posix_acl_create(dentry->d_parent->d_inode,
				       &cattr->mode, &default_acl, &acl);
		if (err)
			return err;
	}

	err = ksu_ovl_lock_rename_workdir(workdir, upperdir);
	if (err)
		goto out;

	upper = lookup_one_len(dentry->d_name.name, upperdir,
			       dentry->d_name.len);
	err = PTR_ERR(upper);
	if (IS_ERR(upper))
		goto out_unlock;

	err = -ESTALE;
	if (d_is_negative(upper) || !IS_WHITEOUT(d_inode(upper)))
		goto out_dput;

	newdentry = ksu_ovl_create_temp(workdir, cattr);
	err = PTR_ERR(newdentry);
	if (IS_ERR(newdentry))
		goto out_dput;

	/*
	 * mode could have been mutilated due to umask (e.g. sgid directory)
	 */
	if (!hardlink &&
	    !S_ISLNK(cattr->mode) &&
	    newdentry->d_inode->i_mode != cattr->mode) {
		struct iattr attr = {
			.ia_valid = ATTR_MODE,
			.ia_mode = cattr->mode,
		};
		inode_lock(newdentry->d_inode);
		err = notify_change(newdentry, &attr, NULL);
		inode_unlock(newdentry->d_inode);
		if (err)
			goto out_cleanup;
	}
	if (!hardlink) {
		err = ksu_ovl_set_upper_acl(newdentry, XATTR_NAME_POSIX_ACL_ACCESS,
					acl);
		if (err)
			goto out_cleanup;

		err = ksu_ovl_set_upper_acl(newdentry, XATTR_NAME_POSIX_ACL_DEFAULT,
					default_acl);
		if (err)
			goto out_cleanup;
	}

	if (!hardlink && S_ISDIR(cattr->mode)) {
		err = ksu_ovl_set_opaque(dentry, newdentry);
		if (err)
			goto out_cleanup;

		err = ksu_ovl_do_rename(wdir, newdentry, udir, upper,
				    RENAME_EXCHANGE);
		if (err)
			goto out_cleanup;

		ksu_ovl_cleanup(wdir, upper);
	} else {
		err = ksu_ovl_do_rename(wdir, newdentry, udir, upper, 0);
		if (err)
			goto out_cleanup;
	}
	err = ksu_ovl_instantiate(dentry, inode, newdentry, hardlink);
	if (err) {
		ksu_ovl_cleanup(udir, newdentry);
		dput(newdentry);
	}
out_dput:
	dput(upper);
out_unlock:
	unlock_rename(workdir, upperdir);
out:
	if (!hardlink) {
		posix_acl_release(acl);
		posix_acl_release(default_acl);
	}
	return err;

out_cleanup:
	ksu_ovl_cleanup(wdir, newdentry);
	dput(newdentry);
	goto out_dput;
}

static int ksu_ovl_create_or_link(struct dentry *dentry, struct inode *inode,
			      struct ksu_ovl_cattr *attr, bool origin)
{
	int err;
	const struct cred *old_cred, *hold_cred = NULL;
	struct cred *override_cred;
	struct dentry *parent = dentry->d_parent;

	err = ksu_ovl_copy_up(parent);
	if (err)
		return err;

	old_cred = ksu_ovl_override_creds(dentry->d_sb);

	/*
	 * When linking a file with copy up origin into a new parent, mark the
	 * new parent dir "impure".
	 */
	if (origin) {
		err = ksu_ovl_set_impure(parent, ksu_ovl_dentry_upper(parent));
		if (err)
			goto out_revert_creds;
	}

	if (!attr->hardlink) {
		err = -ENOMEM;
		override_cred = prepare_creds();
		if (!override_cred)
			goto out_revert_creds;
		/*
		 * In the creation cases(create, mkdir, mknod, symlink),
		 * ovl should transfer current's fs{u,g}id to underlying
		 * fs. Because underlying fs want to initialize its new
		 * inode owner using current's fs{u,g}id. And in this
		 * case, the @inode is a new inode that is initialized
		 * in inode_init_owner() to current's fs{u,g}id. So use
		 * the inode's i_{u,g}id to override the cred's fs{u,g}id.
		 *
		 * But in the other hardlink case, ksu_ovl_link() does not
		 * create a new inode, so just use the ovl mounter's
		 * fs{u,g}id.
		 */
		override_cred->fsuid = inode->i_uid;
		override_cred->fsgid = inode->i_gid;
		err = security_dentry_create_files_as(dentry,
				attr->mode, &dentry->d_name,
				old_cred ? old_cred : current_cred(),
				override_cred);
		if (err) {
			put_cred(override_cred);
			goto out_revert_creds;
		}
		hold_cred = override_creds(override_cred);
		put_cred(override_cred);
	}

	if (!ksu_ovl_dentry_is_whiteout(dentry))
		err = ksu_ovl_create_upper(dentry, inode, attr);
	else
		err = ksu_ovl_create_over_whiteout(dentry, inode, attr);

out_revert_creds:
	ksu_ovl_revert_creds(dentry->d_sb, old_cred ?: hold_cred);
	if (old_cred && hold_cred)
		put_cred(hold_cred);
	return err;
}

static int ksu_ovl_create_object(struct dentry *dentry, int mode, dev_t rdev,
			     const char *link)
{
	int err;
	struct inode *inode;
	struct ksu_ovl_cattr attr = {
		.rdev = rdev,
		.link = link,
	};

	err = ksu_ovl_want_write(dentry);
	if (err)
		goto out;

	/* Preallocate inode to be used by ksu_ovl_get_inode() */
	err = -ENOMEM;
	inode = ksu_ovl_new_inode(dentry->d_sb, mode, rdev);
	if (!inode)
		goto out_drop_write;

	spin_lock(&inode->i_lock);
	inode->i_state |= I_CREATING;
	spin_unlock(&inode->i_lock);

	inode_init_owner(inode, dentry->d_parent->d_inode, mode);
	attr.mode = inode->i_mode;

	err = ksu_ovl_create_or_link(dentry, inode, &attr, false);
	/* Did we end up using the preallocated inode? */
	if (inode != d_inode(dentry))
		iput(inode);

out_drop_write:
	ksu_ovl_drop_write(dentry);
out:
	return err;
}

static int ksu_ovl_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		      bool excl)
{
	return ksu_ovl_create_object(dentry, (mode & 07777) | S_IFREG, 0, NULL);
}

static int ksu_ovl_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return ksu_ovl_create_object(dentry, (mode & 07777) | S_IFDIR, 0, NULL);
}

static int ksu_ovl_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
		     dev_t rdev)
{
	/* Don't allow creation of "whiteout" on overlay */
	if (S_ISCHR(mode) && rdev == WHITEOUT_DEV)
		return -EPERM;

	return ksu_ovl_create_object(dentry, mode, rdev, NULL);
}

static int ksu_ovl_symlink(struct inode *dir, struct dentry *dentry,
		       const char *link)
{
	return ksu_ovl_create_object(dentry, S_IFLNK, 0, link);
}

static int ksu_ovl_set_link_redirect(struct dentry *dentry)
{
	const struct cred *old_cred;
	int err;

	old_cred = ksu_ovl_override_creds(dentry->d_sb);
	err = ksu_ovl_set_redirect(dentry, false);
	ksu_ovl_revert_creds(dentry->d_sb, old_cred);

	return err;
}

static int ksu_ovl_link(struct dentry *old, struct inode *newdir,
		    struct dentry *new)
{
	int err;
	struct inode *inode;

	err = ksu_ovl_want_write(old);
	if (err)
		goto out;

	err = ksu_ovl_copy_up(old);
	if (err)
		goto out_drop_write;

	err = ksu_ovl_copy_up(new->d_parent);
	if (err)
		goto out_drop_write;

	if (ksu_ovl_is_metacopy_dentry(old)) {
		err = ksu_ovl_set_link_redirect(old);
		if (err)
			goto out_drop_write;
	}

	err = ksu_ovl_nlink_start(old);
	if (err)
		goto out_drop_write;

	inode = d_inode(old);
	ihold(inode);

	err = ksu_ovl_create_or_link(new, inode,
			&(struct ksu_ovl_cattr) {.hardlink = ksu_ovl_dentry_upper(old)},
			ksu_ovl_type_origin(old));
	if (err)
		iput(inode);

	ksu_ovl_nlink_end(old);
out_drop_write:
	ksu_ovl_drop_write(old);
out:
	return err;
}

static bool ksu_ovl_matches_upper(struct dentry *dentry, struct dentry *upper)
{
	return d_inode(ksu_ovl_dentry_upper(dentry)) == d_inode(upper);
}

static int ksu_ovl_remove_and_whiteout(struct dentry *dentry,
				   struct list_head *list)
{
	struct ksu_ovl_fs *ofs = KSU_OVL_FS(dentry->d_sb);
	struct dentry *workdir = ksu_ovl_workdir(dentry);
	struct dentry *upperdir = ksu_ovl_dentry_upper(dentry->d_parent);
	struct dentry *upper;
	struct dentry *opaquedir = NULL;
	int err;

	if (WARN_ON(!workdir))
		return -EROFS;

	if (!list_empty(list)) {
		opaquedir = ksu_ovl_clear_empty(dentry, list);
		err = PTR_ERR(opaquedir);
		if (IS_ERR(opaquedir))
			goto out;
	}

	err = ksu_ovl_lock_rename_workdir(workdir, upperdir);
	if (err)
		goto out_dput;

	upper = lookup_one_len(dentry->d_name.name, upperdir,
			       dentry->d_name.len);
	err = PTR_ERR(upper);
	if (IS_ERR(upper))
		goto out_unlock;

	err = -ESTALE;
	if ((opaquedir && upper != opaquedir) ||
	    (!opaquedir && ksu_ovl_dentry_upper(dentry) &&
	     !ksu_ovl_matches_upper(dentry, upper))) {
		goto out_dput_upper;
	}

	err = ksu_ovl_cleanup_and_whiteout(ofs, d_inode(upperdir), upper);
	if (err)
		goto out_d_drop;

	ksu_ovl_dir_modified(dentry->d_parent, true);
out_d_drop:
	d_drop(dentry);
out_dput_upper:
	dput(upper);
out_unlock:
	unlock_rename(workdir, upperdir);
out_dput:
	dput(opaquedir);
out:
	return err;
}

static int ksu_ovl_remove_upper(struct dentry *dentry, bool is_dir,
			    struct list_head *list)
{
	struct dentry *upperdir = ksu_ovl_dentry_upper(dentry->d_parent);
	struct inode *dir = upperdir->d_inode;
	struct dentry *upper;
	struct dentry *opaquedir = NULL;
	int err;

	if (!list_empty(list)) {
		opaquedir = ksu_ovl_clear_empty(dentry, list);
		err = PTR_ERR(opaquedir);
		if (IS_ERR(opaquedir))
			goto out;
	}

	inode_lock_nested(dir, I_MUTEX_PARENT);
	upper = lookup_one_len(dentry->d_name.name, upperdir,
			       dentry->d_name.len);
	err = PTR_ERR(upper);
	if (IS_ERR(upper))
		goto out_unlock;

	err = -ESTALE;
	if ((opaquedir && upper != opaquedir) ||
	    (!opaquedir && !ksu_ovl_matches_upper(dentry, upper)))
		goto out_dput_upper;

	if (is_dir)
		err = vfs_rmdir(dir, upper);
	else
		err = vfs_unlink(dir, upper, NULL);
	ksu_ovl_dir_modified(dentry->d_parent, ksu_ovl_type_origin(dentry));

	/*
	 * Keeping this dentry hashed would mean having to release
	 * upperpath/lowerpath, which could only be done if we are the
	 * sole user of this dentry.  Too tricky...  Just unhash for
	 * now.
	 */
	if (!err)
		d_drop(dentry);
out_dput_upper:
	dput(upper);
out_unlock:
	inode_unlock(dir);
	dput(opaquedir);
out:
	return err;
}

static bool ksu_ovl_pure_upper(struct dentry *dentry)
{
	return !ksu_ovl_dentry_lower(dentry) &&
	       !ksu_ovl_test_flag(KSU_OVL_WHITEOUTS, d_inode(dentry));
}

static void ksu_ovl_drop_nlink(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct dentry *alias;

	/* Try to find another, hashed alias */
	spin_lock(&inode->i_lock);
	hlist_for_each_entry(alias, &inode->i_dentry, d_u.d_alias) {
		if (alias != dentry && !d_unhashed(alias))
			break;
	}
	spin_unlock(&inode->i_lock);

	/*
	 * Changes to underlying layers may cause i_nlink to lose sync with
	 * reality.  In this case prevent the link count from going to zero
	 * prematurely.
	 */
	if (inode->i_nlink > !!alias)
		drop_nlink(inode);
}

static int ksu_ovl_do_remove(struct dentry *dentry, bool is_dir)
{
	int err;
	const struct cred *old_cred;
	struct dentry *upperdentry;
	bool lower_positive = ksu_ovl_lower_positive(dentry);
	LIST_HEAD(list);

	/* No need to clean pure upper removed by vfs_rmdir() */
	if (is_dir && (lower_positive || !ksu_ovl_pure_upper(dentry))) {
		err = ksu_ovl_check_empty_dir(dentry, &list);
		if (err)
			goto out;
	}

	err = ksu_ovl_want_write(dentry);
	if (err)
		goto out;

	err = ksu_ovl_copy_up(dentry->d_parent);
	if (err)
		goto out_drop_write;

	err = ksu_ovl_nlink_start(dentry);
	if (err)
		goto out_drop_write;

	old_cred = ksu_ovl_override_creds(dentry->d_sb);
	if (!lower_positive)
		err = ksu_ovl_remove_upper(dentry, is_dir, &list);
	else
		err = ksu_ovl_remove_and_whiteout(dentry, &list);
	ksu_ovl_revert_creds(dentry->d_sb, old_cred);
	if (!err) {
		if (is_dir)
			clear_nlink(dentry->d_inode);
		else
			ksu_ovl_drop_nlink(dentry);
	}
	ksu_ovl_nlink_end(dentry);

	/*
	 * Copy ctime
	 *
	 * Note: we fail to update ctime if there was no copy-up, only a
	 * whiteout
	 */
	upperdentry = ksu_ovl_dentry_upper(dentry);
	if (upperdentry)
		ksu_ovl_copyattr(d_inode(upperdentry), d_inode(dentry));

out_drop_write:
	ksu_ovl_drop_write(dentry);
out:
	ksu_ovl_cache_free(&list);
	return err;
}

static int ksu_ovl_unlink(struct inode *dir, struct dentry *dentry)
{
	return ksu_ovl_do_remove(dentry, false);
}

static int ksu_ovl_rmdir(struct inode *dir, struct dentry *dentry)
{
	return ksu_ovl_do_remove(dentry, true);
}

static bool ksu_ovl_type_merge_or_lower(struct dentry *dentry)
{
	enum ksu_ovl_path_type type = ksu_ovl_path_type(dentry);

	return KSU_OVL_TYPE_MERGE(type) || !KSU_OVL_TYPE_UPPER(type);
}

static bool ksu_ovl_can_move(struct dentry *dentry)
{
	return ksu_ovl_redirect_dir(dentry->d_sb) ||
		!d_is_dir(dentry) || !ksu_ovl_type_merge_or_lower(dentry);
}

static char *ksu_ovl_get_redirect(struct dentry *dentry, bool abs_redirect)
{
	char *buf, *ret;
	struct dentry *d, *tmp;
	int buflen = ksu_ovl_redirect_max + 1;

	if (!abs_redirect) {
		ret = kstrndup(dentry->d_name.name, dentry->d_name.len,
			       GFP_KERNEL);
		goto out;
	}

	buf = ret = kmalloc(buflen, GFP_KERNEL);
	if (!buf)
		goto out;

	buflen--;
	buf[buflen] = '\0';
	for (d = dget(dentry); !IS_ROOT(d);) {
		const char *name;
		int thislen;

		spin_lock(&d->d_lock);
		name = ksu_ovl_dentry_get_redirect(d);
		if (name) {
			thislen = strlen(name);
		} else {
			name = d->d_name.name;
			thislen = d->d_name.len;
		}

		/* If path is too long, fall back to userspace move */
		if (thislen + (name[0] != '/') > buflen) {
			ret = ERR_PTR(-EXDEV);
			spin_unlock(&d->d_lock);
			goto out_put;
		}

		buflen -= thislen;
		memcpy(&buf[buflen], name, thislen);
		spin_unlock(&d->d_lock);
		tmp = dget_parent(d);

		dput(d);
		d = tmp;

		/* Absolute redirect: finished */
		if (buf[buflen] == '/')
			break;
		buflen--;
		buf[buflen] = '/';
	}
	ret = kstrdup(&buf[buflen], GFP_KERNEL);
out_put:
	dput(d);
	kfree(buf);
out:
	return ret ? ret : ERR_PTR(-ENOMEM);
}

static bool ksu_ovl_need_absolute_redirect(struct dentry *dentry, bool samedir)
{
	struct dentry *lowerdentry;

	if (!samedir)
		return true;

	if (d_is_dir(dentry))
		return false;

	/*
	 * For non-dir hardlinked files, we need absolute redirects
	 * in general as two upper hardlinks could be in different
	 * dirs. We could put a relative redirect now and convert
	 * it to absolute redirect later. But when nlink > 1 and
	 * indexing is on, that means relative redirect needs to be
	 * converted to absolute during copy up of another lower
	 * hardllink as well.
	 *
	 * So without optimizing too much, just check if lower is
	 * a hard link or not. If lower is hard link, put absolute
	 * redirect.
	 */
	lowerdentry = ksu_ovl_dentry_lower(dentry);
	return (d_inode(lowerdentry)->i_nlink > 1);
}

static int ksu_ovl_set_redirect(struct dentry *dentry, bool samedir)
{
	int err;
	const char *redirect = ksu_ovl_dentry_get_redirect(dentry);
	bool absolute_redirect = ksu_ovl_need_absolute_redirect(dentry, samedir);

	if (redirect && (!absolute_redirect || redirect[0] == '/'))
		return 0;

	redirect = ksu_ovl_get_redirect(dentry, absolute_redirect);
	if (IS_ERR(redirect))
		return PTR_ERR(redirect);

	err = ksu_ovl_check_setxattr(dentry, ksu_ovl_dentry_upper(dentry),
				 KSU_OVL_XATTR_REDIRECT,
				 redirect, strlen(redirect), -EXDEV);
	if (!err) {
		spin_lock(&dentry->d_lock);
		ksu_ovl_dentry_set_redirect(dentry, redirect);
		spin_unlock(&dentry->d_lock);
	} else {
		kfree(redirect);
		pr_warn_ratelimited("failed to set redirect (%i)\n",
				    err);
		/* Fall back to userspace copy-up */
		err = -EXDEV;
	}
	return err;
}

static int ksu_ovl_rename(struct inode *olddir, struct dentry *old,
		      struct inode *newdir, struct dentry *new,
		      unsigned int flags)
{
	int err;
	struct dentry *old_upperdir;
	struct dentry *new_upperdir;
	struct dentry *olddentry;
	struct dentry *newdentry;
	struct dentry *trap;
	bool old_opaque;
	bool new_opaque;
	bool cleanup_whiteout = false;
	bool update_nlink = false;
	bool overwrite = !(flags & RENAME_EXCHANGE);
	bool is_dir = d_is_dir(old);
	bool new_is_dir = d_is_dir(new);
	bool samedir = olddir == newdir;
	struct dentry *opaquedir = NULL;
	const struct cred *old_cred = NULL;
	LIST_HEAD(list);

	err = -EINVAL;
	if (flags & ~(RENAME_EXCHANGE | RENAME_NOREPLACE))
		goto out;

	flags &= ~RENAME_NOREPLACE;

	/* Don't copy up directory trees */
	err = -EXDEV;
	if (!ksu_ovl_can_move(old))
		goto out;
	if (!overwrite && !ksu_ovl_can_move(new))
		goto out;

	if (overwrite && new_is_dir && !ksu_ovl_pure_upper(new)) {
		err = ksu_ovl_check_empty_dir(new, &list);
		if (err)
			goto out;
	}

	if (overwrite) {
		if (ksu_ovl_lower_positive(old)) {
			if (!ksu_ovl_dentry_is_whiteout(new)) {
				/* Whiteout source */
				flags |= RENAME_WHITEOUT;
			} else {
				/* Switch whiteouts */
				flags |= RENAME_EXCHANGE;
			}
		} else if (is_dir && ksu_ovl_dentry_is_whiteout(new)) {
			flags |= RENAME_EXCHANGE;
			cleanup_whiteout = true;
		}
	}

	err = ksu_ovl_want_write(old);
	if (err)
		goto out;

	err = ksu_ovl_copy_up(old);
	if (err)
		goto out_drop_write;

	err = ksu_ovl_copy_up(new->d_parent);
	if (err)
		goto out_drop_write;
	if (!overwrite) {
		err = ksu_ovl_copy_up(new);
		if (err)
			goto out_drop_write;
	} else if (d_inode(new)) {
		err = ksu_ovl_nlink_start(new);
		if (err)
			goto out_drop_write;

		update_nlink = true;
	}

	old_cred = ksu_ovl_override_creds(old->d_sb);

	if (!list_empty(&list)) {
		opaquedir = ksu_ovl_clear_empty(new, &list);
		err = PTR_ERR(opaquedir);
		if (IS_ERR(opaquedir)) {
			opaquedir = NULL;
			goto out_revert_creds;
		}
	}

	old_upperdir = ksu_ovl_dentry_upper(old->d_parent);
	new_upperdir = ksu_ovl_dentry_upper(new->d_parent);

	if (!samedir) {
		/*
		 * When moving a merge dir or non-dir with copy up origin into
		 * a new parent, we are marking the new parent dir "impure".
		 * When ksu_ovl_iterate() iterates an "impure" upper dir, it will
		 * lookup the origin inodes of the entries to fill d_ino.
		 */
		if (ksu_ovl_type_origin(old)) {
			err = ksu_ovl_set_impure(new->d_parent, new_upperdir);
			if (err)
				goto out_revert_creds;
		}
		if (!overwrite && ksu_ovl_type_origin(new)) {
			err = ksu_ovl_set_impure(old->d_parent, old_upperdir);
			if (err)
				goto out_revert_creds;
		}
	}

	trap = lock_rename(new_upperdir, old_upperdir);

	olddentry = lookup_one_len(old->d_name.name, old_upperdir,
				   old->d_name.len);
	err = PTR_ERR(olddentry);
	if (IS_ERR(olddentry))
		goto out_unlock;

	err = -ESTALE;
	if (!ksu_ovl_matches_upper(old, olddentry))
		goto out_dput_old;

	newdentry = lookup_one_len(new->d_name.name, new_upperdir,
				   new->d_name.len);
	err = PTR_ERR(newdentry);
	if (IS_ERR(newdentry))
		goto out_dput_old;

	old_opaque = ksu_ovl_dentry_is_opaque(old);
	new_opaque = ksu_ovl_dentry_is_opaque(new);

	err = -ESTALE;
	if (d_inode(new) && ksu_ovl_dentry_upper(new)) {
		if (opaquedir) {
			if (newdentry != opaquedir)
				goto out_dput;
		} else {
			if (!ksu_ovl_matches_upper(new, newdentry))
				goto out_dput;
		}
	} else {
		if (!d_is_negative(newdentry)) {
			if (!new_opaque || !ksu_ovl_is_whiteout(newdentry))
				goto out_dput;
		} else {
			if (flags & RENAME_EXCHANGE)
				goto out_dput;
		}
	}

	if (olddentry == trap)
		goto out_dput;
	if (newdentry == trap)
		goto out_dput;

	if (olddentry->d_inode == newdentry->d_inode)
		goto out_dput;

	err = 0;
	if (ksu_ovl_type_merge_or_lower(old))
		err = ksu_ovl_set_redirect(old, samedir);
	else if (is_dir && !old_opaque && ksu_ovl_type_merge(new->d_parent))
		err = ksu_ovl_set_opaque_xerr(old, olddentry, -EXDEV);
	if (err)
		goto out_dput;

	if (!overwrite && ksu_ovl_type_merge_or_lower(new))
		err = ksu_ovl_set_redirect(new, samedir);
	else if (!overwrite && new_is_dir && !new_opaque &&
		 ksu_ovl_type_merge(old->d_parent))
		err = ksu_ovl_set_opaque_xerr(new, newdentry, -EXDEV);
	if (err)
		goto out_dput;

	err = ksu_ovl_do_rename(old_upperdir->d_inode, olddentry,
			    new_upperdir->d_inode, newdentry, flags);
	if (err)
		goto out_dput;

	if (cleanup_whiteout)
		ksu_ovl_cleanup(old_upperdir->d_inode, newdentry);

	if (overwrite && d_inode(new)) {
		if (new_is_dir)
			clear_nlink(d_inode(new));
		else
			ksu_ovl_drop_nlink(new);
	}

	ksu_ovl_dir_modified(old->d_parent, ksu_ovl_type_origin(old) ||
			 (!overwrite && ksu_ovl_type_origin(new)));
	ksu_ovl_dir_modified(new->d_parent, ksu_ovl_type_origin(old) ||
			 (d_inode(new) && ksu_ovl_type_origin(new)));

	/* copy ctime: */
	ksu_ovl_copyattr(d_inode(olddentry), d_inode(old));
	if (d_inode(new) && ksu_ovl_dentry_upper(new))
		ksu_ovl_copyattr(d_inode(newdentry), d_inode(new));

out_dput:
	dput(newdentry);
out_dput_old:
	dput(olddentry);
out_unlock:
	unlock_rename(new_upperdir, old_upperdir);
out_revert_creds:
	ksu_ovl_revert_creds(old->d_sb, old_cred);
	if (update_nlink)
		ksu_ovl_nlink_end(new);
out_drop_write:
	ksu_ovl_drop_write(old);
out:
	dput(opaquedir);
	ksu_ovl_cache_free(&list);
	return err;
}

const struct inode_operations ksu_ovl_dir_inode_operations = {
	.lookup		= ksu_ovl_lookup,
	.mkdir		= ksu_ovl_mkdir,
	.symlink	= ksu_ovl_symlink,
	.unlink		= ksu_ovl_unlink,
	.rmdir		= ksu_ovl_rmdir,
	.rename		= ksu_ovl_rename,
	.link		= ksu_ovl_link,
	.setattr	= ksu_ovl_setattr,
	.create		= ksu_ovl_create,
	.mknod		= ksu_ovl_mknod,
	.permission	= ksu_ovl_permission,
	.getattr	= ksu_ovl_getattr,
	.listxattr	= ksu_ovl_listxattr,
	.get_acl	= ksu_ovl_get_acl,
	.update_time	= ksu_ovl_update_time,
};
