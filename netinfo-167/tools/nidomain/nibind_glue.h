/*
 * nibindd glue definitions
 * Copyright 1989-94, NeXT Computer Inc.
 */
void *nibind_new(struct in_addr *);
ni_status nibind_listreg(void *, nibind_registration **, unsigned *);
ni_status nibind_createmaster(void *, char *);
ni_status nibind_createclone(void *, char *, char *, struct in_addr *, char *);
ni_status nibind_destroydomain(void *, char *);
void nibind_free(void *);

