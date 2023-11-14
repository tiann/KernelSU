// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Novell Inc.
 * Copyright (C) 2016 Red Hat, Inc.
 */

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/xattr.h>
#include <linux/exportfs.h>
#include <linux/uuid.h>
#include <linux/namei.h>
#include <linux/ratelimit.h>
#include "overlayfs.h"

int ksu_ovl_want_write(struct dentry *dentry)
{
	struct ksu_ovl_fs *ofs = dentry->d_sb->s_fs_info;
	return mnt_want_write(ksu_ovl_upper_mnt(ofs));
}

void ksu_ovl_drop_write(struct dentry *dentry)
{
	struct ksu_ovl_fs *ofs = dentry->d_sb->s_fs_info;
	mnt_drop_write(ksu_ovl_upper_mnt(ofs));
}

struct dentry *ksu_ovl_workdir(struct dentry *dentry)
{
	struct ksu_ovl_fs *ofs = dentry->d_sb->s_fs_info;
	return ofs->workdir;
}

const struct cred *ksu_ovl_override_creds(struct super_block *sb)
{
	struct ksu_ovl_fs *ofs = sb->s_fs_info;

	if (!ofs->config.override_creds)
		return NULL;
	return override_creds(ofs->creator_cred);
}

void ksu_ovl_revert_creds(struct super_block *sb, const struct cred *old_cred)
{
	if (old_cred)
		revert_creds(old_cred);
}

/*
 * Check if underlying fs supports file handles and try to determine encoding
 * type, in order to deduce maximum inode number used by fs.
 *
 * Return 0 if file handles are not supported.
 * Return 1 (FILEID_INO32_GEN) if fs uses the default 32bit inode encoding.
 * Return -1 if fs uses a non default encoding with unknown inode size.
 */
int ksu_ovl_can_decode_fh(struct super_block *sb)
{
	if (!sb->s_export_op || !sb->s_export_op->fh_to_dentry)
		return 0;

	return sb->s_export_op->encode_fh ? -1 : FILEID_INO32_GEN;
}

struct dentry *ksu_ovl_indexdir(struct super_block *sb)
{
	struct ksu_ovl_fs *ofs = sb->s_fs_info;

	return ofs->indexdir;
}

/* Index all files on copy up. For now only enabled for NFS export */
bool ksu_ovl_index_all(struct super_block *sb)
{
	struct ksu_ovl_fs *ofs = sb->s_fs_info;

	return ofs->config.nfs_export && ofs->config.index;
}

/* Verify lower origin on lookup. For now only enabled for NFS export */
bool ksu_ovl_verify_lower(struct super_block *sb)
{
	struct ksu_ovl_fs *ofs = sb->s_fs_info;

	return ofs->config.nfs_export && ofs->config.index;
}

struct ksu_ovl_entry *ksu_ovl_alloc_entry(unsigned int numlower)
{
	size_t size = offsetof(struct ksu_ovl_entry, lowerstack[numlower]);
	struct ksu_ovl_entry *oe = kzalloc(size, GFP_KERNEL);

	if (oe)
		oe->numlower = numlower;

	return oe;
}

bool ksu_ovl_dentry_remote(struct dentry *dentry)
{
	return dentry->d_flags &
		(DCACHE_OP_REVALIDATE | DCACHE_OP_WEAK_REVALIDATE);
}

void ksu_ovl_dentry_update_reval(struct dentry *dentry, struct dentry *upperdentry,
			     unsigned int mask)
{
	struct ksu_ovl_entry *oe = KSU_OVL_E(dentry);
	unsigned int i, flags = 0;

	if (upperdentry)
		flags |= upperdentry->d_flags;
	for (i = 0; i < oe->numlower; i++)
		flags |= oe->lowerstack[i].dentry->d_flags;

	spin_lock(&dentry->d_lock);
	dentry->d_flags &= ~mask;
	dentry->d_flags |= flags & mask;
	spin_unlock(&dentry->d_lock);
}

