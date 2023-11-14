// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * Copyright (C) 2011 Novell Inc.
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/xattr.h>
#include <linux/rbtree.h>
#include <linux/security.h>
#include <linux/cred.h>
#include <linux/ratelimit.h>
#include "overlayfs.h"

struct ksu_ovl_cache_entry {
	unsigned int len;
	unsigned int type;
	u64 real_ino;
	u64 ino;
	struct list_head l_node;
	struct rb_node node;
	struct ksu_ovl_cache_entry *next_maybe_whiteout;
	bool is_upper;
	bool is_whiteout;
	char name[];
};

struct ksu_ovl_dir_cache {
	long refcount;
	u64 version;
	struct list_head entries;
	struct rb_root root;
};

struct ksu_ovl_readdir_data {
	struct dir_context ctx;
	struct dentry *dentry;
	bool is_lowest;
	struct rb_root *root;
	struct list_head *list;
	struct list_head middle;
	struct ksu_ovl_cache_entry *first_maybe_whiteout;
	int count;
	int err;
	bool is_upper;
	bool d_type_supported;
};

struct ksu_ovl_dir_file {
	bool is_real;
	bool is_upper;
	struct ksu_ovl_dir_cache *cache;
	struct list_head *cursor;
	struct file *realfile;
	struct file *upperfile;
};

static struct ksu_ovl_cache_entry *ksu_ovl_cache_entry_from_node(struct rb_node *n)
{
	return rb_entry(n, struct ksu_ovl_cache_entry, node);
}

static bool ksu_ovl_cache_entry_find_link(const char *name, int len,
				      struct rb_node ***link,
				      struct rb_node **parent)
{
	bool found = false;
	struct rb_node **newp = *link;

	while (!found && *newp) {
		int cmp;
		struct ksu_ovl_cache_entry *tmp;

		*parent = *newp;
		tmp = ksu_ovl_cache_entry_from_node(*newp);
		cmp = strncmp(name, tmp->name, len);
		if (cmp > 0)
			newp = &tmp->node.rb_right;
		else if (cmp < 0 || len < tmp->len)
			newp = &tmp->node.rb_left;
		else
			found = true;
	}
	*link = newp;

	return found;
}

static struct ksu_ovl_cache_entry *ksu_ovl_cache_entry_find(struct rb_root *root,
						    const char *name, int len)
{
	struct rb_node *node = root->rb_node;
	int cmp;

	while (node) {
		struct ksu_ovl_cache_entry *p = ksu_ovl_cache_entry_from_node(node);

		cmp = strncmp(name, p->name, len);
		if (cmp > 0)
			node = p->node.rb_right;
		else if (cmp < 0 || len < p->len)
			node = p->node.rb_left;
		else
			return p;
	}

	return NULL;
}

static bool ksu_ovl_calc_d_ino(struct ksu_ovl_readdir_data *rdd,
			   struct ksu_ovl_cache_entry *p)
{
	/* Don't care if not doing ksu_ovl_iter() */
	if (!rdd->dentry)
		return false;

	/* Always recalc d_ino when remapping lower inode numbers */
	if (ksu_ovl_xino_bits(rdd->dentry->d_sb))
		return true;

	/* Always recalc d_ino for parent */
	if (strcmp(p->name, "..") == 0)
		return true;

	/* If this is lower, then native d_ino will do */
	if (!rdd->is_upper)
		return false;

	/*
	 * Recalc d_ino for '.' and for all entries if dir is impure (contains
	 * copied up entries)
	 */
	if ((p->name[0] == '.' && p->len == 1) ||
	    ksu_ovl_test_flag(KSU_OVL_IMPURE, d_inode(rdd->dentry)))
		return true;

	return false;
}

static struct ksu_ovl_cache_entry *ksu_ovl_cache_entry_new(struct ksu_ovl_readdir_data *rdd,
						   const char *name, int len,
						   u64 ino, unsigned int d_type)
{
	struct ksu_ovl_cache_entry *p;
	size_t size = offsetof(struct ksu_ovl_cache_entry, name[len + 1]);

	p = kmalloc(size, GFP_KERNEL);
	if (!p)
		return NULL;

	memcpy(p->name, name, len);
	p->name[len] = '\0';
	p->len = len;
	p->type = d_type;
	p->real_ino = ino;
	p->ino = ino;
	/* Defer setting d_ino for upper entry to ksu_ovl_iterate() */
	if (ksu_ovl_calc_d_ino(rdd, p))
		p->ino = 0;
	p->is_upper = rdd->is_upper;
	p->is_whiteout = false;

	if (d_type == DT_CHR) {
		p->next_maybe_whiteout = rdd->first_maybe_whiteout;
		rdd->first_maybe_whiteout = p;
	}
	return p;
}

