#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/bin/ps -ef 2>&1",
	"/etc/arp -an 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/usr/bin/bdf 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/usr/bin/vmstat 2>&1",
	"/usr/bin/w 2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/usr/tmp",
	".",
	"/",
	"/usr/spool",
	"/usr/adm",
	"/dev",
	"/usr/mail",
	NULL
};

const char *files[] = {
	"/usr/adm/syslog",
	"/etc/wtmp",
	NULL
};

#endif /* _PRAND_CMD_H_ */
