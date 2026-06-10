KDIR := $(KDIR)
MDIR := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

$(info -- KDIR: $(KDIR))
$(info -- MDIR: $(MDIR))

.PHONY: all release tools compdb clean format check-format

all: check_symbol
	make -C $(KDIR) M=$(MDIR) modules -j$(shell nproc)
	./check_symbol kernelsu.ko $(KDIR)/vmlinux

release: all tools
	./tools/localize_symbols.sh kernelsu.ko kernelsu.ko

tools: check_symbol gen_keep_globals trim_local_symnames

compdb:
	python3 $(MDIR)/.vscode/generate_compdb.py -O $(KDIR) $(MDIR)
clean:
	make -C $(KDIR) M=$(MDIR) clean
	rm -f check_symbol gen_keep_globals trim_local_symnames
check_symbol: tools/check_symbol.c
	$(CC) tools/check_symbol.c -o check_symbol
gen_keep_globals: tools/gen_keep_globals.c
	$(CC) -O2 -Wall -Wextra -std=c11 tools/gen_keep_globals.c -o gen_keep_globals
trim_local_symnames: tools/trim_local_symnames.c
	$(CC) -O2 -Wall -Wextra -std=c11 tools/trim_local_symnames.c -o trim_local_symnames
format:
	find . \( -name "*.c" -o -name "*.h" \) -print0 | xargs -0 clang-format -i
check-format:
	find . \( -name "*.c" -o -name "*.h" \) -print0 | xargs -0 clang-format --dry-run --Werror

# Keep a new line here!! Because someone may append config
