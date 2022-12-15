obj-y += ksu.o
obj-y += allowlist.o
obj-y += apk_sign.o
obj-y += module_api.o
obj-y += sucompat.o

obj-y += selinux/

EXPECTED_SIZE := 0x033b
EXPECTED_HASH := 0xb0b91415
ccflags-y += -DEXPECTED_SIZE=$(EXPECTED_SIZE)
ccflags-y += -DEXPECTED_HASH=$(EXPECTED_HASH)
ccflags-y += -Wno-implicit-function-declaration -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat
