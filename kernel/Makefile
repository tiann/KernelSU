KDIR := $(KDIR)
MDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

$(info -- KDIR: $(KDIR))
$(info -- MDIR: $(MDIR))

.PHONY: all compdb clean

all: check_symbol
	make -C $(KDIR) M=$(MDIR) modules
	tools/check_symbol kernelsu.ko $(KDIR)/vmlinux
compdb:
	python3 $(MDIR)/.vscode/generate_compdb.py -O $(KDIR) $(MDIR)
clean:
	make -C $(KDIR) M=$(MDIR) clean
check_symbol: tools/check_symbol.c
	clang tools/check_symbol.c -o tools/check_symbol

# Keep a new line here!! Because someone may append config
