#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/usr/bin/ps -ef 2>&1",
	"/usr/sbin/arp -an 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/usr/bin/df  2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/usr/bin/vmstat  2>&1",
	"/usr/bin/w  2>&1",
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
	"/home",
	NULL
};

const char *files[] = {
	"/var/adm/wtmp",
	NULL
};

#endif /* _PRAND_CMD_H_ */
