/*
 * A rewrite of the original Debian's start-stop-daemon Perl script
 * in C (faster - it is executed many times during system startup).
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * public domain.  Based conceptually on start-stop-daemon.pl, by Ian
 * Jackson <ijackson@gnu.ai.mit.edu>.  May be used and distributed
 * freely for any purpose.  Changes by Christian Schwarz
 * <schwarz@monet.m.isar.de>, to make output conform to the Debian
 * Console Message Standard, also placed in public domain.  Minor
 * changes by Klee Dienes <klee@debian.org>, also placed in the Public
 * Domain. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>

#define VERSION "version 0.4.1, 1997-01-29"

/* Take care of NLS matters.  */

#if HAVE_LOCALE_H
# include <locale.h>
#endif
#if !HAVE_SETLOCALE
# define setlocale(Category, Locale) /* empty */
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory) /* empty */
# undef textdomain
# define textdomain(Domain) /* empty */
# define _(Text) Text
#endif

static int testmode = 0;
static int quietmode = 0;
static int exitnodo = 1;
static int start = 0;
static int stop = 0;
static int signal_nr = 15;
static const char *signal_str = NULL;
static int user_id = -1;
static const char *userspec = NULL;
static const char *cmdname = NULL;
static char *execname = NULL;
static char *startas = NULL;
static const char *pidfile = NULL;
static const char *progname = "";

static struct stat exec_stat;

struct pid_list {
	struct pid_list *next;
	int pid;
};

static struct pid_list *found = NULL;
static struct pid_list *killed = NULL;

static void *xmalloc(int size);
static void push(struct pid_list **list, int pid);
static void do_help(void);
static void parse_options(int argc, char * const *argv);
static int pid_is_exec(int pid, const struct stat *esb);
static int pid_is_user(int pid, int uid);
static int pid_is_cmd(int pid, const char *name);
static void check(int pid);
static void do_pidfile(const char *name);
static void do_procfs(void);
static void do_stop(void);

#ifdef __GNUC__
static void fatal(const char *format, ...)
	__attribute__((noreturn, format(printf, 1, 2)));
static void badusage(const char *msg)
	__attribute__((noreturn));
#else
static void fatal(const char *format, ...);
static void badusage(const char *msg);
#endif

static void
fatal(const char *format, ...)
{
	va_list arglist;

	fprintf(stderr, "%s: ", progname);
	va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	va_end(arglist);
	putc('\n', stderr);
	exit(2);
}


static void *
xmalloc(int size)
{
	void *ptr;

	ptr = malloc(size);
	if (ptr)
		return ptr;
	fatal(_("malloc(%d) failed"), size);
}


static void
push(struct pid_list **list, int pid)
{
	struct pid_list *p;

	p = xmalloc(sizeof(*p));
	p->next = *list;
	p->pid = pid;
	*list = p;
}


static void
do_help(void)
{
	printf("%s" VERSION " \n\n%s",_("\
start-stop-daemon for Debian Linux - small and fast C version written by\n\
Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>, public domain.\n"),
_("Usage:\n\
  start-stop-daemon -S|--start options ... -- arguments ...\n\
  start-stop-daemon -K|--stop options ...\n\
  start-stop-daemon -H|--help\n\
  start-stop-daemon -V|--version\n\
\n\
Options (at least one of --exec|--pidfile|--user is required):\n\
  -x|--exec <executable>        program to start/check if it is running\n\
  -p|--pidfile <pid-file>       pid file to check\n\
  -u|--user <username>|<uid>    stop processes owned by this user\n\
  -n|--name <process-name>      stop processes with this name\n\
  -s|--signal <signal>          signal to send (default TERM)\n\
  -a|--startas <pathname>       program to start (default is <executable>)\n\
  -t|--test                     test mode, don't do anything\n\
  -o|--oknodo                   exit status 0 (not 1) if nothing done\n\
  -q|--quiet                    be more quiet\n\
  -v|--verbose                  be more verbose\n\
\n\
Exit status:  0 = done  1 = nothing done (=> 0 if --oknodo)  2 = trouble\n"));
}


static void
badusage(const char *msg)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", progname, msg);
	fprintf(stderr, _("Try `%s --help' for more information.\n"), progname);
	exit(2);
}

struct sigpair {
	const char *name;
	int signal;
};

const struct sigpair siglist[] = {
	{ "ABRT", SIGABRT },
	{ "ALRM", SIGALRM },
	{ "FPE", SIGFPE },
	{ "HUP", SIGHUP },
	{ "ILL", SIGILL },
	{ "INT", SIGINT },
	{ "KILL", SIGKILL },
	{ "PIPE", SIGPIPE },
	{ "QUIT", SIGQUIT },
	{ "SEGV", SIGSEGV },
	{ "TERM", SIGTERM },
	{ "USR1", SIGUSR1 },
	{ "USR2", SIGUSR2 },
	{ "CHLD", SIGCHLD },
	{ "CONT", SIGCONT },
	{ "STOP", SIGSTOP },
	{ "TSTP", SIGTSTP },
	{ "TTIN", SIGTTIN },
	{ "TTOU", SIGTTOU }
};

static int parse_signal (const char *signal_str, int *signal_nr)
{
	int i;
	for (i = 0; i < sizeof (siglist) / sizeof (siglist[0]); i++) {
		if (strcmp (signal_str, siglist[i].name) == 0) {
			*signal_nr = siglist[i].signal;
			return 0;
		}
	}
	return -1;
}

static void
parse_options(int argc, char * const *argv)
{
	static struct option longopts[] = {
		{ "help",	0, NULL, 'H'},
		{ "stop",	0, NULL, 'K'},
		{ "start",	0, NULL, 'S'},
		{ "version",	0, NULL, 'V'},
		{ "startas",	1, NULL, 'a'},
		{ "name",	1, NULL, 'n'},
		{ "oknodo",	0, NULL, 'o'},
		{ "pidfile",	1, NULL, 'p'},
		{ "quiet",	0, NULL, 'q'},
		{ "signal",	1, NULL, 's'},
		{ "test",	0, NULL, 't'},
		{ "user",	1, NULL, 'u'},
		{ "verbose",	0, NULL, 'v'},
		{ "exec",	1, NULL, 'x'},
		{ NULL,		0, NULL, 0}
	};
	int c;

	for (;;) {
		c = getopt_long(argc, argv, "HKSVa:n:op:qs:tu:vx:",
				longopts, (int *) 0);
		if (c == -1)
			break;
		switch (c) {
		case 'H':  /* --help */
			do_help();
			exit(0);
		case 'K':  /* --stop */
			stop = 1;
			break;
		case 'S':  /* --start */
			start = 1;
			break;
		case 'V':  /* --version */
			printf("start-stop-daemon " VERSION "\n");
			exit(0);
		case 'a':  /* --startas <pathname> */
			startas = optarg;
			break;
		case 'n':  /* --name <process-name> */
			cmdname = optarg;
			break;
		case 'o':  /* --oknodo */
			exitnodo = 0;
			break;
		case 'p':  /* --pidfile <pid-file> */
			pidfile = optarg;
			break;
		case 'q':  /* --quiet */
			quietmode = 1;
			break;
		case 's':  /* --signal <signal> */
			signal_str = optarg;
			break;
		case 't':  /* --test */
			testmode = 1;
			break;
		case 'u':  /* --user <username>|<uid> */
			userspec = optarg;
			break;
		case 'v':  /* --verbose */
			quietmode = -1;
			break;
		case 'x':  /* --exec <executable> */
			execname = optarg;
			break;
		default:
			badusage(NULL);  /* message printed by getopt */
		}
	}

	if (signal_str != NULL) {
		if (sscanf (signal_str, "%d", &signal_nr) != 1) {
			if (parse_signal (signal_str, &signal_nr) != 0) {
				badusage (_("--signal takes a numeric argument or name of signal (KILL, INTR, ...)"));
			}
		}	
	}

	if (start == stop)
		badusage(_("need one of --start or --stop"));

	if (!execname && !pidfile && !userspec)
		badusage(_("need at least one of --exec, --pidfile or --user"));

	if (!startas)
		startas = execname;

	if (start && !startas)
		badusage(_("--start needs --exec or --startas"));
}


static int
pid_is_exec(int pid, const struct stat *esb)
{
	struct stat sb;
	char buf[32];

	sprintf(buf, "/proc/%d/exe", pid);
	if (stat(buf, &sb) != 0)
		return 0;
	return (sb.st_dev == esb->st_dev && sb.st_ino == esb->st_ino);
}


static int
pid_is_user(int pid, int uid)
{
	struct stat sb;
	char buf[32];

	sprintf(buf, "/proc/%d", pid);
	if (stat(buf, &sb) != 0)
		return 0;
	return (sb.st_uid == uid);
}


static int
pid_is_cmd(int pid, const char *name)
{
	char buf[32];
	FILE *f;
	int c;

	sprintf(buf, "/proc/%d/stat", pid);
	f = fopen(buf, "r");
	if (!f)
		return 0;
	while ((c = getc(f)) != EOF && c != '(')
		;
	if (c != '(') {
		fclose(f);
		return 0;
	}
	/* this hopefully handles command names containing ')' */
	while ((c = getc(f)) != EOF && c == *name)
		name++;
	fclose(f);
	return (c == ')' && *name == '\0');
}


static void
check(int pid)
{
	if (execname && !pid_is_exec(pid, &exec_stat))
		return;
	if (userspec && !pid_is_user(pid, user_id))
		return;
	if (cmdname && !pid_is_cmd(pid, cmdname))
		return;
	push(&found, pid);
}


static void
do_pidfile(const char *name)
{
	FILE *f;
	int pid;

	f = fopen(name, "r");
	if (f) {
		if (fscanf(f, "%d", &pid) == 1)
			check(pid);
		fclose(f);
	}
}


static void
do_procfs(void)
{
	DIR *procdir;
	struct dirent *entry;
	int foundany, pid;

	procdir = opendir("/proc");
	if (!procdir)
		fatal(_("opendir /proc failed: %s"), strerror(errno));

	foundany = 0;
	while ((entry = readdir(procdir)) != NULL) {
		if (sscanf(entry->d_name, "%d", &pid) != 1)
			continue;
		foundany++;
		check(pid);
	}
	closedir(procdir);
	if (!foundany)
		fatal(_("nothing in /proc - not mounted?"));
}


static void
do_stop(void)
{
	char what[1024];
	struct pid_list *p;

	if (cmdname)
		strcpy(what, cmdname);
	else if (execname)
		strcpy(what, execname);
	else if (pidfile)
		/* submsg: do_stop */
		sprintf(what, _("process in pidfile `%s'"), pidfile);
	else if (userspec)
		/* submsg: do_stop */
		sprintf(what, _("process(es) owned by `%s'"), userspec);
	else
		fatal(_("internal error, please report"));

	if (!found) {
		if (quietmode <= 0)
			/* supermsg: do_stop -- can also be a command name */
			printf(_("No %s found running; none killed.\n"), what);
		exit(exitnodo);
	}
	for (p = found; p; p = p->next) {
		if (testmode)
			printf(_("Would send signal %d to %d.\n"),
			       signal_nr, p->pid);
		else if (kill(p->pid, signal_nr) == 0)
			push(&killed, p->pid);
		else
			printf(_("%s: warning: failed to kill %d: %s\n"),
			       progname, p->pid, strerror(errno));
	}
	if (quietmode < 0 && killed) {
		/* FIXME: should first sprintf pids, and then printf 1 only string */
		printf(_("Stopped %s (pid"), what);
		for (p = killed; p; p = p->next)
			printf(" %d", p->pid);
		printf(").\n");
	}
}


int
main(int argc, char **argv)
{
	progname = argv[0];

	setlocale (LC_ALL, "");
	bindtextdomain ("dpkgutil", LOCALEDIR);
	textdomain ("dpkgutil");

	parse_options(argc, argv);
	argc -= optind;
	argv += optind;

	if (execname && stat(execname, &exec_stat))
		fatal(_("stat %s: %s"), execname, strerror(errno));

	if (userspec && sscanf(userspec, "%d", &user_id) != 1) {
		struct passwd *pw;

		pw = getpwnam(userspec);
		if (!pw)
			fatal(_("user `%s' not found\n"), userspec);

		user_id = pw->pw_uid;
	}

	if (pidfile)
		do_pidfile(pidfile);
	else
		do_procfs();

	if (stop) {
		do_stop();
		exit(0);
	}

	if (found) {
		if (quietmode <= 0)
			printf(_("%s already running.\n"), execname);
		exit(exitnodo);
	}
	if (testmode) {
		/* should sprintf args first, and then output only 1 string */
		printf(_("Would start %s "), startas);
		while (argc-- > 0)
			printf("%s ", *argv++);
		printf(".\n");
		exit(0);
	}
	if (quietmode < 0)
		printf(_("Starting %s...\n"), startas);
	*--argv = startas;
	execv(startas, argv);
	fatal(_("Unable to start %s: %s"), startas, strerror(errno));
}
