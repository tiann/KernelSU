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

# .git is a text file while the module is imported by 'git submodule add'.
ifeq ($(shell test -e $(srctree)/$(src)/../.git; echo $$?),0)
$(shell cd $(srctree)/$(src); /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin [ -f ../.git/shallow ] && git fetch --unshallow)
KSU_GIT_VERSION := $(shell cd $(srctree)/$(src); /usr/bin/env PATH="$$PATH":/usr/bin:/usr/local/bin git rev-list --count HEAD)
# ksu_version: major * 10000 + git version + 200 for historical reasons
$(eval KSU_VERSION=$(shell expr 10000 + $(KSU_GIT_VERSION) + 200))
$(info -- KernelSU version: $(KSU_VERSION))
ccflags-y += -DKSU_VERSION=$(KSU_VERSION)
else # If there is no .git file, the default version will be passed.
$(warning "KSU_GIT_VERSION not defined! It is better to make KernelSU a git submodule!")
ccflags-y += -DKSU_VERSION=16
endif

ifeq ($(shell grep -q " current_sid(void)" $(srctree)/security/selinux/include/objsec.h; echo $$?),0)
ccflags-y += -DKSU_COMPAT_HAS_CURRENT_SID
endif

ifeq ($(shell grep -q "struct selinux_state " $(srctree)/security/selinux/include/security.h; echo $$?),0)
ccflags-y += -DKSU_COMPAT_HAS_SELINUX_STATE
endif

ifndef KSU_EXPECTED_SIZE
KSU_EXPECTED_SIZE := 0x033b
endif

ifndef KSU_EXPECTED_HASH
KSU_EXPECTED_HASH := c371061b19d8c7d7d6133c6a9bafe198fa944e50c1b31c9d8daa8d7f1fc2d2d6
endif

ifndef KSU_NEXT_EXPECTED_SIZE
KSU_NEXT_EXPECTED_SIZE := 0x27b
endif

ifndef KSU_NEXT_EXPECTED_HASH
KSU_NEXT_EXPECTED_HASH := d50f782a7d12deaa1ae967959a5b69cd88587874387e92b8b1517df232e5a061
endif

ifndef KSU_LEGACY_EXPECTED_SIZE
KSU_LEGACY_EXPECTED_SIZE := 0x363
endif

ifndef KSU_LEGACY_EXPECTED_HASH
KSU_LEGACY_EXPECTED_HASH := 4359c171f32543394cbc23ef908c4bb94cad7c8087002ba164c8230948c21549
endif

ifndef KSU_N3X7G3N_EXPECTED_SIZE
KSU_N3X7G3N_EXPECTED_SIZE := 0x29c
endif

ifndef KSU_N3X7G3N_EXPECTED_HASH
KSU_N3X7G3N_EXPECTED_HASH := bfddf83a559355b053187177775c39c639d2d2695163baa77253746dbf18098d
endif

ifdef KSU_MANAGER_PACKAGE
ccflags-y += -DKSU_MANAGER_PACKAGE=\"$(KSU_MANAGER_PACKAGE)\"
$(info -- KernelSU Manager package name: $(KSU_MANAGER_PACKAGE))
endif

$(info -- KernelSU Manager signature size: $(KSU_EXPECTED_SIZE))
$(info -- KernelSU Manager signature hash: $(KSU_EXPECTED_HASH))
$(info -- KernelSU next Manager signature size: $(KSU_NEXT_EXPECTED_SIZE))
$(info -- KernelSU next Manager signature hash: $(KSU_NEXT_EXPECTED_HASH))
$(info -- KernelSU legacy Manager signature size: $(KSU_LEGACY_EXPECTED_SIZE))
$(info -- KernelSU legacy Manager signature hash: $(KSU_LEGACY_EXPECTED_HASH))
$(info -- KernelSU n3x7g3n Manager signature size: $(KSU_N3X7G3N_EXPECTED_SIZE))
$(info -- KernelSU n3x7g3n Manager signature hash: $(KSU_N3X7G3N_EXPECTED_HASH))

ccflags-y += -DEXPECTED_SIZE=$(KSU_EXPECTED_SIZE)
ccflags-y += -DEXPECTED_HASH=\"$(KSU_EXPECTED_HASH)\"
ccflags-y += -DEXPECTED_NEXT_SIZE=$(KSU_NEXT_EXPECTED_SIZE)
ccflags-y += -DEXPECTED_NEXT_HASH=\"$(KSU_NEXT_EXPECTED_HASH)\"
ccflags-y += -DEXPECTED_LEGACY_SIZE=$(KSU_LEGACY_EXPECTED_SIZE)
ccflags-y += -DEXPECTED_LEGACY_HASH=\"$(KSU_LEGACY_EXPECTED_HASH)\"
ccflags-y += -DEXPECTED_N3X7G3N_SIZE=$(KSU_N3X7G3N_EXPECTED_SIZE)
ccflags-y += -DEXPECTED_N3X7G3N_HASH=\"$(KSU_N3X7G3N_EXPECTED_HASH)\"

#ifeq ($(shell grep -q "int path_umount" $(srctree)/fs/namespace.c; echo $$?),0)
#ccflags-y += -DKSU_UMOUNT
#else
#$(info -- Did you know you can backport path_umount to fs/namespace.c from 5.9?)
#$(info -- Read: https://kernelsu.org/guide/how-to-integrate-for-non-gki.html#how-to-backport-path-umount)
#endif

