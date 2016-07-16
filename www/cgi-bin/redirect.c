/* redirect - simple redirection CGI program
**
** Copyright � 1995 by Jef Poskanzer <jef@mail.acme.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/

/* Three steps to set up a redirection:
**  1. Make sure your web server is set up to allow CGI programs.
**  2. Make a symbolic link from the file you want to redirect,
**     pointing at this program in the CGI bin directory.
**  3. Add an entry to the file ".redirects" in the directory where your
**     http server runs CGI programs.  For most servers, this is the
**     directory where the given CGI program lives.  The format of the
**     file is a bunch of lines with a filename, whitespace, and the new
**     URL.  For example:

/test/oldfile.html    http://www.acme.com/test/newfile.html

**     The easiest way to figure out precisely what filename to put into
**     .redirects is to set up the symlink and then click on it.  You'll get
**     back a "404 Not Found" page which includes the filename as received by
**     the redirect program, and that's what you want to use.
**
** Note: this is designed for thttpd (http://www.acme.com/software/thttpd/)
** and using it with other web servers may require some hacking.  A possible
** gotcha is with the symbolic link from the old file pointing at this
** script - servers other than thttpd may not allow that link to be run
** as a CGI program, because they don't check the link to see that it
** points into the allowed CGI directory.
**
** Note two: It would be really cool to have this program look for
** the .redirects file in the same directory as the file being redirected,
** instead of in the binaries directory.  Unfortunately, this appears
** to be impossible with the information CGI gives, plus the non-standardized
** but widespread practice of running CGI programs in the directory where
** the binary lives.  Perhaps CGI 1.2 will address this.
*/

#include "config.h"

//system headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//local headers
#include <thttpd.h>

static char* argv0;


static void
internal_error( char* reason )
    {
    char* title = "500 Internal Error";

    (void) printf( "\
Status: %s\n\
Content-type: text/html\n\
\n\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY><H2>%s</H2>\n\
Something unusual went wrong during a redirection request:\n\
<BLOCKQUOTE>\n\
%s\n\
</BLOCKQUOTE>\n\
</BODY></HTML>\n", title, title, title, reason );
    }


static void
not_found( char* script_name )
    {
    char* title = "404 Not Found";

    (void) printf( "\
Status: %s\n\
Content-type: text/html\n\
\n\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY><H2>%s</H2>\n\
The requested filename, %s, is set up to be redirected to another URL;\n\
however, the new URL has not yet been specified.\n\
</BODY></HTML>\n", title, title, title, script_name );
    }


static void
moved( char* script_name, char* url )
    {
    char* title = "Moved";

    (void) printf( "\
Location: %s\n\
Content-type: text/html\n\
\n\
<HTML><HEAD><TITLE>%s</TITLE></HEAD>\n\
<BODY><H2>%s</H2>\n\
The requested filename, %s, has moved to a new URL:\n\
<A HREF=\"%s\">%s</A>.\n\
</BODY></HTML>\n", url, title, title, script_name, url, url );
    }
	

int
main( int argc, char** argv )
    {
    char* script_name;
    char* path_info;
    char* cp;
    FILE* fp;
    char *star;
    char buf[5000], file[5000], url[5000];

    argv0 = argv[0];

    /* Get the name that we were run as, which is the filename being
    ** redirected.
    */
    script_name = getenv( "SCRIPT_NAME" );
    if ( script_name == (char*) 0 )
	{
	internal_error( "Couldn't get SCRIPT_NAME environment variable." );
	exit( 1 );
	}

    /* Append the PATH_INFO, if any.  This allows redirection of whole
    ** directories.
    */
    path_info = getenv( "PATH_INFO" );
    if ( path_info != (char*) 0 )
	{
	cp = (char*) malloc( strlen( script_name ) + strlen( path_info ) + 1 );
	if ( cp == (char*) 0 )
	    {
	    internal_error( "Out of memory." );
	    exit( 1 );
	    }
	(void) sprintf( cp, "%s%s", script_name, path_info );
	script_name = cp;
	}

    /* Open the redirects file. */
    fp = fopen( ".redirects", "r" );
    if ( fp == (FILE*) 0 )
	{
	internal_error( "Couldn't open .redirects file." );
	exit( 1 );
	}

    /* Search the file for a matching entry. */
    while ( fgets( buf, sizeof(buf), fp ) != (char*) 0 )
	{
	/* Remove comments. */
	cp = strchr( buf, '#' );
	if ( cp != (char*) 0 )
	    *cp = '\0';
	/* Skip leading whitespace. */
	cp = buf;
	cp += strspn( cp, " \t" );
	/* Check for blank line. */
	if ( *cp != '\0' )
	    {
	    /* Parse line. */
	    if ( sscanf( cp, "%[^ \t\n] %[^ \t\n]", file, url ) == 2 )
		{
		/* Check for wildcard match. */
		star = strchr( file, '*' );
		if ( star != (char*) 0 )
		    {
		    /* Check for leading match. */
		    if ( strncmp( file, script_name, star - file ) == 0 )
			{
			/* Got it; put together the full name. */
			strcat( url, script_name + ( star - file ) );
			/* XXX Whack the script_name, too? */
			moved( script_name, url );
			exit( 0 );
			}
		    }
		/* Check for exact match. */
		if ( strcmp( file, script_name ) == 0 )
		    {
		    /* Got it. */
		    moved( script_name, url );
		    exit( 0 );
		    }
		}
	    }
	}

    /* No match found. */
    not_found( script_name );
    exit( 1 );
    }
