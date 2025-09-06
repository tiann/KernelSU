// SPDX-License-Identifier: GPL-2.0

#include <asm-generic/sections.h>
#include <linux/mm.h>
#include "../mm/slab.h"
#include <linux/slub_def.h>
#include <linux/binfmts.h>

#include <linux/mount.h>
#include <linux/cred.h>
#include <linux/security.h>
#include <linux/init_task.h>
#include "../fs/mount.h"

void kdp_usecount_inc(struct cred *cred)
{
	atomic_long_inc(&cred->usage);
}
EXPORT_SYMBOL(kdp_usecount_inc);

unsigned int kdp_usecount_dec_and_test(struct cred *cred)
{
	return atomic_long_dec_and_test(&cred->usage);
}
EXPORT_SYMBOL(kdp_usecount_dec_and_test);

void kdp_set_cred_non_rcu(struct cred *cred, int val)
{
	cred->non_rcu = val;
}
EXPORT_SYMBOL(kdp_set_cred_non_rcu);
