#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/bin/ps -ef 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/uptime  2>&1",
	"/usr/bin/printenv  2>&1",
	"/usr/bin/netstat -s 2>&1",
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
	"/var/spool/mail",
	"/home",
	NULL
};

const char *files[] = {
	"/var/adm/wtmp",
	NULL
};

#endif /* _PRAND_CMD_H_ */