static int ksu_ovl_cache_entry_add_rb(struct ksu_ovl_readdir_data *rdd,
				  const char *name, int len, u64 ino,
				  unsigned int d_type)
{
	struct rb_node **newp = &rdd->root->rb_node;
	struct rb_node *parent = NULL;
	struct ksu_ovl_cache_entry *p;

	if (ksu_ovl_cache_entry_find_link(name, len, &newp, &parent))
		return 0;

	p = ksu_ovl_cache_entry_new(rdd, name, len, ino, d_type);
	if (p == NULL) {
		rdd->err = -ENOMEM;
		return -ENOMEM;
	}

	list_add_tail(&p->l_node, rdd->list);
	rb_link_node(&p->node, parent, newp);
	rb_insert_color(&p->node, rdd->root);

	return 0;
}

static int ksu_ovl_fill_lowest(struct ksu_ovl_readdir_data *rdd,
			   const char *name, int namelen,
			   loff_t offset, u64 ino, unsigned int d_type)
{
	struct ksu_ovl_cache_entry *p;

	p = ksu_ovl_cache_entry_find(rdd->root, name, namelen);
	if (p) {
		list_move_tail(&p->l_node, &rdd->middle);
	} else {
		p = ksu_ovl_cache_entry_new(rdd, name, namelen, ino, d_type);
		if (p == NULL)
			rdd->err = -ENOMEM;
		else
			list_add_tail(&p->l_node, &rdd->middle);
	}

	return rdd->err;
}

void ksu_ovl_cache_free(struct list_head *list)
{
	struct ksu_ovl_cache_entry *p;
	struct ksu_ovl_cache_entry *n;

	list_for_each_entry_safe(p, n, list, l_node)
		kfree(p);

	INIT_LIST_HEAD(list);
}

void ksu_ovl_dir_cache_free(struct inode *inode)
{
	struct ksu_ovl_dir_cache *cache = ksu_ovl_dir_cache(inode);

	if (cache) {
		ksu_ovl_cache_free(&cache->entries);
		kfree(cache);
	}
}

static void ksu_ovl_cache_put(struct ksu_ovl_dir_file *od, struct dentry *dentry)
{
	struct ksu_ovl_dir_cache *cache = od->cache;

	WARN_ON(cache->refcount <= 0);
	cache->refcount--;
	if (!cache->refcount) {
		if (ksu_ovl_dir_cache(d_inode(dentry)) == cache)
			ksu_ovl_set_dir_cache(d_inode(dentry), NULL);

		ksu_ovl_cache_free(&cache->entries);
		kfree(cache);
	}
}

static int ksu_ovl_fill_merge(struct dir_context *ctx, const char *name,
			  int namelen, loff_t offset, u64 ino,
			  unsigned int d_type)
{
	struct ksu_ovl_readdir_data *rdd =
		container_of(ctx, struct ksu_ovl_readdir_data, ctx);

	rdd->count++;
	if (!rdd->is_lowest)
		return ksu_ovl_cache_entry_add_rb(rdd, name, namelen, ino, d_type);
	else
		return ksu_ovl_fill_lowest(rdd, name, namelen, offset, ino, d_type);
}

static int ksu_ovl_check_whiteouts(struct dentry *dir, struct ksu_ovl_readdir_data *rdd)
{
	int err;
	struct ksu_ovl_cache_entry *p;
	struct dentry *dentry;
	const struct cred *old_cred;

	old_cred = ksu_ovl_override_creds(rdd->dentry->d_sb);

	err = down_write_killable(&dir->d_inode->i_rwsem);
	if (!err) {
		while (rdd->first_maybe_whiteout) {
			p = rdd->first_maybe_whiteout;
			rdd->first_maybe_whiteout = p->next_maybe_whiteout;
			dentry = lookup_one_len(p->name, dir, p->len);
			if (!IS_ERR(dentry)) {
				p->is_whiteout = ksu_ovl_is_whiteout(dentry);
				dput(dentry);
			}
		}
		inode_unlock(dir->d_inode);
	}
	ksu_ovl_revert_creds(rdd->dentry->d_sb, old_cred);

	return err;
}

static inline int ksu_ovl_dir_read(struct path *realpath,
			       struct ksu_ovl_readdir_data *rdd)
{
	struct file *realfile;
	int err;

	realfile = ksu_ovl_path_open(realpath, O_RDONLY | O_LARGEFILE);
	if (IS_ERR(realfile))
		return PTR_ERR(realfile);

	rdd->first_maybe_whiteout = NULL;
	rdd->ctx.pos = 0;
	do {
		rdd->count = 0;
		rdd->err = 0;
		err = iterate_dir(realfile, &rdd->ctx);
		if (err >= 0)
			err = rdd->err;
	} while (!err && rdd->count);

	if (!err && rdd->first_maybe_whiteout && rdd->dentry)
		err = ksu_ovl_check_whiteouts(realpath->dentry, rdd);

	fput(realfile);

