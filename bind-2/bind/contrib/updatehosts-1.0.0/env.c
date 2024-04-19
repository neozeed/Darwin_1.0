#include <stdio.h>
#include <sys/stat.h>
#include "env.h"

extern struct environment environment[];

int
changeenv(name, value)
char *name;
char *value;
{
	int i;

	for (i=0; environment[i].name != NULL; i++) {
		if (strcmp(environment[i].name, name) == 0) {
			environment[i].value = (char *)malloc(strlen(value)+1);
			strcpy(environment[i].value, value);
			return 1;
		}
	}
	return 0;
}

char *
getenvval(name)
char *name;
{
	int i;

	for (i=0; environment[i].name != NULL; i++) {
		if (strcmp(environment[i].name, name) == 0) {
			return(environment[i].value);
		}
	}
	return NULL;
}

int
safe(filename)
char *filename;
{
	struct stat status;

	if (stat(filename, &status) == -1) {
		fprintf(stderr, "%s cannot stat\n", filename);
		return 0;
	}
	if (status.st_uid != 0) {
		fprintf(stderr, "%s not owned by root\n", filename);
		return 0;
	}
	if (status.st_nlink > 1) {
		fprintf(stderr, "%s has more than one link\n", filename);
		return 0;
	}
	if (status.st_mode&(S_IWGRP|S_IWOTH)) {
		fprintf(stderr, "%s can be written by others\n", filename);
		return 0;
	}
	return 1;
}
