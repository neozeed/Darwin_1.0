#include "srv.h"

#include <stdio.h> /* fprintf */
#include <stdlib.h> /* srandom */
#include <unistd.h> /* getpid */
#include <time.h> /* time */

int main( int argc, char ** argv )
{
    struct srvhost * r;
    struct srvhost * s;

    if ( argc < 3 || argc > 4 ) {
	fprintf( stderr,
		 "%s: usage: %s service domain [protocol, defaults to tcp]\n"
		 "%s: examples: <%s telnet troll.no> <%s nfs troll.no udp>\n",
		 argv[0], argv[0], argv[0], argv[0], argv[0] );
	exit( 1 );
    }

    /* set up the random generator so weight will be randomized properly */
    srandom( getpid() * time( NULL ) );

    r = getsrv( argv[2], argv[1], argv[3] ? argv[3] : "tcp" );
    if ( !r ) {
	fprintf( stderr, "%s: failed.  figure it out, please.\n", argv[0] );
	exit( 2 );
    }

    s = r;
    while( s ) {
	printf( "%5d %5d %s:%d\n", s->pref, s->weight, s->name, s->port );
	s = s->next;
    }

    /* just to test freesrvhost(), really */
    freesrvhost( r );

    return 0;
}
