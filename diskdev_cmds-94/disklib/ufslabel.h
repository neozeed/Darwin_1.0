
/* 
 * Copyright 1999 Apple Computer, Inc.
 *
 * ufslabel.h
 * - library routines to read/write the UFS disk label
 */

/*
 * Modification History:
 * 
 * Dieter Siegmund (dieter@apple.com)	Fri Nov  5 12:48:55 PST 1999
 * - created
 */
boolean_t	ufslabel_get(int fd, void * name, int * len);
boolean_t	ufslabel_set(int fd, void * name, int len);
