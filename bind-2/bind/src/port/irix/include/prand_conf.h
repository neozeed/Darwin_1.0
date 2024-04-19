#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/bin/ps -ef 2>&1",
	"/usr/etc/arp -a 2>&1",
	"/usr/etc/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/dig com. soa +ti=1 2>&1",
	"/usr/bsd/uptime  2>&1",
	"/usr/bin/printenv  2>&1",
	"/usr/etc/netstat -s 2>&1",
	"/usr/bin/dig . soa +ti=1 2>&1",
	"/usr/bsd/w  2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/var/tmp",
	".",
	"/",
	"/var/spool",
	"/var/adm",
	"/dev",
	"/var/mail",
	NULL
};

const char *files[] = {
	NULL
};

#endif /* _PRAND_CMD_H_ */
