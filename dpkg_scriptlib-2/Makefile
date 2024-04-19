.PHONY: clean install installhdrs installsrc installsrc-check

d      = $(DSTROOT)
perl   = $(DSTROOT)$(shell echo 'require(Config); print "$$Config::Config{privlib}"' | perl)/Dpkg
python = $(DSTROOT)/usr/lib/python1.5/site-packages/dpkg

clean:
	find . -name \*~ -print0 | xargs -0 $(RM)

install:

#	install -d $(python)
#	install -d /usr/doc/dpkg-python

#	install -c debian/changelog /usr/doc/dpkg-python/changelog.Debian
#	gzip -9vf /usr/doc/dpkg-python/*
#	install -c debian/copyright /usr/doc/dpkg-python/copyright

#	cp python/dpkg/*.py $(python)

	install -d $(perl)
#	install -d /usr/doc/dpkg-perl

#	install -c debian/changelog /usr/doc/dpkg-perl/changelog.Debian
#	gzip -9vf /usr/doc/dpkg-perl/*
#	install -c debian/copyright /usr/doc/dpkg-perl/copyright

	cp -rp perl5/Dpkg/* $(perl)
	find $(perl) -type f \! -name \*.pm -print0 | xargs -0 $(RM)

	chown -R root.root $(d) || chown -R root.wheel $(d)
	find $(d) -type f -print0 | xargs -0 chmod 644
	find $(d) -type d -print0 | xargs -0 chmod 755

installhdrs:

installsrc:
	gnutar --exclude=CVS -cf - . | gnutar -C $(SRCROOT) -xf -

installsrc-check:
	[ -z `find . -name \*~` ] || \
		(echo 'Emacs-style backup files present; not copying.' && exit 1)
