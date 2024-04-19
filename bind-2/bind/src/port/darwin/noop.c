/*
 * GG 19980728 apparently ranlib complains if this function is called
 * noop as it was originally
 */
extern void mynoop();

void
mynoop() {
	int i;
	i = 10;
	/* NOOP */
}
