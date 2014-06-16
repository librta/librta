/***************************************************************
 * Run Time Access Library
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * token.l  -- Lex tokenizer for SQL commands.  
 **************************************************************/
%option noyywrap
%option never-interactive

%{
#include <string.h>
#include "do_sql.h"
#include "parse.tab.h"

#define yylval  rta_yylval
#define yyparse rta_yyparse

#define YY_NO_UNPUT

void rta_dosql_init();
extern int yylval;
extern int yydebug;
extern struct Sql_Cmd rta_cmd;
extern char *rta_parsestr[];

#ifndef __USE_GNU
static char *strndup(const char *, unsigned);
#endif
%}

%%

[Aa][Nn][Dd]				{ return(AND); }
[Ff][Rr][Oo][Mm]			{ return(FROM); }
[Ll][Ii][Mm][Ii][Tt]		{ return(LIMIT); }
[Oo][Ff][Ff][Ss][Ee][Tt]	{ return(OFFSET); }
[Ss][Ee][Ll][Ee][Cc][Tt]	{ return(SELECT); }
[Ss][Ee][Tt]				{ return(SET); }
[Uu][Pp][Dd][Aa][Tt][Ee]	{ return(UPDATE); }
[Ii][Nn][Ss][Ee][Rr][Tt]	{ return(INSERT); }
[Ii][Nn][Tt][Oo]		{ return(INTO); }
[Vv][Aa][Ll][Uu][Ee][Ss]	{ return(VALUES); }
[Dd][Ee][Ll][Ee][Tt][Ee]	{ return(DELETE); }
[Ww][Hh][Ee][Rr][Ee]		{ return(WHERE); }

\"[A-Za-z][_A-Za-z0-9 \t]*\"	|
\'[A-Za-z][_A-Za-z0-9 \t]*\'	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (rta_parsestr[i] == (char *) NULL) {
							rta_parsestr[i] = strndup(&yytext[1], yyleng-2);
							break;
						}
					}
					yylval = i;
					return(NAME);
				}
\*				|
[A-Za-z][_A-Za-z0-9]*		{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (rta_parsestr[i] == (char *) NULL) {
							rta_parsestr[i] = strdup(yytext);
							break;
						}
					}
					yylval = i;
					return(NAME);
				}

\"-?[0-9]+\"	|
\'-?[0-9]+\'	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (rta_parsestr[i] == (char *) NULL) {
							rta_parsestr[i] = strndup(&yytext[1], yyleng-2);
							break;
						}
					}
					yylval = i;
					return(INTEGER);
				}
-?[0-9]+		{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (rta_parsestr[i] == (char *) NULL) {
							rta_parsestr[i] = strdup(yytext);
							break;
						}
					}
					yylval = i;
					return(INTEGER);
				}
\"-?[0-9]+\.[0-9]*\"	|
\'-?[0-9]+\.[0-9]*\'	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (rta_parsestr[i] == (char *) NULL) {
							rta_parsestr[i] = strndup(&yytext[1], yyleng-2);
							break;
						}
					}
					yylval = i;
					return(REALNUM);
				}
-?[0-9]+\.[0-9]*	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (rta_parsestr[i] == (char *) NULL) {
							rta_parsestr[i] = strdup(yytext);
							break;
						}
					}
					yylval = i;
					return(REALNUM);
				}

=				{ return(EQ); }
\!=			 	{ return(NE); }
\>				{ return(GT); }
\<				{ return(LT); }
\>=				{ return(GE); }
\<=				{ return(LE); }
\<\>				{ return(NE); }

\,				{ return((int)','); }
\(				{ return((int)'('); }
\)				{ return((int)')'); }

\"[A-Za-z0-9 \t!@#$%^&*()_+-={}|;:<>?~`\[\]'\\]*\"	|
\'[A-Za-z0-9 \t!@#$%^&*()_+-={}|;:<>?~`\[\]"\\]*\'	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (rta_parsestr[i] == (char *) NULL) {
							rta_parsestr[i] = strndup(&yytext[1], yyleng-2);
							break;
						}
					}
					yylval = i;
					return(STRING);
				}

[ \t\n]+		{ /* Ignore white space */ }

\;					|
\000				|
<<EOF>>			{ return(TERMINATOR); /* done with this buffer of SQL */ }

.						{ return(GARBAGE);  /* Causes a syntax error */}

%%


void rta_SQL_string(char *s, int incnt, char *out, int *nout)
{
    extern int yyparse();
    YY_BUFFER_STATE x;
    int             i;

    /* Sanity checks */
    if (!s || !out || !nout) {
      return;
    }

    rta_dosql_init();
    rta_cmd.out  = out;
    rta_cmd.nout = nout;
    rta_cmd.sqlcmd = s;

    /* We need to store the start addr of the buffer in case we
     * we need to send an error message after we've started 
     * sending a reply.  */
    rta_cmd.errout   = out;
    rta_cmd.nerrout  = *nout;
    rta_cmd.nlineout = 0;

    x = yy_scan_bytes(s, incnt);

    while(yyparse() == 0) {
        /* At this point we have parsed the command.  */
        /* If no errors were detected, we can continue processing */
        if (!rta_cmd.err) {
            /* everything is set.  do the command */
            rta_do_sql(&out[rta_cmd.nerrout - *nout], nout);
        }
        else {
            yy_delete_buffer(x);
            return;
        }
        rta_dosql_init();   /* free memory and re-init the rta_cmd structure */
    }

    /* We are done processing the command and have assembled response */
    /* Tell the other end that we are ready for a new command.  */
    i = rta_cmd.nerrout - *nout;
    out[i++] = 'Z';               /* Ready */
    out[i++] = 0;                 /* byte 4 of length */
    out[i++] = 0;                 /* byte 3 of length */
    out[i++] = 0;                 /* byte 2 of length */
    out[i++] = 5;                 /* byte 1 of length */
    out[i++] = 'I';               /* status of result */
    *nout -= 6;
    yy_delete_buffer(x);
    return;
}

#ifndef __USE_GNU
static char *strndup(const char *string, unsigned length)
{
    char *newstring = malloc(length + 1);

    if (newstring != NULL) {
        strncpy(newstring, string, length);
        newstring[length] = '\0';
    }
    return newstring;
}
#endif

#ifdef xxxx
    extern int yydebug;
    yydebug = 1;

    printf("N-out = %d\n", *rta_cmd.nout);
    printf("Scanning ---%s---\n", s);

        printf("Limit  = %d\n", rta_cmd.limit);
        printf("Offset = %d\n", rta_cmd.offset);
        printf("Table = %s\n", rta_cmd.tbl);
        for (i=0; i<rta_cmd.ncols; i++)
             printf("   %s  %s\n",rta_cmd.cols[i],rta_cmd.updvals[i]);
        for (i=0; i<rta_cmd.nwhrcols; i++)
             printf("   %s  %d  %s\n",rta_cmd.whrcols[i], rta_cmd.whrrel[i], rta_cmd.whrvals[i]);
        printf("Error  = %d\n", rta_cmd.err);

        printf("Limit  = %d\n", rta_cmd.limit);
        printf("Offset = %d\n", rta_cmd.offset);
        printf("N-out  = %d\n", *rta_cmd.nout);
        printf("Error  = %d\n", rta_cmd.err);
#endif