	return err;
}

static void ksu_ovl_dir_reset(struct file *file)
{
	struct ksu_ovl_dir_file *od = file->private_data;
	struct ksu_ovl_dir_cache *cache = od->cache;
	struct dentry *dentry = file->f_path.dentry;
	bool is_real;

	if (cache && ksu_ovl_dentry_version_get(dentry) != cache->version) {
		ksu_ovl_cache_put(od, dentry);
		od->cache = NULL;
		od->cursor = NULL;
	}
	is_real = ksu_ovl_dir_is_real(dentry);
	if (od->is_real != is_real) {
		/* is_real can only become false when dir is copied up */
		if (WARN_ON(is_real))
			return;
		od->is_real = false;
	}
}

static int ksu_ovl_dir_read_merged(struct dentry *dentry, struct list_head *list,
	struct rb_root *root)
{
	int err;
	struct path realpath;
	struct ksu_ovl_readdir_data rdd = {
		.ctx.actor = ksu_ovl_fill_merge,
		.dentry = dentry,
		.list = list,
		.root = root,
		.is_lowest = false,
	};
	int idx, next;

	for (idx = 0; idx != -1; idx = next) {
		next = ksu_ovl_path_next(idx, dentry, &realpath);
		rdd.is_upper = ksu_ovl_dentry_upper(dentry) == realpath.dentry;

		if (next != -1) {
			err = ksu_ovl_dir_read(&realpath, &rdd);
			if (err)
				break;
		} else {
			/*
			 * Insert lowest layer entries before upper ones, this
			 * allows offsets to be reasonably constant
			 */
			list_add(&rdd.middle, rdd.list);
			rdd.is_lowest = true;
			err = ksu_ovl_dir_read(&realpath, &rdd);
			list_del(&rdd.middle);
		}
	}
	return err;
}

static void ksu_ovl_seek_cursor(struct ksu_ovl_dir_file *od, loff_t pos)
{
	struct list_head *p;
	loff_t off = 0;

	list_for_each(p, &od->cache->entries) {
		if (off >= pos)
			break;
		off++;
	}
	/* Cursor is safe since the cache is stable */
	od->cursor = p;
}

static struct ksu_ovl_dir_cache *ksu_ovl_cache_get(struct dentry *dentry)
{
	int res;
	struct ksu_ovl_dir_cache *cache;

	cache = ksu_ovl_dir_cache(d_inode(dentry));
	if (cache && ksu_ovl_dentry_version_get(dentry) == cache->version) {
		WARN_ON(!cache->refcount);
		cache->refcount++;
		return cache;
	}
	ksu_ovl_set_dir_cache(d_inode(dentry), NULL);

	cache = kzalloc(sizeof(struct ksu_ovl_dir_cache), GFP_KERNEL);
	if (!cache)
		return ERR_PTR(-ENOMEM);

	cache->refcount = 1;
	INIT_LIST_HEAD(&cache->entries);
	cache->root = RB_ROOT;

	res = ksu_ovl_dir_read_merged(dentry, &cache->entries, &cache->root);
	if (res) {
		ksu_ovl_cache_free(&cache->entries);
		kfree(cache);
		return ERR_PTR(res);
	}

	cache->version = ksu_ovl_dentry_version_get(dentry);
	ksu_ovl_set_dir_cache(d_inode(dentry), cache);

	return cache;
}

/* Map inode number to lower fs unique range */
static u64 ksu_ovl_remap_lower_ino(u64 ino, int xinobits, int fsid,
			       const char *name, int namelen, bool warn)
{
	unsigned int xinoshift = 64 - xinobits;

	if (unlikely(ino >> xinoshift)) {
		if (warn) {
			pr_warn_ratelimited("d_ino too big (%.*s, ino=%llu, xinobits=%d)\n",
					    namelen, name, ino, xinobits);
		}
		return ino;
	}

	/*
	 * The lowest xinobit is reserved for mapping the non-peresistent inode
	 * numbers range, but this range is only exposed via st_ino, not here.
	 */
	return ino | ((u64)fsid) << (xinoshift + 1);
}

/*
 * Set d_ino for upper entries. Non-upper entries should always report
 * the uppermost real inode ino and should not call this function.
 *
 * When not all layer are on same fs, report real ino also for upper.
 *
 * When all layers are on the same fs, and upper has a reference to
 * copy up origin, call vfs_getattr() on the overlay entry to make
 * sure that d_ino will be consistent with st_ino from stat(2).
 */
static int ksu_ovl_cache_update_ino(struct path *path, struct ksu_ovl_cache_entry *p)

