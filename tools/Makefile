TARGETS = amalgamate textfilter
OBJS = $(addsuffix .o,$(TARGETS))

all: $(TARGETS)
	CC="$(XCC)" CFLAGS="$(XCFLAGS)" ./mkoffsets.sh ../pico

clean:
	$(RM) $(TARGETS) $(OBJS)

.PHONY: clean all
