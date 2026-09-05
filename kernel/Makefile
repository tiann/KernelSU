KDIR := $(KDIR)
MDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
ODIR := $(MDIR)
KMI := $(notdir $(patsubst %/,%,$(KDIR)))

ifeq ($(KMI),android17-6.18)
KBUILD_ARGS := M=$(MDIR) MO=$(ODIR)
else ifeq ($(KMI),android16-6.12)
# 6.12 does not support KBUILD_EXTMOD_OUTPUT/MO. Build in the source
# directory and copy the final module to ODIR after linking.
KBUILD_ARGS := M=$(MDIR)
else
KBUILD_ARGS := M=$(ODIR) src=$(MDIR)
endif

ifeq ($(KMI),android16-6.12)
KSU_MODULE_PATH := $(MDIR)/kernelsu.ko
KSU_COPY_MODULE := mkdir -p $(ODIR); cp $(MDIR)/kernelsu.ko $(ODIR)/kernelsu.ko; rm -f $(MDIR)/modules.order
else
KSU_MODULE_PATH := $(ODIR)/kernelsu.ko
KSU_COPY_MODULE :=
endif

$(info -- KDIR: $(KDIR))
$(info -- MDIR: $(MDIR))
$(info -- OUTPUT DIR: $(ODIR))
$(info -- KMI: $(KMI))
$(info -- KBUILD_ARGS: $(KBUILD_ARGS))

.PHONY: all compdb clean format check-format

all: check_symbol
	$(MAKE) -C $(KDIR) $(KBUILD_ARGS) modules compile_commands.json -j$(shell nproc)
	$(KSU_COPY_MODULE)
	./check_symbol $(KSU_MODULE_PATH) $(KDIR)/vmlinux

compdb:
	python3 $(MDIR)/.vscode/generate_compdb.py -O $(ODIR) $(MDIR)
clean:
	$(MAKE) -C $(KDIR) $(KBUILD_ARGS) clean
	rm check_symbol
check_symbol: tools/check_symbol.c
	$(CC) tools/check_symbol.c -o check_symbol
format:
	find . \( -name "*.c" -o -name "*.h" \) -print0 | xargs -0 clang-format -i
check-format:
	find . \( -name "*.c" -o -name "*.h" \) -print0 | xargs -0 clang-format --dry-run --Werror

# Keep a new line here!! Because someone may append config
