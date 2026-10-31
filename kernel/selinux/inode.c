#include <linux/errno.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "objsec.h"
#include "selinux/selinux.h"
#include "infra/btf_parser.h"
#include "klog.h"

#if defined(MODULE) && defined(CONFIG_DEBUG_INFO_BTF)
#include <uapi/linux/btf.h>

static u32 inode_blob_offset __ro_after_init;

int __init ksu_selinux_inode_init(void)
{
    struct ksu_btf_field blob_field;
    s32 blob_offset;
    int ret;

    ret = ksu_btf_field("lsm_blob_sizes", "lbs_inode", BTF_KIND_INT, sizeof(int), &blob_field);
    if (ret)
        goto fail;
    ret = copy_from_kernel_nofault(&blob_offset, (const char *)&selinux_blob_sizes + blob_field.offset,
                                   sizeof(blob_offset));
    if (ret) {
        pr_err("selinux_blob_sizes.lbs_inode unreadable (%d)\n", ret);
        goto fail;
    }
    if (blob_offset < 0 || blob_offset % __alignof__(struct inode_security_struct)) {
        ret = -EINVAL;
        goto fail;
    }
    inode_blob_offset = blob_offset;
    pr_info("offset from vmlinux.btf, lbs_inode@%u, blob=%u\n", blob_field.offset, inode_blob_offset);
    return 0;
fail:
    pr_err("vmlinux BTF (%d) unreadable!\n", ret);
    return ret;
}
#endif

struct inode_security_struct *ksu_selinux_inode(const struct inode *inode)
{
#if defined(MODULE) && defined(CONFIG_DEBUG_INFO_BTF)
    struct inode_security_struct *security;

    if (!inode->i_security)
        return NULL;
    security = (void *)((char *)inode->i_security + inode_blob_offset);
    if (READ_ONCE(security->inode) != inode) {
        pr_err("Can not resolve inode_security_struct offset, current offset got: %d\n", inode_blob_offset);
        return ERR_PTR(-EINVAL);
    }
    return security;
#else
    return selinux_inode(inode);
#endif
}
