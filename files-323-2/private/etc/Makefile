Destination = $(DSTROOT)/private/etc

# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Subdirectories with their own makefiles
SubDirs = netinfo

install::
	$(_v) for subdir in $(SubDirs); do						\
		(cd "$$subdir" && $(MAKE) $@ Destination="$(Destination)/$$subdir");	\
	      done

install::
	@echo "Installing $(Destination)"
	$(_v) $(INSTALL_DIRECTORY) "$(Destination)"
	$(_v) $(INSTALL_FILE) -c -m 644 afpovertcp.cfg	 "$(Destination)/afpovertcp.cfg"
	$(_v) $(INSTALL_FILE) -c -m 644 bootstrap.conf	 "$(Destination)/bootstrap.conf"
	$(_v) $(INSTALL_FILE) -c -m 644 changelist	 "$(Destination)/changelist"
	$(_v) $(INSTALL_FILE) -c -m 644 crontab		 "$(Destination)/crontab"
	$(_v) $(INSTALL_FILE) -c -m 644 crontab.local	 "$(Destination)/crontab.local"
	$(_v) $(INSTALL_FILE) -c -m 644 daily		 "$(Destination)/daily"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/find.codes"
	$(_v) $(INSTALL_FILE) -c -m 644 ftpusers	 "$(Destination)/ftpusers"
	$(_v) $(INSTALL_FILE) -c	fstab.hd	 "$(Destination)/fstab.hd"
	$(_v) $(INSTALL_FILE) -c	fstab.rd	 "$(Destination)/fstab.rd"
	$(_v) $(INSTALL_FILE) -c	fstab.sd	 "$(Destination)/fstab.sd"
	$(_v) $(INSTALL_FILE) -c -m 644 gettytab	 "$(Destination)/gettytab"
	$(_v) $(INSTALL_FILE) -c -m 644 group		 "$(Destination)/group"
	$(_v) $(INSTALL_FILE) -c -m 644 hosts		 "$(Destination)/hosts"
	$(_v) $(INSTALL_FILE) -c -m 644 hosts.lpd	 "$(Destination)/hosts.lpd"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/hosts.equiv"
	$(_v) $(INSTALL_FILE) -c -m 644 iftab		 "$(Destination)/iftab"
	$(_v) $(INSTALL_FILE) -c -m 644 inetd.conf	 "$(Destination)/inetd.conf"
	$(_v) $(INSTALL_FILE) -c -m 644 kern_loader.conf "$(Destination)/kern_loader.conf"
	$(_v) $(INSTALL_FILE) -c -m 600 master.passwd	 "$(Destination)/master.passwd"
	$(_v) $(INSTALL_FILE) -c -m 644 monthly		 "$(Destination)/monthly"
	$(_v) $(INSTALL_FILE) -c -m 644 motd		 "$(Destination)/motd"
	$(_v) $(INSTALL_FILE) -c -m 644 netgroup	 "$(Destination)/netgroup"
	$(_v) $(INSTALL_FILE) -c -m 644 networks	 "$(Destination)/networks"
	$(_v) $(INSTALL_FILE) -c -m 644 passwd		 "$(Destination)/passwd"
	$(_v) $(INSTALL_FILE) -c -m 644 phones		 "$(Destination)/phones"
	$(_v) $(INSTALL_FILE) -c -m 644 printcap	 "$(Destination)/printcap"
	$(_v) $(INSTALL_FILE) -c -m 644 protocols	 "$(Destination)/protocols"
	$(_v) $(INSTALL_FILE) -c -m 644 remote		 "$(Destination)/remote"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/rmtab"
	$(_v) $(INSTALL_FILE) -c -m 644 rpc		 "$(Destination)/rpc"
	$(_v) $(INSTALL_FILE) -c -m 644 services	 "$(Destination)/services"
	$(_v) $(INSTALL_FILE) -c -m 644 shells		 "$(Destination)/shells"
	$(_v) $(INSTALL_FILE) -c -m 644 syslog.conf	 "$(Destination)/syslog.conf"
ifeq "$(RC_RELEASE)" "Darwin"
	$(_v) cat ttys | sed -e 's/^console/##console/' -e 's/^#console/console/' > "$(Destination)/ttys"
	$(_v) chmod 644 "$(Destination)/ttys" ; chown root:wheel "$(Destination)/ttys"
else
	$(_v) $(INSTALL_FILE) -c -m 644 ttys		 "$(Destination)/ttys"
endif
	$(_v) $(INSTALL_FILE) -c	ttys.installer	 "$(Destination)/ttys.installer"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/utmp"
	$(_v) $(INSTALL_FILE) -c -m 644 weekly		 "$(Destination)/weekly"
	$(_v) $(INSTALL_FILE) -c -m 644 /dev/null	 "$(Destination)/xtab"
