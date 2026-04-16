#ifndef __KSU_UAPI_PROCESS_TAG_H
#define __KSU_UAPI_PROCESS_TAG_H

#include <linux/types.h>

enum process_tag_type : __u8 {
    PROCESS_TAG_NONE = 0,
    PROCESS_TAG_KSUD = 1,
    PROCESS_TAG_APP = 2,
    PROCESS_TAG_MODULE = 3,
    PROCESS_TAG_MANAGER = 4,
};

#endif
