// GG 19980728 apparently ranlib complains is this function is called
// noop as it was originally
extern void mynoop();

void
mynoop() {
	int i;
	i = 10;
	/* NOOP */
}
