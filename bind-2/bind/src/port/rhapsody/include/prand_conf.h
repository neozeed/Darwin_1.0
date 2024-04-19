#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/usr/sbin/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/uptime  2>&1",
	"/usr/bin/printenv  2>&1",
	"/usr/sbin/netstat -s 2>&1",
	"/usr/bin/vm_stat  2>&1",
	"/usr/bin/w  2>&1",
	NULL
};

const char *dirs[] = {
	"/var/tmp",
	".",
	"/",
	"/var/spool",
	"/var/mail",
	NULL
};

const char *files[] = {
	"/var/log/wtmp",
	NULL
};

#endif /* _PRAND_CMD_H_ */
