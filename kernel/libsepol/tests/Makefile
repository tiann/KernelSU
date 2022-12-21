ENV ?= env
M4 ?= m4
MKDIR ?= mkdir
EXE ?= libsepol-tests

CFLAGS += -g3 -gdwarf-2 -O0 -Wall -W -Wundef -Wmissing-noreturn -Wmissing-format-attribute -Wno-unused-parameter -Werror

# Statically link libsepol on the assumption that we are going to
# be testing internal functions.
LIBSEPOL := ../src/libsepol.a

# In order to load source policies we need to link in the checkpolicy/checkmodule parser and util code.
# This is less than ideal, but it makes the tests easier to maintain by allowing source policies
# to be loaded directly.
CHECKPOLICY := ../../checkpolicy/
override CPPFLAGS += -I../include/ -I$(CHECKPOLICY)

# test program object files
objs := $(patsubst %.c,%.o,$(sort $(wildcard *.c)))
parserobjs := $(CHECKPOLICY)queue.o $(CHECKPOLICY)y.tab.o \
	$(CHECKPOLICY)parse_util.o $(CHECKPOLICY)lex.yy.o \
	$(CHECKPOLICY)policy_define.o $(CHECKPOLICY)module_compiler.o

# test policy pieces
m4support := $(wildcard policies/support/*.spt)
testsuites := $(wildcard policies/test-*)
policysrc := $(foreach path,$(testsuites),$(wildcard $(path)/*.conf))
stdpol := $(addsuffix .std,$(policysrc))
mlspol := $(addsuffix .mls,$(policysrc))
policies := $(stdpol) $(mlspol)

all: $(EXE) $(policies)
policies: $(policies)

$(EXE): $(objs) $(parserobjs) $(LIBSEPOL)
	$(CC) $(LDFLAGS) $(objs) $(parserobjs) -lcunit $(LIBSEPOL) -o $@

%.conf.std: $(m4support) %.conf
	$(M4) $(M4PARAMS) $^ > $@

%.conf.mls: $(m4support) %.conf
	$(M4) $(M4PARAMS) -D enable_mls $^ > $@

clean: 
	rm -f $(objs) $(EXE)
	rm -f $(policies)
	rm -f policies/test-downgrade/policy.hi policies/test-downgrade/policy.lo

# mkdir is run in a clean environment created by env -i to avoid failing under ASan with:
#
#   ASan runtime does not come first in initial library list;
#   you should either link runtime to your application or manually preload it with LD_PRELOAD
#
# when the source code is built with ASan
test: $(EXE) $(policies)
	$(ENV) -i $(MKDIR) -p policies/test-downgrade
	../../checkpolicy/checkpolicy -M policies/test-cond/refpolicy-base.conf -o policies/test-downgrade/policy.hi	
	./$(EXE)

.PHONY: all policies clean test
