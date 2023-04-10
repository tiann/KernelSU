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
# .git is a text file while the module is imported by 'git submodule add'.
ifeq ($(shell test -e $(srctree)/$(src)/../.git; echo $$?),0)
KSU_GIT_VERSION := $(shell cd $(srctree)/$(src); /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin git rev-list --count HEAD)
ccflags-y += -DKSU_GIT_VERSION=$(KSU_GIT_VERSION)
endif

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