{
	struct dentry *dir = path->dentry;
	struct dentry *this = NULL;
	enum ksu_ovl_path_type type;
	u64 ino = p->real_ino;
	int xinobits = ksu_ovl_xino_bits(dir->d_sb);
	int err = 0;

	if (!ksu_ovl_same_dev(dir->d_sb))
		goto out;

	if (p->name[0] == '.') {
		if (p->len == 1) {
			this = dget(dir);
			goto get;
		}
		if (p->len == 2 && p->name[1] == '.') {
			/* we shall not be moved */
			this = dget(dir->d_parent);
			goto get;
		}
	}
	this = lookup_one_len(p->name, dir, p->len);
	if (IS_ERR_OR_NULL(this) || !this->d_inode) {
		if (IS_ERR(this)) {
			err = PTR_ERR(this);
			this = NULL;
			goto fail;
		}
		goto out;
	}

get:
	type = ksu_ovl_path_type(this);
	if (KSU_OVL_TYPE_ORIGIN(type)) {
		struct kstat stat;
		struct path statpath = *path;

		statpath.dentry = this;
		err = vfs_getattr(&statpath, &stat, STATX_INO, 0);
		if (err)
			goto fail;

		/*
		 * Directory inode is always on overlay st_dev.
		 * Non-dir with ksu_ovl_same_dev() could be on pseudo st_dev in case
		 * of xino bits overflow.
		 */
		WARN_ON_ONCE(S_ISDIR(stat.mode) &&
			     dir->d_sb->s_dev != stat.dev);
		ino = stat.ino;
	} else if (xinobits && !KSU_OVL_TYPE_UPPER(type)) {
		ino = ksu_ovl_remap_lower_ino(ino, xinobits,
					  ksu_ovl_layer_lower(this)->fsid,
					  p->name, p->len,
					  ksu_ovl_xino_warn(dir->d_sb));
	}

out:
	p->ino = ino;
	dput(this);
	return err;

fail:
	pr_warn_ratelimited("failed to look up (%s) for ino (%i)\n",
			    p->name, err);
	goto out;
}

static int ksu_ovl_fill_plain(struct dir_context *ctx, const char *name,
			  int namelen, loff_t offset, u64 ino,
			  unsigned int d_type)
{
	struct ksu_ovl_cache_entry *p;
	struct ksu_ovl_readdir_data *rdd =
		container_of(ctx, struct ksu_ovl_readdir_data, ctx);

	rdd->count++;
	p = ksu_ovl_cache_entry_new(rdd, name, namelen, ino, d_type);
	if (p == NULL) {
		rdd->err = -ENOMEM;
		return -ENOMEM;
	}
	list_add_tail(&p->l_node, rdd->list);

	return 0;
}

static int ksu_ovl_dir_read_impure(struct path *path,  struct list_head *list,
			       struct rb_root *root)
{
	int err;
	struct path realpath;
	struct ksu_ovl_cache_entry *p, *n;
	struct ksu_ovl_readdir_data rdd = {
		.ctx.actor = ksu_ovl_fill_plain,
		.list = list,
		.root = root,
	};

	INIT_LIST_HEAD(list);
	*root = RB_ROOT;
	ksu_ovl_path_upper(path->dentry, &realpath);

	err = ksu_ovl_dir_read(&realpath, &rdd);
	if (err)
		return err;

	list_for_each_entry_safe(p, n, list, l_node) {
		if (strcmp(p->name, ".") != 0 &&
		    strcmp(p->name, "..") != 0) {
			err = ksu_ovl_cache_update_ino(path, p);
			if (err)
				return err;
		}
		if (p->ino == p->real_ino) {
			list_del(&p->l_node);
			kfree(p);
		} else {
			struct rb_node **newp = &root->rb_node;
			struct rb_node *parent = NULL;

			if (WARN_ON(ksu_ovl_cache_entry_find_link(p->name, p->len,
							      &newp, &parent)))
				return -EIO;

			rb_link_node(&p->node, parent, newp);
			rb_insert_color(&p->node, root);
		}
	}
	return 0;
}

static struct ksu_ovl_dir_cache *ksu_ovl_cache_get_impure(struct path *path)
{
	int res;
	struct dentry *dentry = path->dentry;
	struct ksu_ovl_fs *ofs = KSU_OVL_FS(dentry->d_sb);
	struct ksu_ovl_dir_cache *cache;

	cache = ksu_ovl_dir_cache(d_inode(dentry));
	if (cache && ksu_ovl_dentry_version_get(dentry) == cache->version)
		return cache;

	/* Impure cache is not refcounted, free it here */
	ksu_ovl_dir_cache_free(d_inode(dentry));
	ksu_ovl_set_dir_cache(d_inode(dentry), NULL);

	cache = kzalloc(sizeof(struct ksu_ovl_dir_cache), GFP_KERNEL);
	if (!cache)
		return ERR_PTR(-ENOMEM);

