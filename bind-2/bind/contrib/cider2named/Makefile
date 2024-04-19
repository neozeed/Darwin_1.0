DIST	=	\
	INSTRUCTIONS \
	named.ca \
	Makefile \
	cider2named.sh
CIDERFILE = hosts.cider


all:
	sh cider2named.sh $(CIDERFILE)

install:
	for x in xfer*.sh ; do \
		/bin/sh $$x ; \
	done

uninstall:
	@echo 'No uninstall implemented - do by hand using backup files'

dist:
	rm -f updt
	$(MAKE) updt

updt: $(DIST)
	tar cvf - $? | gzip >cider2named.tar.gz
	touch updt
