TARGETS = amalgamate textfilter
HOSTCC ?= cc

all:
	CC="$(XCC)" CFLAGS="$(XCFLAGS)" sh ./mkoffsets.sh ../pico

$(TARGETS): $(addsuffix .c,$(TARGETS))
	$(HOSTCC) -o $@ -O $@.c

clean:
	$(RM) $(TARGETS) $(OBJS)

.PHONY: clean all
