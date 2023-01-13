obj-y += selinux.o
obj-y += sepolicy.o
obj-y += rules.o


ccflags-y += -Wno-implicit-function-declaration -Wno-strict-prototypes -Wno-int-conversion
ccflags-y += -Wno-macro-redefined -Wno-declaration-after-statement -Wno-unused-function
ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include
ccflags-y += -I$(objtree)/security/selinux