bool ksu_ovl_dentry_weird(struct dentry *dentry)
{
	return dentry->d_flags & (DCACHE_NEED_AUTOMOUNT |
				  DCACHE_MANAGE_TRANSIT |
				  DCACHE_OP_HASH |
				  DCACHE_OP_COMPARE);
}

enum ksu_ovl_path_type ksu_ovl_path_type(struct dentry *dentry)
{
	struct ksu_ovl_entry *oe = dentry->d_fsdata;
	enum ksu_ovl_path_type type = 0;

	if (ksu_ovl_dentry_upper(dentry)) {
		type = __KSU_OVL_PATH_UPPER;

		/*
		 * Non-dir dentry can hold lower dentry of its copy up origin.
		 */
		if (oe->numlower) {
			if (ksu_ovl_test_flag(KSU_OVL_CONST_INO, d_inode(dentry)))
				type |= __KSU_OVL_PATH_ORIGIN;
			if (d_is_dir(dentry) ||
			    !ksu_ovl_has_upperdata(d_inode(dentry)))
				type |= __KSU_OVL_PATH_MERGE;
		}
	} else {
		if (oe->numlower > 1)
			type |= __KSU_OVL_PATH_MERGE;
	}
	return type;
}

void ksu_ovl_path_upper(struct dentry *dentry, struct path *path)
{
	struct ksu_ovl_fs *ofs = dentry->d_sb->s_fs_info;

	path->mnt = ksu_ovl_upper_mnt(ofs);
	path->dentry = ksu_ovl_dentry_upper(dentry);
}

void ksu_ovl_path_lower(struct dentry *dentry, struct path *path)
{
	struct ksu_ovl_entry *oe = dentry->d_fsdata;

	if (oe->numlower) {
		path->mnt = oe->lowerstack[0].layer->mnt;
		path->dentry = oe->lowerstack[0].dentry;
	} else {
		*path = (struct path) { };
	}
}

void ksu_ovl_path_lowerdata(struct dentry *dentry, struct path *path)
{
	struct ksu_ovl_entry *oe = dentry->d_fsdata;

	if (oe->numlower) {
		path->mnt = oe->lowerstack[oe->numlower - 1].layer->mnt;
		path->dentry = oe->lowerstack[oe->numlower - 1].dentry;
	} else {
		*path = (struct path) { };
	}
}

enum ksu_ovl_path_type ksu_ovl_path_real(struct dentry *dentry, struct path *path)
{
	enum ksu_ovl_path_type type = ksu_ovl_path_type(dentry);

	if (!KSU_OVL_TYPE_UPPER(type))
		ksu_ovl_path_lower(dentry, path);
	else
		ksu_ovl_path_upper(dentry, path);

	return type;
}

struct dentry *ksu_ovl_dentry_upper(struct dentry *dentry)
{
	return ksu_ovl_upperdentry_dereference(KSU_OVL_I(d_inode(dentry)));
}

struct dentry *ksu_ovl_dentry_lower(struct dentry *dentry)
{
	struct ksu_ovl_entry *oe = dentry->d_fsdata;

	return oe->numlower ? oe->lowerstack[0].dentry : NULL;
}

const struct ksu_ovl_layer *ksu_ovl_layer_lower(struct dentry *dentry)
{
	struct ksu_ovl_entry *oe = dentry->d_fsdata;

	return oe->numlower ? oe->lowerstack[0].layer : NULL;
}

/*
 * ksu_ovl_dentry_lower() could return either a data dentry or metacopy dentry
 * dependig on what is stored in lowerstack[0]. At times we need to find
 * lower dentry which has data (and not metacopy dentry). This helper
 * returns the lower data dentry.
 */
struct dentry *ksu_ovl_dentry_lowerdata(struct dentry *dentry)
{
	struct ksu_ovl_entry *oe = dentry->d_fsdata;

	return oe->numlower ? oe->lowerstack[oe->numlower - 1].dentry : NULL;
}

struct dentry *ksu_ovl_dentry_real(struct dentry *dentry)
{
	return ksu_ovl_dentry_upper(dentry) ?: ksu_ovl_dentry_lower(dentry);
}

