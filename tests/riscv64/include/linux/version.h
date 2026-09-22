#ifndef KSU_TEST_LINUX_VERSION_H
#define KSU_TEST_LINUX_VERSION_H
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#define LINUX_VERSION_CODE KERNEL_VERSION(7, 1, 0)
#endif
