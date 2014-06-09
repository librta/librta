/***************************************************************
 * Run Time Access
 * Copyright (C) 2003 Robert W Smith (bsmith@linuxtoys.org)
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
#define __USE_GNU
#include <string.h>
#include "do_sql.h"
#include "parse.tab.h"

void dosql_init();
extern int yylval;
extern int yydebug;
extern struct Sql_Cmd cmd;
extern char *parsestr[];
%}

%%

[Aa][Nn][Dd]				{ return(AND); }
[Ff][Rr][Oo][Mm]			{ return(FROM); }
[Ll][Ii][Mm][Ii][Tt]		{ return(LIMIT); }
[Oo][Ff][Ff][Ss][Ee][Tt]	{ return(OFFSET); }
[Ss][Ee][Ll][Ee][Cc][Tt]	{ return(SELECT); }
[Ss][Ee][Tt]				{ return(SET); }
[Uu][Pp][Dd][Aa][Tt][Ee]	{ return(UPDATE); }
[Ww][Hh][Ee][Rr][Ee]		{ return(WHERE); }

\"[A-Za-z][_A-Za-z0-9 \t]*\"	|
\'[A-Za-z][_A-Za-z0-9 \t]*\'	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (parsestr[i] == (char *) NULL) {
							parsestr[i] = strndup(&yytext[1], yyleng-2);
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
						if (parsestr[i] == (char *) NULL) {
							parsestr[i] = strdup(yytext);
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
						if (parsestr[i] == (char *) NULL) {
							parsestr[i] = strndup(&yytext[1], yyleng-2);
							break;
						}
					}
					yylval = i;
					return(INTEGER);
				}
-?[0-9]+		{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (parsestr[i] == (char *) NULL) {
							parsestr[i] = strdup(yytext);
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
						if (parsestr[i] == (char *) NULL) {
							parsestr[i] = strndup(&yytext[1], yyleng-2);
							break;
						}
					}
					yylval = i;
					return(REALNUM);
				}
-?[0-9]+\.[0-9]*	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (parsestr[i] == (char *) NULL) {
							parsestr[i] = strdup(yytext);
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

\,				{ return((int)','); }
\(				{ return((int)'('); }
\)				{ return((int)')'); }
\;				{ return((int)';'); }
\.				{ return((int)'.'); }

\"[A-Za-z0-9 \t!@#$%^&*()_+-={}|;:<>?~`\[\]'\\]*\"	|
\'[A-Za-z0-9 \t!@#$%^&*()_+-={}|;:<>?~`\[\]"\\]*\'	{
					int i;
					for (i=0; i<MXPARSESTR; i++) {
						if (parsestr[i] == (char *) NULL) {
							parsestr[i] = strndup(&yytext[1], yyleng-2);
							break;
						}
					}
					yylval = i;
					return(STRING);
				}
[ \t\n]+		{ }

%%


void SQL_string(char *s, char *out, int *nout)
{
    extern int yyparse();
    YY_BUFFER_STATE x;
    int             i;


    dosql_init();
    cmd.out  = out;
    cmd.nout = nout;
    cmd.sqlcmd = s;

    /* We need to store the start addr of the buffer in case we
     * we need to send an error message after we've started 
     * sending a reply.  */
    cmd.errout   = out;
    cmd.nerrout  = *nout;
    cmd.nlineout = 0;

    x = yy_scan_string(s);

    while(yyparse() == 0) {
        /* At this point we have parsed the command.  */
        /* If no errors were detected, we can continue processing */
        if (!cmd.err) {
            /* everything is set.  do the command */
            do_sql(&out[cmd.nerrout - *nout], nout);
        }
        else {
            yy_delete_buffer(x);
            return;
        }
        dosql_init();   /* free memory and re-init the cmd structure */
    }

    /* We are done processing the command and have assembled response */
    /* Tell the other end that we are ready for a new command.  */
    i = cmd.nerrout - *nout;
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




#ifdef xxxx

    printf("N-out = %d\n", *cmd.nout);
    printf("Scanning ---%s---\n", s);

        printf("Limit  = %d\n", cmd.limit);
        printf("Offset = %d\n", cmd.offset);


        int i;
        for (i=0; i<cmd.ncols; i++)
             printf("   %s  %s\n",cmd.cols[i],cmd.updvals[i]);
        printf("Table = %s\n", cmd.tbl);
        for (i=0; i<cmd.nwhrcols; i++)
             printf("   %s  %d  %s\n",cmd.whrcols[i],
                    cmd.whrrel[i], cmd.whrvals[i]);
        printf("Limit  = %d\n", cmd.limit);
        printf("Offset = %d\n", cmd.offset);
        printf("N-out  = %d\n", *cmd.nout);
        printf("Error  = %d\n", cmd.err);
#endif
