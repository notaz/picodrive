platform/common/menu.o : revision.h

revision.h: FORCE
ifndef NOREVISION
	@echo "#define REVISION \"`svn info -r HEAD | grep Revision | cut -c 11-`\"" > /tmp/r.tmp
else
	@echo "#define REVISION \"0\"" > /tmp/r.tmp
endif
	@diff -q $@ /tmp/r.tmp > /dev/null 2>&1 || mv -f /tmp/r.tmp $@

FORCE:

