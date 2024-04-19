#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/usr/ucb/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/dig com. soa +ti=1 +retry=0 2>&1",
	"/usr/ucb/uptime  2>&1",
	"/usr/ucb/netstat -an 2>&1",
	"/bin/iostat  2>&1",
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
	"/var/adm/messages",
	"/var/adm/wtmp",
	"/var/adm/lastlog",
	NULL
};

#endif /* _PRAND_CMD_H_ */
