#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "daemonize.h"
#include <sys/wait.h>

pid_t
daemonize()
{
int   peip[2]; /* for synchronizing parent and child */
pid_t child, gchild;
int   nulfd;
int   stat;

	if ( pipe( peip ) ) {
		perror("daemonize: unable to create synchronization pipe");
		return (pid_t)-1;
	}

	child = fork();

	if ( (pid_t)-1 == child ) {
		perror("daemonize: unable to fork");
		close( peip[0] );
		close( peip[1] );
		return child;
	}

	if ( child ) {
		/* parent process */
		waitpid(child, &stat, 0);
		if ( sizeof(gchild) != read( peip[0], &gchild, sizeof(gchild) ) ) {
			perror("daemonize: unable to synchronize with child!\n");
			gchild = (pid_t)-1;	
		}
		close( peip[0] );
		close( peip[1] );
		return gchild;
	}

	/* here we are executing in the child process */

	/* Don't need 'read' end */
	close( peip[0] );

	/* Assume we have no other open descriptors */
	/* Assume we have no signal handlers        */
	/* Assume we have no relevant environment   */

	/* make new session                         */
	if ( (pid_t) -1 == setsid() ) {
		perror("daemonize: child unable to 'setsid'");
		goto bail;
	}

	/* Second 'fork'                            */
	gchild = fork();


	if ( (pid_t)-1 == gchild ) {
		perror("daemonize: unable to fork in child");
		goto bail;
	}

	if ( gchild ) {
		/* exit second 'parent' */
		exit(0);
	}

	umask( 0 );

	if ( chdir( "/" ) ) {
		perror("daemonize: unable to chdir daemon to '/'");
		goto bail;
	}

	/* Assume we have no privilege to drop */

	if ( (nulfd = open( "/dev/null", O_RDWR, 0 )) < 0 ) {
		perror("daemonize: unable to open '/dev/null'");
		goto bail;
	}

	/* Once we redirect stdio we cannot 'perror' anymore! */

	close( 0 );
	close( 1 );
	close( 2 );

	if ( dup( nulfd ) )
		goto bail;
	if ( dup( nulfd ) )
		goto bail;
	if ( dup( nulfd ) )
		goto bail;

	close( nulfd );

	gchild = getpid();
	
	/* Notify parent that we're done */
	if ( write( peip[1], &gchild, sizeof(gchild) ) < 0 )
		goto bail;

	close( peip[1] );

	return (pid_t)0;

bail:
	gchild = (pid_t) -1;
	(void)write( peip[1], &gchild, sizeof(gchild) );
	exit(1);
	/* never get here */
	return (pid_t)-1;
}
