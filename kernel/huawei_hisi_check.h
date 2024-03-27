#ifndef _HUAWEI_HISI_CHECK_H
#define _HUAWEI_HISI_CHECK_H


#include "linux/version.h"
#include "ss/policydb.h"

/*
 * Adapt to Huawei HISI kernel without affecting other kernels ,
 * Huawei Hisi Kernel EBITMAP Enable or Disable Flag ,
 * From ss/ebitmap.h
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)) &&                           \
		(LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)) ||               \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)) &&                      \
		(LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
#ifdef HISI_SELINUX_EBITMAP_RO
#define CONFIG_IS_HW_HISI
#endif
#endif

#endif /* _HUAWEI_HISI_CHECK_H */