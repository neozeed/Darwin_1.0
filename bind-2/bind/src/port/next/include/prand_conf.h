#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/usr/etc/arp -a 2>&1",
	"/usr/ucb/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/local/bin/dig com. soa +ti=1 2>&1",
	"/usr/ucb/uptime  2>&1",
	"/usr/ucb/printenv  2>&1",
	"/usr/ucb/netstat -s 2>&1",
	"/usr/local/bin/dig . soa +ti=1 2>&1",
	"/usr/bin/iostat  2>&1",
	"/usr/bin/vm_stat  2>&1",
	"/usr/ucb/w  2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/usr/tmp",
	".",
	"/",
	"/usr/spool",
	"/dev",
	NULL
};

const char *files[] = {
	"/usr/adm/messages",
	"/usr/adm/wtmp",
	"/usr/adm/lastlog",
	NULL
};

#endif /* _PRAND_CMD_H_ */
