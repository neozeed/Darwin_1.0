#define DNSHDRSIZE 12
   
struct dnshdr {
	unsigned short int id;

	unsigned char  rd:1;           /* recursion desired */
	unsigned char  tc:1;           /* truncated message */
	unsigned char  aa:1;           /* authoritive answer */
	unsigned char  opcode:4;       /* purpose of message */
	unsigned char  qr:1;           /* response flag */

	unsigned char  rcode:4;        /* response code */
	unsigned char  unused:2;       /* unused bits */
	unsigned char  pr:1;           /* primary server required (non standard) */
	unsigned char  ra:1;           /* recursion available */

	unsigned short int que_num;
	unsigned short int rep_num;
	unsigned short int num_rr;
	unsigned short int num_rrsup;
};
