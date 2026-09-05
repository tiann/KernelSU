# Builds ksu_api_test.ko against an already built kernelsu.ko.
#   make -C kernel tests            (uses KDIR from the environment, e.g. ddk)
#   make -C kernel/tests KDIR=... KSU_SYMVERS=path/to/Module.symvers
# The module is linked in this directory (the only layout every KMI supports)
# and copied to KSU_TEST_OUT when set, so builds for several KMIs do not get
# mixed up: a module built for another KMI crashes the loader on insmod.
KDIR := $(KDIR)
TDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KSU_SYMVERS ?= $(TDIR)/../Module.symvers
KSU_TEST_OUT ?=

ifneq ($(wildcard $(KSU_SYMVERS)),)
EXTRA_SYMBOLS := KBUILD_EXTRA_SYMBOLS=$(KSU_SYMVERS)
else
$(warning -- $(KSU_SYMVERS) not found; ksu_get_api will have no symbol version)
EXTRA_SYMBOLS :=
endif

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(TDIR) $(EXTRA_SYMBOLS) modules
ifneq ($(KSU_TEST_OUT),)
	mkdir -p $(KSU_TEST_OUT) && cp $(TDIR)/ksu_api_test.ko $(KSU_TEST_OUT)/ksu_api_test.ko
endif

clean:
	$(MAKE) -C $(KDIR) M=$(TDIR) clean
