TARGETS = amalgamate textfilter
OBJS = $(addsuffix .o,$(TARGETS))

all: $(TARGETS)
	CC="$(XCC)" CFLAGS="$(XCFLAGS)" sh ./mkoffsets.sh ../pico

clean:
	$(RM) $(TARGETS) $(OBJS)

.PHONY: clean all
