/*
 * etrn.c -- ETRN protocol methods
 *
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#ifdef ETRN_ENABLE
#include  <stdio.h>
#include  <stdlib.h>
#include  <assert.h>
#include  <netdb.h>
#include  <errno.h>
#include  <unistd.h>
#include  "i18n.h"
#include  "fetchmail.h"
#include  "smtp.h"
#include  "socket.h"

static int etrn_ok (int sock, char *argbuf)
/* parse command response */
{
    int ok;

    ok = SMTP_ok(sock);
    if (ok == SM_UNRECOVERABLE)
	return(PS_PROTOCOL);
    else
	return(ok);
}

static int etrn_getrange(int sock, struct query *ctl, const char *id, 
			 int *countp, int *newp, int *bytes)
/* send ETRN and interpret the response */
{
    int ok, opts;
    char buf [MSGBUFSIZE+1];
    struct idlist *qnp;		/* pointer to Q names */

    if ((ok = SMTP_ehlo(sock, fetchmailhost, &opts)))
    {
	report(stderr, _("%s's SMTP listener does not support ESMTP\n"),
	      ctl->server.pollname);
	return(ok);
    }
    else if (!(opts & ESMTP_ETRN))
    {
	report(stderr, _("%s's SMTP listener does not support ETRN\n"),
	      ctl->server.pollname);
	return(PS_PROTOCOL);
    }

    /* make sure we don't enter the fetch loop */
    *bytes = *countp = *newp = -1;

    /*
     * By default, the hostlist has a single entry, the fetchmail host's
     * canonical DNS name.
     */
    for (qnp = ctl->smtphunt; qnp; qnp = qnp->next)
    {
	/* ship the actual poll and get the response */
	gen_send(sock, "ETRN %s", qnp->id);
	if ((ok = gen_recv(sock, buf, sizeof(buf))))
	    return(ok);

	/* this switch includes all response codes described in RFC1985 */
	switch(atoi(buf))
	{
	case 250:	/* OK, queuing for node <x> started */
	    if (outlevel >= O_SILENT)
		report(stdout, _("Queuing for %s started\n"), qnp->id);
	    break;

	case 251:	/* OK, no messages waiting for node <x> */
	    if (outlevel >= O_SILENT)
		report(stdout, _("No messages waiting for %s\n"), qnp->id);
	    return(PS_NOMAIL);

	case 252:	/* OK, pending messages for node <x> started */
	case 253:	/* OK, <n> pending messages for node <x> started */
	    if (outlevel >= O_SILENT)
		report(stdout, _("Pending messages for %s started\n"), qnp->id);
	    break;

	case 458:	/* Unable to queue messages for node <x> */
	    report(stderr, _("Unable to queue messages for node %s\n"),qnp->id);
	    return(PS_PROTOCOL);

	case 459:	/* Node <x> not allowed: <reason> */
	    report(stderr, _("Node %s not allowed: %s\n"), qnp->id, buf);
	    return(PS_AUTHFAIL);

	case 500:	/* Syntax Error */
	    report(stderr, _("ETRN syntax error\n"));
	    return(PS_PROTOCOL);

	case 501:	/* Syntax Error in Parameters */
	    report(stderr, _("ETRN syntax error in parameters\n"));
	    return(PS_PROTOCOL);

	default:
	    report(stderr, _("Unknown ETRN error %d\n"), atoi(buf));
	    return(PS_PROTOCOL);
	}
    }

    return(0);
}

static int etrn_logout(int sock, struct query *ctl)
/* send logout command */
{
    return(gen_transact(sock, "QUIT"));
}

const static struct method etrn =
{
    "ETRN",		/* ESMTP ETRN extension */
#if INET6
    "smtp",		/* standard SMTP port */
#else /* INET6 */
    25,			/* standard SMTP port */
#endif /* INET6 */
    FALSE,		/* this is not a tagged protocol */
    FALSE,		/* this does not use a message delimiter */
    etrn_ok,		/* parse command response */
    NULL,		/* no password canonicalization */
    NULL,		/* no need to get authentication */
    etrn_getrange,	/* initialize message sending */
    NULL,		/* we cannot get a list of sizes */
    NULL,		/* how do we tell a message is old? */
    NULL,		/* no way to fetch headers */
    NULL,		/* no way to fetch body */
    NULL,		/* no message trailer */
    NULL,		/* how to delete a message */
    etrn_logout,	/* log out, we're done */
    FALSE,		/* no, we can't re-poll */
};

int doETRN (struct query *ctl)
/* retrieve messages using ETRN */
{
    int status;

    if (ctl->keep) {
	fprintf(stderr, _("Option --keep is not supported with ETRN\n"));
	return(PS_SYNTAX);
    }
    if (ctl->flush) {
	fprintf(stderr, _("Option --flush is not supported with ETRN\n"));
	return(PS_SYNTAX);
    }
    if (ctl->mailboxes->id) {
	fprintf(stderr, _("Option --remote is not supported with ETRN\n"));
	return(PS_SYNTAX);
    }
    if (check_only) {
	fprintf(stderr, _("Option --check is not supported with ETRN\n"));
	return(PS_SYNTAX);
    }
    peek_capable = FALSE;

    status = do_protocol(ctl, &etrn);
    if (status == PS_NOMAIL)
	status = PS_SUCCESS;
    return(status);
}
#endif /* ETRN_ENABLE */

/* etrn.c ends here */