	res = ksu_ovl_dir_read_impure(path, &cache->entries, &cache->root);
	if (res) {
		ksu_ovl_cache_free(&cache->entries);
		kfree(cache);
		return ERR_PTR(res);
	}
	if (list_empty(&cache->entries)) {
		/*
		 * A good opportunity to get rid of an unneeded "impure" flag.
		 * Removing the "impure" xattr is best effort.
		 */
		if (!ksu_ovl_want_write(dentry)) {
			ksu_ovl_do_removexattr(ofs, ksu_ovl_dentry_upper(dentry),
					   KSU_OVL_XATTR_IMPURE);
			ksu_ovl_drop_write(dentry);
		}
		ksu_ovl_clear_flag(KSU_OVL_IMPURE, d_inode(dentry));
		kfree(cache);
		return NULL;
	}

	cache->version = ksu_ovl_dentry_version_get(dentry);
	ksu_ovl_set_dir_cache(d_inode(dentry), cache);

	return cache;
}

struct ksu_ovl_readdir_translate {
	struct dir_context *orig_ctx;
	struct ksu_ovl_dir_cache *cache;
	struct dir_context ctx;
	u64 parent_ino;
	int fsid;
	int xinobits;
	bool xinowarn;
};

static int ksu_ovl_fill_real(struct dir_context *ctx, const char *name,
			   int namelen, loff_t offset, u64 ino,
			   unsigned int d_type)
{
	struct ksu_ovl_readdir_translate *rdt =
		container_of(ctx, struct ksu_ovl_readdir_translate, ctx);
	struct dir_context *orig_ctx = rdt->orig_ctx;

	if (rdt->parent_ino && strcmp(name, "..") == 0) {
		ino = rdt->parent_ino;
	} else if (rdt->cache) {
		struct ksu_ovl_cache_entry *p;

		p = ksu_ovl_cache_entry_find(&rdt->cache->root, name, namelen);
		if (p)
			ino = p->ino;
	} else if (rdt->xinobits) {
		ino = ksu_ovl_remap_lower_ino(ino, rdt->xinobits, rdt->fsid,
					  name, namelen, rdt->xinowarn);
	}

	return orig_ctx->actor(orig_ctx, name, namelen, offset, ino, d_type);
}

static bool ksu_ovl_is_impure_dir(struct file *file)
{
	struct ksu_ovl_dir_file *od = file->private_data;
	struct inode *dir = d_inode(file->f_path.dentry);

	/*
	 * Only upper dir can be impure, but if we are in the middle of
	 * iterating a lower real dir, dir could be copied up and marked
	 * impure. We only want the impure cache if we started iterating
	 * a real upper dir to begin with.
	 */
	return od->is_upper && ksu_ovl_test_flag(KSU_OVL_IMPURE, dir);

}

static int ksu_ovl_iterate_real(struct file *file, struct dir_context *ctx)
{
	int err;
	struct ksu_ovl_dir_file *od = file->private_data;
	struct dentry *dir = file->f_path.dentry;
	const struct ksu_ovl_layer *lower_layer = ksu_ovl_layer_lower(dir);
	struct ksu_ovl_readdir_translate rdt = {
		.ctx.actor = ksu_ovl_fill_real,
		.orig_ctx = ctx,
		.xinobits = ksu_ovl_xino_bits(dir->d_sb),
		.xinowarn = ksu_ovl_xino_warn(dir->d_sb),
	};

	if (rdt.xinobits && lower_layer)
		rdt.fsid = lower_layer->fsid;

	if (KSU_OVL_TYPE_MERGE(ksu_ovl_path_type(dir->d_parent))) {
		struct kstat stat;
		struct path statpath = file->f_path;

		statpath.dentry = dir->d_parent;
		err = vfs_getattr(&statpath, &stat, STATX_INO, 0);
		if (err)
			return err;

		WARN_ON_ONCE(dir->d_sb->s_dev != stat.dev);
		rdt.parent_ino = stat.ino;
	}

	if (ksu_ovl_is_impure_dir(file)) {
		rdt.cache = ksu_ovl_cache_get_impure(&file->f_path);
		if (IS_ERR(rdt.cache))
			return PTR_ERR(rdt.cache);
	}

	err = iterate_dir(od->realfile, &rdt.ctx);
	ctx->pos = rdt.ctx.pos;

	return err;
}


