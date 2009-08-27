platform/common/menu.o : revision.h

revision.h: FORCE
	@echo "#define REVISION \"`svn info -r HEAD | grep Revision | cut -c 11-`\"" > /tmp/r.tmp
	@diff -q $@ /tmp/r.tmp > /dev/null 2>&1 || mv -f /tmp/r.tmp $@

FORCE:

