#ifndef _PRAND_CMD_H_
#define _PRAND_CMD_H_

const char *cmds[] = {
	"/sbin/ps -efl 2>&1",
	"/usr/sbin/arp -an 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/sbin/df 2>&1",
	"/usr/sbin/dig com. soa +ti=1 +retry=0 2>&1",
	"/sbin/who -b 2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/var/tmp",
	".",
	"/",
	"/var/spool",
	"/dev",
	NULL
};

const char *files[] = {
	"/var/adm/log/osmlog",
	"/var/adm/syslog",
	"/var/adm/wtmpx",
	NULL
};

#endif /* _PRAND_CMD_H_ */
