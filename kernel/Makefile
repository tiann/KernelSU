kernelsu-objs := ksu.o
kernelsu-objs += allowlist.o
kernelsu-objs += apk_sign.o
kernelsu-objs += sucompat.o
kernelsu-objs += throne_tracker.o
kernelsu-objs += core_hook.o
kernelsu-objs += ksud.o
kernelsu-objs += embed_ksud.o
kernelsu-objs += kernel_compat.o

kernelsu-objs += selinux/selinux.o
kernelsu-objs += selinux/sepolicy.o
kernelsu-objs += selinux/rules.o
ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include
ccflags-y += -I$(objtree)/security/selinux -include $(srctree)/include/uapi/asm-generic/errno.h

obj-$(CONFIG_KSU) += kernelsu.o

IS_KERNEL_69 := $(strip $(shell \
    if [ "$(VERSION)" -ge "6" -a "$(PATCHLEVEL)" -ge "9" ]; then \
      echo TRUE; \
    else \
      echo FALSE; \
    fi \
  ))

# https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=b1992c3772e69a6fd0e3fc81cd4d2820c8b6eca0
ifeq ($(IS_KERNEL_69),TRUE)
SRC_LOCATION := $(src)
else
SRC_LOCATION := $(srctree)/$(src)
endif

GIT_TOOL := /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin git

# Check for both https & ssh remote
REMOTE_CHECK := $(shell cd $(SRC_LOCATION); $(GIT_TOOL) remote -v | grep -E 'github\.com[/:]tiann/KernelSU')

ifneq ($(REMOTE_CHECK),)
ifeq ($(shell cd $(SRC_LOCATION); $(GIT_TOOL) rev-parse --is-shallow-repository),true)
$(shell cd $(SRC_LOCATION); $(GIT_TOOL) fetch --unshallow)
endif
KSU_GIT_VERSION := $(shell cd $(SRC_LOCATION); $(GIT_TOOL) rev-list --count HEAD)
# ksu_version: major * 10000 + git version + 200 for historical reasons
$(eval KSU_VERSION=$(shell expr 10000 + $(KSU_GIT_VERSION) + 200))
$(info -- KernelSU version: $(KSU_VERSION))
ccflags-y += -DKSU_VERSION=$(KSU_VERSION)
else # Pass default version if the defined remote is not found
$(warning "KSU_GIT_VERSION not defined! It is better to make KernelSU a git submodule!")
ccflags-y += -DKSU_VERSION=16
endif

ifndef KSU_EXPECTED_SIZE
KSU_EXPECTED_SIZE := 0x033b
endif

ifndef KSU_EXPECTED_HASH
KSU_EXPECTED_HASH := c371061b19d8c7d7d6133c6a9bafe198fa944e50c1b31c9d8daa8d7f1fc2d2d6
endif

ifdef KSU_MANAGER_PACKAGE
ccflags-y += -DKSU_MANAGER_PACKAGE=\"$(KSU_MANAGER_PACKAGE)\"
$(info -- KernelSU Manager package name: $(KSU_MANAGER_PACKAGE))
endif

$(info -- KernelSU Manager signature size: $(KSU_EXPECTED_SIZE))
$(info -- KernelSU Manager signature hash: $(KSU_EXPECTED_HASH))

ccflags-y += -DEXPECTED_SIZE=$(KSU_EXPECTED_SIZE)
ccflags-y += -DEXPECTED_HASH=\"$(KSU_EXPECTED_HASH)\"

ccflags-y += -Wno-implicit-function-declaration -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat
ccflags-y += -Wno-declaration-after-statement -Wno-unused-function

# Keep a new line here!! Because someone may append config