struct dentry *ksu_ovl_i_dentry_upper(struct inode *inode)
{
	return ksu_ovl_upperdentry_dereference(KSU_OVL_I(inode));
}

struct inode *ksu_ovl_inode_upper(struct inode *inode)
{
	struct dentry *upperdentry = ksu_ovl_i_dentry_upper(inode);

	return upperdentry ? d_inode(upperdentry) : NULL;
}

struct inode *ksu_ovl_inode_lower(struct inode *inode)
{
	return KSU_OVL_I(inode)->lower;
}

struct inode *ksu_ovl_inode_real(struct inode *inode)
{
	return ksu_ovl_inode_upper(inode) ?: ksu_ovl_inode_lower(inode);
}

/* Return inode which contains lower data. Do not return metacopy */
struct inode *ksu_ovl_inode_lowerdata(struct inode *inode)
{
	if (WARN_ON(!S_ISREG(inode->i_mode)))
		return NULL;

	return KSU_OVL_I(inode)->lowerdata ?: ksu_ovl_inode_lower(inode);
}

/* Return real inode which contains data. Does not return metacopy inode */
struct inode *ksu_ovl_inode_realdata(struct inode *inode)
{
	struct inode *upperinode;

	upperinode = ksu_ovl_inode_upper(inode);
	if (upperinode && ksu_ovl_has_upperdata(inode))
		return upperinode;

	return ksu_ovl_inode_lowerdata(inode);
}

struct ksu_ovl_dir_cache *ksu_ovl_dir_cache(struct inode *inode)
{
	return KSU_OVL_I(inode)->cache;
}

void ksu_ovl_set_dir_cache(struct inode *inode, struct ksu_ovl_dir_cache *cache)
{
	KSU_OVL_I(inode)->cache = cache;
}

void ksu_ovl_dentry_set_flag(unsigned long flag, struct dentry *dentry)
{
	set_bit(flag, &KSU_OVL_E(dentry)->flags);
}

void ksu_ovl_dentry_clear_flag(unsigned long flag, struct dentry *dentry)
{
	clear_bit(flag, &KSU_OVL_E(dentry)->flags);
}

bool ksu_ovl_dentry_test_flag(unsigned long flag, struct dentry *dentry)
{
	return test_bit(flag, &KSU_OVL_E(dentry)->flags);
}

bool ksu_ovl_dentry_is_opaque(struct dentry *dentry)
{
	return ksu_ovl_dentry_test_flag(KSU_OVL_E_OPAQUE, dentry);
}

bool ksu_ovl_dentry_is_whiteout(struct dentry *dentry)
{
	return !dentry->d_inode && ksu_ovl_dentry_is_opaque(dentry);
}

void ksu_ovl_dentry_set_opaque(struct dentry *dentry)
{
	ksu_ovl_dentry_set_flag(KSU_OVL_E_OPAQUE, dentry);
}

/*
 * For hard links and decoded file handles, it's possible for ksu_ovl_dentry_upper()
 * to return positive, while there's no actual upper alias for the inode.
 * Copy up code needs to know about the existence of the upper alias, so it
 * can't use ksu_ovl_dentry_upper().
 */
bool ksu_ovl_dentry_has_upper_alias(struct dentry *dentry)
{
	return ksu_ovl_dentry_test_flag(KSU_OVL_E_UPPER_ALIAS, dentry);
}

void ksu_ovl_dentry_set_upper_alias(struct dentry *dentry)
{
	ksu_ovl_dentry_set_flag(KSU_OVL_E_UPPER_ALIAS, dentry);
}

static bool ksu_ovl_should_check_upperdata(struct inode *inode)
{
	if (!S_ISREG(inode->i_mode))
		return false;

	if (!ksu_ovl_inode_lower(inode))
		return false;

	return true;
}

bool ksu_ovl_has_upperdata(struct inode *inode)
{
	if (!ksu_ovl_should_check_upperdata(inode))
		return true;

	if (!ksu_ovl_test_flag(KSU_OVL_UPPERDATA, inode))
		return false;
	/*
	 * Pairs with smp_wmb() in ksu_ovl_set_upperdata(). Main user of
	 * ksu_ovl_has_upperdata() is ksu_ovl_copy_up_meta_inode_data(). Make sure
	 * if setting of KSU_OVL_UPPERDATA is visible, then effects of writes
	 * before that are visible too.
	 */
	smp_rmb();
	return true;
}

