#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/usr/sbin/arp -an 2>&1",
	"/usr/sbin/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/dig com. soa +ti=1 +retry=0 2>&1",
	"/usr/ucb/uptime  2>&1",
	"/usr/sbin/netstat -an 2>&1",
	"/usr/sbin/iostat  2>&1",
	"/usr/sbin/vmstat  2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/var/tmp",
	".",
	"/",
	"/var/spool",
	"/dev",
	"/var/mail",
	"/usr/home",
	NULL
};

const char *files[] = {
	"/var/log/messages",
	"/var/log/wtmp",
	"/var/log/lastlog",
	NULL
};

#endif /* _PRAND_CMD_H_ */
