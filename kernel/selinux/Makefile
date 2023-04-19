obj-y += selinux.o
obj-y += sepolicy.o
obj-y += rules.o

ifeq ($(shell [ $(VERSION) -lt 5 -o \( $(VERSION) -eq 5 -a $(PATCHLEVEL) -lt 10 \) ]; echo $$?), 0)
  ifeq ($(shell grep -q " current_sid(void)" $(srctree)/security/selinux/include/objsec.h; echo $$?),0)
  ccflags-y += -DKSU_COMPAT_HAS_CURRENT_SID
  endif
  ifeq ($(shell grep -q "struct selinux_state " $(srctree)/security/selinux/include/security.h; echo $$?),0)
  ccflags-y += -DKSU_COMPAT_USE_SELINUX_STATE
  endif
endif

ccflags-y += -Wno-implicit-function-declaration -Wno-strict-prototypes -Wno-int-conversion
ccflags-y += -Wno-macro-redefined -Wno-declaration-after-statement -Wno-unused-function
ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include
ccflags-y += -I$(objtree)/security/selinux