void ksu_ovl_set_upperdata(struct inode *inode)
{
	/*
	 * Pairs with smp_rmb() in ksu_ovl_has_upperdata(). Make sure
	 * if KSU_OVL_UPPERDATA flag is visible, then effects of write operations
	 * before it are visible as well.
	 */
	smp_wmb();
	ksu_ovl_set_flag(KSU_OVL_UPPERDATA, inode);
}

/* Caller should hold ksu_ovl_inode->lock */
bool ksu_ovl_dentry_needs_data_copy_up_locked(struct dentry *dentry, int flags)
{
	if (!ksu_ovl_open_flags_need_copy_up(flags))
		return false;

	return !ksu_ovl_test_flag(KSU_OVL_UPPERDATA, d_inode(dentry));
}

bool ksu_ovl_dentry_needs_data_copy_up(struct dentry *dentry, int flags)
{
	if (!ksu_ovl_open_flags_need_copy_up(flags))
		return false;

	return !ksu_ovl_has_upperdata(d_inode(dentry));
}

bool ksu_ovl_redirect_dir(struct super_block *sb)
{
	struct ksu_ovl_fs *ofs = sb->s_fs_info;

	return ofs->config.redirect_dir && !ofs->noxattr;
}

const char *ksu_ovl_dentry_get_redirect(struct dentry *dentry)
{
	return KSU_OVL_I(d_inode(dentry))->redirect;
}

void ksu_ovl_dentry_set_redirect(struct dentry *dentry, const char *redirect)
{
	struct ksu_ovl_inode *oi = KSU_OVL_I(d_inode(dentry));

	kfree(oi->redirect);
	oi->redirect = redirect;
}

void ksu_ovl_inode_update(struct inode *inode, struct dentry *upperdentry)
{
	struct inode *upperinode = d_inode(upperdentry);

	WARN_ON(KSU_OVL_I(inode)->__upperdentry);

	/*
	 * Make sure upperdentry is consistent before making it visible
	 */
	smp_wmb();
	KSU_OVL_I(inode)->__upperdentry = upperdentry;
	if (inode_unhashed(inode)) {
		inode->i_private = upperinode;
		__insert_inode_hash(inode, (unsigned long) upperinode);
	}
}

static void ksu_ovl_dir_version_inc(struct dentry *dentry, bool impurity)
{
	struct inode *inode = d_inode(dentry);

	WARN_ON(!inode_is_locked(inode));
	WARN_ON(!d_is_dir(dentry));
	/*
	 * Version is used by readdir code to keep cache consistent.
	 * For merge dirs (or dirs with origin) all changes need to be noted.
	 * For non-merge dirs, cache contains only impure entries (i.e. ones
	 * which have been copied up and have origins), so only need to note
	 * changes to impure entries.
	 */
	if (!ksu_ovl_dir_is_real(dentry) || impurity)
		KSU_OVL_I(inode)->version++;
}

void ksu_ovl_dir_modified(struct dentry *dentry, bool impurity)
{
	/* Copy mtime/ctime */
	ksu_ovl_copyattr(d_inode(ksu_ovl_dentry_upper(dentry)), d_inode(dentry));

	ksu_ovl_dir_version_inc(dentry, impurity);
}

u64 ksu_ovl_dentry_version_get(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);

	WARN_ON(!inode_is_locked(inode));
	return KSU_OVL_I(inode)->version;
}

bool ksu_ovl_is_whiteout(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;

	return inode && IS_WHITEOUT(inode);
}

struct file *ksu_ovl_path_open(struct path *path, int flags)
{
	struct inode *inode = d_inode(path->dentry);
	int err, acc_mode;