static int ksu_ovl_iterate(struct file *file, struct dir_context *ctx)
{
	struct ksu_ovl_dir_file *od = file->private_data;
	struct dentry *dentry = file->f_path.dentry;
	struct ksu_ovl_cache_entry *p;
	const struct cred *old_cred;
	int err;

	old_cred = ksu_ovl_override_creds(dentry->d_sb);
	if (!ctx->pos)
		ksu_ovl_dir_reset(file);

	if (od->is_real) {
		/*
		 * If parent is merge, then need to adjust d_ino for '..', if
		 * dir is impure then need to adjust d_ino for copied up
		 * entries.
		 */
		if (ksu_ovl_xino_bits(dentry->d_sb) ||
		    (ksu_ovl_same_fs(dentry->d_sb) &&
		     (ksu_ovl_is_impure_dir(file) ||
		      KSU_OVL_TYPE_MERGE(ksu_ovl_path_type(dentry->d_parent))))) {
			err = ksu_ovl_iterate_real(file, ctx);
		} else {
			err = iterate_dir(od->realfile, ctx);
		}
		goto out;
	}

	if (!od->cache) {
		struct ksu_ovl_dir_cache *cache;

		cache = ksu_ovl_cache_get(dentry);
		err = PTR_ERR(cache);
		if (IS_ERR(cache))
			goto out;

		od->cache = cache;
		ksu_ovl_seek_cursor(od, ctx->pos);
	}

	while (od->cursor != &od->cache->entries) {
		p = list_entry(od->cursor, struct ksu_ovl_cache_entry, l_node);
		if (!p->is_whiteout) {
			if (!p->ino) {
				err = ksu_ovl_cache_update_ino(&file->f_path, p);
				if (err)
					goto out;
			}
			if (!dir_emit(ctx, p->name, p->len, p->ino, p->type))
				break;
		}
		od->cursor = p->l_node.next;
		ctx->pos++;
	}
	err = 0;
out:
	ksu_ovl_revert_creds(dentry->d_sb, old_cred);
	return err;
}

static loff_t ksu_ovl_dir_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t res;
	struct ksu_ovl_dir_file *od = file->private_data;

	inode_lock(file_inode(file));
	if (!file->f_pos)
		ksu_ovl_dir_reset(file);

	if (od->is_real) {
		res = vfs_llseek(od->realfile, offset, origin);
		file->f_pos = od->realfile->f_pos;
	} else {
		res = -EINVAL;

		switch (origin) {
		case SEEK_CUR:
			offset += file->f_pos;
			break;
		case SEEK_SET:
			break;
		default:
			goto out_unlock;
		}
		if (offset < 0)
			goto out_unlock;

		if (offset != file->f_pos) {
			file->f_pos = offset;
			if (od->cache)
				ksu_ovl_seek_cursor(od, offset);
		}
		res = offset;
	}
out_unlock:
	inode_unlock(file_inode(file));

	return res;
}

static struct file *ksu_ovl_dir_open_realfile(const struct file *file,
					  struct path *realpath)
{
	struct file *res;
	const struct cred *old_cred;

	old_cred = ksu_ovl_override_creds(file_inode(file)->i_sb);
	res = ksu_ovl_path_open(realpath, O_RDONLY | (file->f_flags & O_LARGEFILE));
	ksu_ovl_revert_creds(file_inode(file)->i_sb, old_cred);

	return res;
}

/*
 * Like ksu_ovl_real_fdget(), returns upperfile if dir was copied up since open.
 * Unlike ksu_ovl_real_fdget(), this caches upperfile in file->private_data.
 *
 * TODO: use same abstract type for file->private_data of dir and file so
 * upperfile could also be cached for files as well.
 */
struct file *ksu_ovl_dir_real_file(const struct file *file, bool want_upper)
{

	struct ksu_ovl_dir_file *od = file->private_data;
	struct dentry *dentry = file->f_path.dentry;
	struct file *old, *realfile = od->realfile;

	if (!KSU_OVL_TYPE_UPPER(ksu_ovl_path_type(dentry)))
		return want_upper ? NULL : realfile;

	/*
	 * Need to check if we started out being a lower dir, but got copied up
	 */
	if (!od->is_upper) {
		realfile = READ_ONCE(od->upperfile);
		if (!realfile) {
			struct path upperpath;

			ksu_ovl_path_upper(dentry, &upperpath);
			realfile = ksu_ovl_dir_open_realfile(file, &upperpath);
			if (IS_ERR(realfile))
				return realfile;

			old = cmpxchg_release(&od->upperfile, NULL, realfile);
			if (old) {
				fput(realfile);
				realfile = old;
			}
		}
	}

	return realfile;
}

static int ksu_ovl_dir_fsync(struct file *file, loff_t start, loff_t end,
			 int datasync)
{
	struct file *realfile;
	int err;

	err = ksu_ovl_sync_status(KSU_OVL_FS(file->f_path.dentry->d_sb));
	if (err <= 0)
		return err;

	realfile = ksu_ovl_dir_real_file(file, true);
	err = PTR_ERR_OR_ZERO(realfile);

	/* Nothing to sync for lower */
	if (!realfile || err)
		return err;

	return vfs_fsync_range(realfile, start, end, datasync);
}

static int ksu_ovl_dir_release(struct inode *inode, struct file *file)
{
	struct ksu_ovl_dir_file *od = file->private_data;

	if (od->cache) {
		inode_lock(inode);
		ksu_ovl_cache_put(od, file->f_path.dentry);
		inode_unlock(inode);
	}
	fput(od->realfile);
	if (od->upperfile)
		fput(od->upperfile);
	kfree(od);

	return 0;
}