ccflags-y += -DKSU_UMOUNT

ifneq ($(shell grep -Eq "^static int can_umount" $(srctree)/fs/namespace.c; echo $$?),0)
$(info -- KSU_SUSFS: adding function 'static int can_umount(const struct path *path, int flags);' to $(srctree)/fs/namespace.c)
CAN_UMOUNT = static int can_umount(const struct path *path, int flags)\n\
{\n\t\
	struct mount *mnt = real_mount(path->mnt);\n\t\
	if (flags & ~(MNT_FORCE | MNT_DETACH | MNT_EXPIRE | UMOUNT_NOFOLLOW))\n\t\t\
		return -EINVAL;\n\t\
	if (!may_mount())\n\t\t\
		return -EPERM;\n\t\
	if (path->dentry != path->mnt->mnt_root)\n\t\t\
		return -EINVAL;\n\t\
	if (!check_mnt(mnt))\n\t\t\
		return -EINVAL;\n\t\
	if (mnt->mnt.mnt_flags & MNT_LOCKED)\n\t\t\
		return -EINVAL;\n\t\
	if (flags & MNT_FORCE && !capable(CAP_SYS_ADMIN))\n\t\t\
		return -EPERM;\n\t\
	return 0;\n\
}\n
$(shell sed -i '/^static bool is_mnt_ns_file/i $(CAN_UMOUNT)' $(srctree)/fs/namespace.c;)
endif

ifneq ($(shell grep -Eq "^int path_umount" $(srctree)/fs/namespace.c; echo $$?),0)
$(info -- KSU_SUSFS: adding function 'int path_umount(struct path *path, int flags);' to $(srctree)/fs/namespace.c)
PATH_UMOUNT = int path_umount(struct path *path, int flags)\n\
{\n\t\
	struct mount *mnt = real_mount(path->mnt);\n\t\
	int ret;\n\t\
	ret = can_umount(path, flags);\n\t\
	if (!ret)\n\t\t\
		ret = do_umount(mnt, flags);\n\t\
	dput(path->dentry);\n\t\
	mntput_no_expire(mnt);\n\t\
	return ret;\n\
}\n
$(shell sed -i '/^static bool is_mnt_ns_file/i $(PATH_UMOUNT)' $(srctree)/fs/namespace.c;)
endif

ifneq ($(shell grep -Eq "^int path_umount" $(srctree)/fs/internal.h; echo $$?),0)
$(shell sed -i '/^extern void __init mnt_init/a int path_umount(struct path *path, int flags);' $(srctree)/fs/internal.h;)
$(info -- KSU_SUSFS: adding 'int path_umount(struct path *path, int flags);' to $(srctree)/fs/internal.h)
endif

ccflags-y += -Wno-implicit-function-declaration -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat
ccflags-y += -Wno-declaration-after-statement -Wno-unused-function

ifeq ($(shell test -e $(srctree)/fs/susfs.c; echo $$?),0)
$(eval SUSFS_VERSION=$(shell cat $(srctree)/include/linux/susfs.h | grep -E '^#define SUSFS_VERSION' | cut -d' ' -f3 | sed 's/"//g'))
$(info -- SUSFS_VERSION: $(SUSFS_VERSION))
ifdef KSU_SUSFS
ccflags-y += -DKSU_SUSFS
endif
ifdef KSU_SUSFS_SUS_PATH
ccflags-y += -DKSU_SUSFS_SUS_PATH
endif
ifdef KSU_SUSFS_SUS_MOUNT
ccflags-y += -DKSU_SUSFS_SUS_MOUNT
endif
ifdef KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT
ccflags-y += -DKSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT
endif
ifdef KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT
ccflags-y += -DKSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT
endif
ifdef KSU_SUSFS_SUS_KSTAT
ccflags-y += -DKSU_SUSFS_SUS_KSTAT
endif
ifdef KSU_SUSFS_SUS_OVERLAYFS
ccflags-y += -DKSU_SUSFS_SUS_OVERLAYFS
endif
ifdef KSU_SUSFS_TRY_UMOUNT
ccflags-y += -DKSU_SUSFS_TRY_UMOUNT
endif
ifdef KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT
ccflags-y += -DKSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT
endif
ifdef KSU_SUSFS_SPOOF_UNAME
ccflags-y += -DKSU_SUSFS_SPOOF_UNAME
endif
ifdef KSU_SUSFS_ENABLE_LOG
ccflags-y += -DKSU_SUSFS_ENABLE_LOG
endif
ifdef KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
ccflags-y += -DKSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
endif
ifdef KSU_SUSFS_SPOOF_BOOTCONFIG
ccflags-y += -DKSU_SUSFS_SPOOF_BOOTCONFIG
endif
ifdef KSU_SUSFS_OPEN_REDIRECT
ccflags-y += -DKSU_SUSFS_OPEN_REDIRECT
endif
ifdef KSU_SUSFS_SUS_SU
ccflags-y += -DKSU_SUSFS_SUS_SU
endif
else
$(info -- You have not integrate susfs in your kernel.)
$(info -- Read: https://gitlab.com/simonpunk/susfs4ksu)
endif

# Keep a new line here!! Because someone may append config