	if (flags & ~(O_ACCMODE | O_LARGEFILE))
		BUG();

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		acc_mode = MAY_READ;
		break;
	case O_WRONLY:
		acc_mode = MAY_WRITE;
		break;
	default:
		BUG();
	}

	err = inode_permission(inode, acc_mode | MAY_OPEN);
	if (err)
		return ERR_PTR(err);

	/* O_NOATIME is an optimization, don't fail if not permitted */
	if (inode_owner_or_capable(inode))
		flags |= O_NOATIME;

	return dentry_open(path, flags, current_cred());
}

/* Caller should hold ksu_ovl_inode->lock */
static bool ksu_ovl_already_copied_up_locked(struct dentry *dentry, int flags)
{
	bool disconnected = dentry->d_flags & DCACHE_DISCONNECTED;

	if (ksu_ovl_dentry_upper(dentry) &&
	    (ksu_ovl_dentry_has_upper_alias(dentry) || disconnected) &&
	    !ksu_ovl_dentry_needs_data_copy_up_locked(dentry, flags))
		return true;

	return false;
}

bool ksu_ovl_already_copied_up(struct dentry *dentry, int flags)
{
	bool disconnected = dentry->d_flags & DCACHE_DISCONNECTED;

	/*
	 * Check if copy-up has happened as well as for upper alias (in
	 * case of hard links) is there.
	 *
	 * Both checks are lockless:
	 *  - false negatives: will recheck under oi->lock
	 *  - false positives:
	 *    + ksu_ovl_dentry_upper() uses memory barriers to ensure the
	 *      upper dentry is up-to-date
	 *    + ksu_ovl_dentry_has_upper_alias() relies on locking of
	 *      upper parent i_rwsem to prevent reordering copy-up
	 *      with rename.
	 */
	if (ksu_ovl_dentry_upper(dentry) &&
	    (ksu_ovl_dentry_has_upper_alias(dentry) || disconnected) &&
	    !ksu_ovl_dentry_needs_data_copy_up(dentry, flags))
		return true;

	return false;
}

int ksu_ovl_copy_up_start(struct dentry *dentry, int flags)
{
	struct inode *inode = d_inode(dentry);
	int err;

	err = ksu_ovl_inode_lock_interruptible(inode);
	if (!err && ksu_ovl_already_copied_up_locked(dentry, flags)) {
		err = 1; /* Already copied up */
		ksu_ovl_inode_unlock(inode);
	}

	return err;
}

void ksu_ovl_copy_up_end(struct dentry *dentry)
{
	ksu_ovl_inode_unlock(d_inode(dentry));
}

bool ksu_ovl_check_origin_xattr(struct ksu_ovl_fs *ofs, struct dentry *dentry)
{
	ssize_t res;

	res = ksu_ovl_do_getxattr(ofs, dentry, KSU_OVL_XATTR_ORIGIN, NULL, 0);

	/* Zero size value means "copied up but origin unknown" */
	if (res >= 0)
		return true;

	return false;
}

bool ksu_ovl_check_dir_xattr(struct super_block *sb, struct dentry *dentry,
			 enum ksu_ovl_xattr ox)
{
	ssize_t res;
	char val;

	if (!d_is_dir(dentry))
		return false;

	res = ksu_ovl_do_getxattr(KSU_OVL_FS(sb), dentry, ox, &val, 1);
	if (res == 1 && val == 'y')
		return true;

	return false;
}

#define KSU_OVL_XATTR_OPAQUE_POSTFIX	"opaque"
#define KSU_OVL_XATTR_REDIRECT_POSTFIX	"redirect"
#define KSU_OVL_XATTR_ORIGIN_POSTFIX	"origin"
#define KSU_OVL_XATTR_IMPURE_POSTFIX	"impure"
#define KSU_OVL_XATTR_NLINK_POSTFIX		"nlink"
#define KSU_OVL_XATTR_UPPER_POSTFIX		"upper"
#define KSU_OVL_XATTR_METACOPY_POSTFIX	"metacopy"

#define KSU_OVL_XATTR_TAB_ENTRY(x) \
	[x] = KSU_OVL_XATTR_PREFIX x ## _POSTFIX