static int ksu_ovl_dir_open(struct inode *inode, struct file *file)
{
	struct path realpath;
	struct file *realfile;
	struct ksu_ovl_dir_file *od;
	enum ksu_ovl_path_type type;

	od = kzalloc(sizeof(struct ksu_ovl_dir_file), GFP_KERNEL);
	if (!od)
		return -ENOMEM;

	type = ksu_ovl_path_real(file->f_path.dentry, &realpath);
	realfile = ksu_ovl_dir_open_realfile(file, &realpath);
	if (IS_ERR(realfile)) {
		kfree(od);
		return PTR_ERR(realfile);
	}
	od->realfile = realfile;
	od->is_real = ksu_ovl_dir_is_real(file->f_path.dentry);
	od->is_upper = KSU_OVL_TYPE_UPPER(type);
	file->private_data = od;

	return 0;
}

const struct file_operations ksu_ovl_dir_operations = {
	.read		= generic_read_dir,
	.open		= ksu_ovl_dir_open,
	.iterate	= ksu_ovl_iterate,
	.llseek		= ksu_ovl_dir_llseek,
	.fsync		= ksu_ovl_dir_fsync,
	.release	= ksu_ovl_dir_release,
	.unlocked_ioctl	= ksu_ovl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ksu_ovl_compat_ioctl,
#endif
};

int ksu_ovl_check_empty_dir(struct dentry *dentry, struct list_head *list)
{
	int err;
	struct ksu_ovl_cache_entry *p, *n;
	struct rb_root root = RB_ROOT;
	const struct cred *old_cred;

	old_cred = ksu_ovl_override_creds(dentry->d_sb);
	err = ksu_ovl_dir_read_merged(dentry, list, &root);
	ksu_ovl_revert_creds(dentry->d_sb, old_cred);
	if (err)
		return err;

	err = 0;

	list_for_each_entry_safe(p, n, list, l_node) {
		/*
		 * Select whiteouts in upperdir, they should
		 * be cleared when deleting this directory.
		 */
		if (p->is_whiteout) {
			if (p->is_upper)
				continue;
			goto del_entry;
		}

		if (p->name[0] == '.') {
			if (p->len == 1)
				goto del_entry;
			if (p->len == 2 && p->name[1] == '.')
				goto del_entry;
		}
		err = -ENOTEMPTY;
		break;

del_entry:
		list_del(&p->l_node);
		kfree(p);
	}

	return err;
}

void ksu_ovl_cleanup_whiteouts(struct dentry *upper, struct list_head *list)
{
	struct ksu_ovl_cache_entry *p;

	inode_lock_nested(upper->d_inode, I_MUTEX_CHILD);
	list_for_each_entry(p, list, l_node) {
		struct dentry *dentry;

		if (WARN_ON(!p->is_whiteout || !p->is_upper))
			continue;

		dentry = lookup_one_len(p->name, upper, p->len);
		if (IS_ERR(dentry)) {
			pr_err("lookup '%s/%.*s' failed (%i)\n",
			       upper->d_name.name, p->len, p->name,
			       (int) PTR_ERR(dentry));
			continue;
		}
		if (dentry->d_inode)
			ksu_ovl_cleanup(upper->d_inode, dentry);
		dput(dentry);
	}
	inode_unlock(upper->d_inode);
}

static int ksu_ovl_check_d_type(struct dir_context *ctx, const char *name,
			  int namelen, loff_t offset, u64 ino,
			  unsigned int d_type)
{
	struct ksu_ovl_readdir_data *rdd =
		container_of(ctx, struct ksu_ovl_readdir_data, ctx);

	/* Even if d_type is not supported, DT_DIR is returned for . and .. */
	if (!strncmp(name, ".", namelen) || !strncmp(name, "..", namelen))
		return 0;

	if (d_type != DT_UNKNOWN)
		rdd->d_type_supported = true;

	return 0;
}

/*
 * Returns 1 if d_type is supported, 0 not supported/unknown. Negative values
 * if error is encountered.
 */
int ksu_ovl_check_d_type_supported(struct path *realpath)
{
	int err;
	struct ksu_ovl_readdir_data rdd = {
		.ctx.actor = ksu_ovl_check_d_type,
		.d_type_supported = false,
	};

	err = ksu_ovl_dir_read(realpath, &rdd);
	if (err)
		return err;

	return rdd.d_type_supported;
}

#define KSU_OVL_INCOMPATDIR_NAME "incompat"

