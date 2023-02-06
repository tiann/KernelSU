obj-y += ksu.o
obj-y += allowlist.o
kernelsu-objs := apk_sign.o
obj-y += kernelsu.o
obj-y += module_api.o
obj-y += sucompat.o
obj-y += uid_observer.o
obj-y += manager.o
obj-y += core_hook.o
obj-y += ksud.o
obj-y += embed_ksud.o
obj-y += kernel_compat.o

obj-y += selinux/

ifeq ($(shell test -d $(srctree)/$(src)/../.git; echo $$?),1)
# not single git repo

# Set mannually
# KSU_GIT_VERSION := 123

# Use remote git version
# KSU_GIT_VERSION := $(shell /usr/bin/python3 $(src)/../scripts/get_commits.py tiann KernelSU main)

ifeq ($(KSU_GIT_VERSION),)
$(error Please set KSU_GIT_VERSION in your Makefile)
endif

else

KSU_GIT_VERSION := $(shell cd $(srctree)/$(src);/usr/bin/git rev-list --count HEAD)
ifeq ($(KSU_GIT_VERSION),)
$(error Get git version failed, please make sure git is installed in /usr/bin/git)
endif

endif


ifeq ($(KSU_GIT_VERSION),)
$(error Get git version failed)
endif

ccflags-y += -DKSU_GIT_VERSION=$(KSU_GIT_VERSION)

ifndef EXPECTED_SIZE
EXPECTED_SIZE := 0x033b
endif

ifndef EXPECTED_HASH
EXPECTED_HASH := 0xb0b91415
endif

ccflags-y += -DEXPECTED_SIZE=$(EXPECTED_SIZE)
ccflags-y += -DEXPECTED_HASH=$(EXPECTED_HASH)
ccflags-y += -Wno-implicit-function-declaration -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat
ccflags-y += -Wno-macro-redefined -Wno-declaration-after-statement