const char *ksu_ovl_xattr_table[] = {
	KSU_OVL_XATTR_TAB_ENTRY(KSU_OVL_XATTR_OPAQUE),
	KSU_OVL_XATTR_TAB_ENTRY(KSU_OVL_XATTR_REDIRECT),
	KSU_OVL_XATTR_TAB_ENTRY(KSU_OVL_XATTR_ORIGIN),
	KSU_OVL_XATTR_TAB_ENTRY(KSU_OVL_XATTR_IMPURE),
	KSU_OVL_XATTR_TAB_ENTRY(KSU_OVL_XATTR_NLINK),
	KSU_OVL_XATTR_TAB_ENTRY(KSU_OVL_XATTR_UPPER),
	KSU_OVL_XATTR_TAB_ENTRY(KSU_OVL_XATTR_METACOPY),
};

int ksu_ovl_check_setxattr(struct dentry *dentry, struct dentry *upperdentry,
		       enum ksu_ovl_xattr ox, const void *value, size_t size,
		       int xerr)
{
	int err;
	struct ksu_ovl_fs *ofs = dentry->d_sb->s_fs_info;

	if (ofs->noxattr)
		return xerr;

	err = ksu_ovl_do_setxattr(ofs, upperdentry, ox, value, size);

	if (err == -EOPNOTSUPP) {
		pr_warn("cannot set %s xattr on upper\n", ksu_ovl_xattr(ofs, ox));
		ofs->noxattr = true;
		return xerr;
	}

	return err;
}

int ksu_ovl_set_impure(struct dentry *dentry, struct dentry *upperdentry)
{
	int err;

	if (ksu_ovl_test_flag(KSU_OVL_IMPURE, d_inode(dentry)))
		return 0;

	/*
	 * Do not fail when upper doesn't support xattrs.
	 * Upper inodes won't have origin nor redirect xattr anyway.
	 */
	err = ksu_ovl_check_setxattr(dentry, upperdentry, KSU_OVL_XATTR_IMPURE,
				 "y", 1, 0);
	if (!err)
		ksu_ovl_set_flag(KSU_OVL_IMPURE, d_inode(dentry));

	return err;
}

/**
 * Caller must hold a reference to inode to prevent it from being freed while
 * it is marked inuse.
 */
bool ksu_ovl_inuse_trylock(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	bool locked = false;

	spin_lock(&inode->i_lock);
	if (!(inode->i_state & I_OVL_INUSE)) {
		inode->i_state |= I_OVL_INUSE;
		locked = true;
	}
	spin_unlock(&inode->i_lock);

	return locked;
}

void ksu_ovl_inuse_unlock(struct dentry *dentry)
{
	if (dentry) {
		struct inode *inode = d_inode(dentry);

		spin_lock(&inode->i_lock);
		WARN_ON(!(inode->i_state & I_OVL_INUSE));
		inode->i_state &= ~I_OVL_INUSE;
		spin_unlock(&inode->i_lock);
	}
}

bool ksu_ovl_is_inuse(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	bool inuse;

	spin_lock(&inode->i_lock);
	inuse = (inode->i_state & I_OVL_INUSE);
	spin_unlock(&inode->i_lock);

	return inuse;
}

/*
 * Does this overlay dentry need to be indexed on copy up?
 */
bool ksu_ovl_need_index(struct dentry *dentry)
{
	struct dentry *lower = ksu_ovl_dentry_lower(dentry);

	if (!lower || !ksu_ovl_indexdir(dentry->d_sb))
		return false;

	/* Index all files for NFS export and consistency verification */
	if (ksu_ovl_index_all(dentry->d_sb))
		return true;

	/* Index only lower hardlinks on copy up */
	if (!d_is_dir(lower) && d_inode(lower)->i_nlink > 1)
		return true;

	return false;
}