static int ksu_ovl_workdir_cleanup_recurse(struct path *path, int level)
{
	int err;
	struct inode *dir = path->dentry->d_inode;
	LIST_HEAD(list);
	struct rb_root root = RB_ROOT;
	struct ksu_ovl_cache_entry *p;
	struct ksu_ovl_readdir_data rdd = {
		.ctx.actor = ksu_ovl_fill_merge,
		.dentry = NULL,
		.list = &list,
		.root = &root,
		.is_lowest = false,
	};
	bool incompat = false;

	/*
	 * The "work/incompat" directory is treated specially - if it is not
	 * empty, instead of printing a generic error and mounting read-only,
	 * we will error about incompat features and fail the mount.
	 *
	 * When called from ksu_ovl_indexdir_cleanup(), path->dentry->d_name.name
	 * starts with '#'.
	 */
	if (level == 2 &&
	    !strcmp(path->dentry->d_name.name, KSU_OVL_INCOMPATDIR_NAME))
		incompat = true;

	err = ksu_ovl_dir_read(path, &rdd);
	if (err)
		goto out;

	inode_lock_nested(dir, I_MUTEX_PARENT);
	list_for_each_entry(p, &list, l_node) {
		struct dentry *dentry;

		if (p->name[0] == '.') {
			if (p->len == 1)
				continue;
			if (p->len == 2 && p->name[1] == '.')
				continue;
		} else if (incompat) {
			pr_err("overlay with incompat feature '%s' cannot be mounted\n",
				p->name);
			err = -EINVAL;
			break;
		}
		dentry = lookup_one_len(p->name, path->dentry, p->len);
		if (IS_ERR(dentry))
			continue;
		if (dentry->d_inode)
			err = ksu_ovl_workdir_cleanup(dir, path->mnt, dentry, level);
		dput(dentry);
		if (err)
			break;
	}
	inode_unlock(dir);
out:
	ksu_ovl_cache_free(&list);
	return err;
}

int ksu_ovl_workdir_cleanup(struct inode *dir, struct vfsmount *mnt,
			 struct dentry *dentry, int level)
{
	int err;

	if (!d_is_dir(dentry) || level > 1) {
		return ksu_ovl_cleanup(dir, dentry);
	}

	err = ksu_ovl_do_rmdir(dir, dentry);
	if (err) {
		struct path path = { .mnt = mnt, .dentry = dentry };

		inode_unlock(dir);
		err = ksu_ovl_workdir_cleanup_recurse(&path, level + 1);
		inode_lock_nested(dir, I_MUTEX_PARENT);
		if (!err)
			err = ksu_ovl_cleanup(dir, dentry);
	}

	return err;
}

int ksu_ovl_indexdir_cleanup(struct ksu_ovl_fs *ofs)
{
	int err;
	struct dentry *indexdir = ofs->indexdir;
	struct dentry *index = NULL;
	struct inode *dir = indexdir->d_inode;
	struct path path = { .mnt = ksu_ovl_upper_mnt(ofs), .dentry = indexdir };
	LIST_HEAD(list);
	struct rb_root root = RB_ROOT;
	struct ksu_ovl_cache_entry *p;
	struct ksu_ovl_readdir_data rdd = {
		.ctx.actor = ksu_ovl_fill_merge,
		.dentry = NULL,
		.list = &list,
		.root = &root,
		.is_lowest = false,
	};

	err = ksu_ovl_dir_read(&path, &rdd);
	if (err)
		goto out;

	inode_lock_nested(dir, I_MUTEX_PARENT);
	list_for_each_entry(p, &list, l_node) {
		if (p->name[0] == '.') {
			if (p->len == 1)
				continue;
			if (p->len == 2 && p->name[1] == '.')
				continue;
		}
		index = lookup_one_len(p->name, indexdir, p->len);
		if (IS_ERR(index)) {
			err = PTR_ERR(index);
			index = NULL;
			break;
		}
		/* Cleanup leftover from index create/cleanup attempt */
		if (index->d_name.name[0] == '#') {
			err = ksu_ovl_workdir_cleanup(dir, path.mnt, index, 1);
			if (err)
				break;
			goto next;
		}
		err = ksu_ovl_verify_index(ofs, index);
		if (!err) {
			goto next;
		} else if (err == -ESTALE) {
			/* Cleanup stale index entries */
			err = ksu_ovl_cleanup(dir, index);
		} else if (err != -ENOENT) {
			/*
			 * Abort mount to avoid corrupting the index if
			 * an incompatible index entry was found or on out
			 * of memory.
			 */
			break;
		} else if (ofs->config.nfs_export) {
			/*
			 * Whiteout orphan index to block future open by
			 * handle after overlay nlink dropped to zero.
			 */
			err = ksu_ovl_cleanup_and_whiteout(ofs, dir, index);
		} else {
			/* Cleanup orphan index entries */
			err = ksu_ovl_cleanup(dir, index);
		}

		if (err)
			break;

next:
		dput(index);
		index = NULL;
	}
	dput(index);
	inode_unlock(dir);
out:
	ksu_ovl_cache_free(&list);
	if (err)
		pr_err("failed index dir cleanup (%i)\n", err);
	return err;
}
