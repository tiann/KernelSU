kernelsu-objs := ksu.o
kernelsu-objs += allowlist.o
kernelsu-objs += app_profile.o
kernelsu-objs += apk_sign.o
kernelsu-objs += sucompat.o
kernelsu-objs += hook_manager.o
kernelsu-objs += throne_tracker.o
kernelsu-objs += pkg_observer.o
kernelsu-objs += core_hook.o
kernelsu-objs += supercalls.o
kernelsu-objs += feature.o
kernelsu-objs += ksud.o
kernelsu-objs += embed_ksud.o
kernelsu-objs += seccomp_cache.o
kernelsu-objs += file_wrapper.o

kernelsu-objs += selinux/selinux.o
kernelsu-objs += selinux/sepolicy.o
kernelsu-objs += selinux/rules.o
ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include
ccflags-y += -I$(objtree)/security/selinux -include $(srctree)/include/uapi/asm-generic/errno.h

obj-$(CONFIG_KSU) += kernelsu.o

KDIR := $(KDIR)
MDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

$(info -- KDIR: $(KDIR))
$(info -- MDIR: $(MDIR))

# Check if this is a git repository
# For in-tree build: check $(srctree)/$(src)/../.git
# For out-of-tree build: check $(MDIR)/../.git
ifeq ($(shell test -e $(srctree)/$(src)/../.git && echo "in-tree"),in-tree)
# In-tree build (git submodule)
$(shell cd $(srctree)/$(src); /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin [ -f ../.git/shallow ] && git fetch --unshallow)
KSU_GIT_VERSION := $(shell cd $(srctree)/$(src); /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin git rev-list --count HEAD)
KSU_GIT_VERSION_VALID := 1
else ifeq ($(shell test -e $(MDIR)/../.git && echo "out-of-tree"),out-of-tree)
# Out-of-tree build (standalone repository)
$(shell cd $(MDIR); /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin [ -f ../.git/shallow ] && git fetch --unshallow)
KSU_GIT_VERSION := $(shell cd $(MDIR); /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin git rev-list --count HEAD)
KSU_GIT_VERSION_VALID := 1
endif

# Calculate version if git version is available
ifdef KSU_GIT_VERSION_VALID
# ksu_version: major * 10000 + git version + 200 for historical reasons
$(eval KSU_VERSION=$(shell expr 20000 + $(KSU_GIT_VERSION)))
$(info -- KernelSU version: $(KSU_VERSION))
ccflags-y += -DKSU_VERSION=$(KSU_VERSION)
else
# If there is no .git directory, use default version
$(warning "KSU_GIT_VERSION not defined! It is better to make KernelSU a git repository!")
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

ccflags-y += -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat -Wno-missing-prototypes
ccflags-y += -Wno-declaration-after-statement -Wno-unused-function

all:
	make -C $(KDIR) M=$(MDIR) modules
compdb:
	python3 $(MDIR)/.vscode/generate_compdb.py -O $(KDIR) $(MDIR)
clean:
	make -C $(KDIR) M=$(MDIR) clean

# Keep a new line here!! Because someone may append config