/* Caller must hold KSU_OVL_I(inode)->lock */
static void ksu_ovl_cleanup_index(struct dentry *dentry)
{
	struct dentry *indexdir = ksu_ovl_indexdir(dentry->d_sb);
	struct inode *dir = indexdir->d_inode;
	struct dentry *lowerdentry = ksu_ovl_dentry_lower(dentry);
	struct dentry *upperdentry = ksu_ovl_dentry_upper(dentry);
	struct dentry *index = NULL;
	struct inode *inode;
	struct qstr name = { };
	int err;

	err = ksu_ovl_get_index_name(lowerdentry, &name);
	if (err)
		goto fail;

	inode = d_inode(upperdentry);
	if (!S_ISDIR(inode->i_mode) && inode->i_nlink != 1) {
		pr_warn_ratelimited("cleanup linked index (%pd2, ino=%lu, nlink=%u)\n",
				    upperdentry, inode->i_ino, inode->i_nlink);
		/*
		 * We either have a bug with persistent union nlink or a lower
		 * hardlink was added while overlay is mounted. Adding a lower
		 * hardlink and then unlinking all overlay hardlinks would drop
		 * overlay nlink to zero before all upper inodes are unlinked.
		 * As a safety measure, when that situation is detected, set
		 * the overlay nlink to the index inode nlink minus one for the
		 * index entry itself.
		 */
		set_nlink(d_inode(dentry), inode->i_nlink - 1);
		ksu_ovl_set_nlink_upper(dentry);
		goto out;
	}

	inode_lock_nested(dir, I_MUTEX_PARENT);
	index = lookup_one_len(name.name, indexdir, name.len);
	err = PTR_ERR(index);
	if (IS_ERR(index)) {
		index = NULL;
	} else if (ksu_ovl_index_all(dentry->d_sb)) {
		/* Whiteout orphan index to block future open by handle */
		err = ksu_ovl_cleanup_and_whiteout(KSU_OVL_FS(dentry->d_sb),
					       dir, index);
	} else {
		/* Cleanup orphan index entries */
		err = ksu_ovl_cleanup(dir, index);
	}

	inode_unlock(dir);
	if (err)
		goto fail;

out:
	kfree(name.name);
	dput(index);
	return;

fail:
	pr_err("cleanup index of '%pd2' failed (%i)\n", dentry, err);
	goto out;
}

/*
 * Operations that change overlay inode and upper inode nlink need to be
 * synchronized with copy up for persistent nlink accounting.
 */
int ksu_ovl_nlink_start(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	const struct cred *old_cred;
	int err;

	if (WARN_ON(!inode))
		return -ENOENT;

	/*
	 * With inodes index is enabled, we store the union overlay nlink
	 * in an xattr on the index inode. When whiting out an indexed lower,
	 * we need to decrement the overlay persistent nlink, but before the
	 * first copy up, we have no upper index inode to store the xattr.
	 *
	 * As a workaround, before whiteout/rename over an indexed lower,
	 * copy up to create the upper index. Creating the upper index will
	 * initialize the overlay nlink, so it could be dropped if unlink
	 * or rename succeeds.
	 *
	 * TODO: implement metadata only index copy up when called with
	 *       ksu_ovl_copy_up_flags(dentry, O_PATH).
	 */
	if (ksu_ovl_need_index(dentry) && !ksu_ovl_dentry_has_upper_alias(dentry)) {
		err = ksu_ovl_copy_up(dentry);
		if (err)
			return err;
	}

	err = ksu_ovl_inode_lock_interruptible(inode);
	if (err)
		return err;

	if (d_is_dir(dentry) || !ksu_ovl_test_flag(KSU_OVL_INDEX, inode))
		goto out;

	old_cred = ksu_ovl_override_creds(dentry->d_sb);
	/*
	 * The overlay inode nlink should be incremented/decremented IFF the
	 * upper operation succeeds, along with nlink change of upper inode.
	 * Therefore, before link/unlink/rename, we store the union nlink
	 * value relative to the upper inode nlink in an upper inode xattr.
	 */
	err = ksu_ovl_set_nlink_upper(dentry);
	ksu_ovl_revert_creds(dentry->d_sb, old_cred);

out:
	if (err)
		ksu_ovl_inode_unlock(inode);

	return err;
}

void ksu_ovl_nlink_end(struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);

	if (ksu_ovl_test_flag(KSU_OVL_INDEX, inode) && inode->i_nlink == 0) {
		const struct cred *old_cred;

		old_cred = ksu_ovl_override_creds(dentry->d_sb);
		ksu_ovl_cleanup_index(dentry);
		ksu_ovl_revert_creds(dentry->d_sb, old_cred);
	}

	ksu_ovl_inode_unlock(inode);
}

