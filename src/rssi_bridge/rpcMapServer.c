#include <protRelayUtil.h>
#include <daemonize.h>
#include <printErrMsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>

int
rpcMapServer()
{
pid_t    relay;
pid_t    parent;
sigset_t mask, omask;
int      got;

	if (  ! protMap() ) {
		fprintf(stderr,"No RELAY seems to be running!; try to start one...\n");
		sigemptyset( &mask );
		sigaddset  ( &mask, SIGUSR1 );
		sigprocmask( SIG_BLOCK, &mask, &omask );
		parent = getpid();
		relay  = daemonize();
		if ( (pid_t)0 == relay ) {
			/* daemon child */
			SVCXPRT *relsvc;
			sigprocmask( SIG_SETMASK, &omask, 0 );
			openlog( "RSSIB Relay", LOG_CONS, LOG_DAEMON );
			syslog( LOG_ERR, "Relay starting up\n");
			printErrMsgTo( PRINT_ERR_MSG_SYSLOG );
			relsvc = protRelay();
			/* let the parent know */
			kill( parent, SIGUSR1 );
			if ( relsvc ) {
				svc_run();
			}
			/* This exits the daemon child */
			exit(1);
		}


		sigwait( &mask,  &got );

		fprintf( stderr,"Relay started; PID: %lu\n", (unsigned long) relay );

		if ( ! protMap() ) {
			fprintf(stderr,"Second attempt to start MAP FAILED\n");
			return 1;
		}
	}
	return 0;
}