int ksu_ovl_lock_rename_workdir(struct dentry *workdir, struct dentry *upperdir)
{
	/* Workdir should not be the same as upperdir */
	if (workdir == upperdir)
		goto err;

	/* Workdir should not be subdir of upperdir and vice versa */
	if (lock_rename(workdir, upperdir) != NULL)
		goto err_unlock;

	return 0;

err_unlock:
	unlock_rename(workdir, upperdir);
err:
	pr_err("failed to lock workdir+upperdir\n");
	return -EIO;
}

/* err < 0, 0 if no metacopy xattr, 1 if metacopy xattr found */
int ksu_ovl_check_metacopy_xattr(struct ksu_ovl_fs *ofs, struct dentry *dentry)
{
	ssize_t res;

	/* Only regular files can have metacopy xattr */
	if (!S_ISREG(d_inode(dentry)->i_mode))
		return 0;

	res = ksu_ovl_do_getxattr(ofs, dentry, KSU_OVL_XATTR_METACOPY, NULL, 0);
	if (res < 0) {
		if (res == -ENODATA || res == -EOPNOTSUPP)
			return 0;
		goto out;
	}

	return 1;
out:
	pr_warn_ratelimited("failed to get metacopy (%zi)\n", res);
	return res;
}

bool ksu_ovl_is_metacopy_dentry(struct dentry *dentry)
{
	struct ksu_ovl_entry *oe = dentry->d_fsdata;

	if (!d_is_reg(dentry))
		return false;

	if (ksu_ovl_dentry_upper(dentry)) {
		if (!ksu_ovl_has_upperdata(d_inode(dentry)))
			return true;
		return false;
	}

	return (oe->numlower > 1);
}

char *ksu_ovl_get_redirect_xattr(struct ksu_ovl_fs *ofs, struct dentry *dentry,
			     int padding)
{
	int res;
	char *s, *next, *buf = NULL;

	res = ksu_ovl_do_getxattr(ofs, dentry, KSU_OVL_XATTR_REDIRECT, NULL, 0);
	if (res == -ENODATA || res == -EOPNOTSUPP)
		return NULL;
	if (res < 0)
		goto fail;
	if (res == 0)
		goto invalid;

	buf = kzalloc(res + padding + 1, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	res = ksu_ovl_do_getxattr(ofs, dentry, KSU_OVL_XATTR_REDIRECT, buf, res);
	if (res < 0)
		goto fail;
	if (res == 0)
		goto invalid;

	if (buf[0] == '/') {
		for (s = buf; *s++ == '/'; s = next) {
			next = strchrnul(s, '/');
			if (s == next)
				goto invalid;
		}
	} else {
		if (strchr(buf, '/') != NULL)
			goto invalid;
	}

	return buf;
invalid:
	pr_warn_ratelimited("invalid redirect (%s)\n", buf);
	res = -EINVAL;
	goto err_free;
fail:
	pr_warn_ratelimited("failed to get redirect (%i)\n", res);
err_free:
	kfree(buf);
	return ERR_PTR(res);
}

/*
 * ksu_ovl_sync_status() - Check fs sync status for volatile mounts
 *
 * Returns 1 if this is not a volatile mount and a real sync is required.
 *
 * Returns 0 if syncing can be skipped because mount is volatile, and no errors
 * have occurred on the upperdir since the mount.
 *
 * Returns -errno if it is a volatile mount, and the error that occurred since
 * the last mount. If the error code changes, it'll return the latest error
 * code.
 */

int ksu_ovl_sync_status(struct ksu_ovl_fs *ofs)
{
	struct vfsmount *mnt;

	if (ksu_ovl_should_sync(ofs))
		return 1;

	mnt = ksu_ovl_upper_mnt(ofs);
	if (!mnt)
		return 0;

	return errseq_check(&mnt->mnt_sb->s_wb_err, ofs->errseq);
}
