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
extern char *yylval;
extern int yydebug;
extern struct Sql_Cmd cmd;
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
[Bb][Ee][Gg][Ii][Nn]		{ return(SQLBEGIN); printf("got begin\n");}
[Cc][Oo][Mm][Mm][Ii][Tt]	{ return(SQLCOMMIT); }

\"[A-Za-z][_A-Za-z0-9 \t]*\"	|
\'[A-Za-z][_A-Za-z0-9 \t]*\'	{
					yylval = strndup(&yytext[1], yyleng-2);
					return(NAME);
				}
\*				|
[A-Za-z][_A-Za-z0-9]*		{
					yylval = strdup(yytext);
					return(NAME);
				}

\"-?[0-9]+\"	|
\'-?[0-9]+\'	{
					yylval = strndup(&yytext[1], yyleng-2);
					return(INTEGER);
				}
-?[0-9]+		{
					yylval = strdup(yytext);
					return(INTEGER);
				}
\"-?[0-9]+\.[0-9]*\"	|
\'-?[0-9]+\.[0-9]*\'	{
                    yylval = strndup(&yytext[1], yyleng-2);
					return(REALNUM);
				}
-?[0-9]+\.[0-9]*	{
					yylval = strdup(yytext);
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
					yylval = strndup(&yytext[1], yyleng-2);
					return(STRING);
				}
[ \t\n]+		{ }

%%

void yyerror(char *s)
{
    send_error(LOC, E_BADPARSE);
    return;
}


void SQL_string(char *s, char *out, int *nout)
{
    extern int yyparse();

    YY_BUFFER_STATE x;
    int    i;

    yydebug = 0;

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

    dosql_init();
    while(yyparse() == 0) {
        /* At this point we have parsed the command.  */
        /* If no errors were detected, we can continue processing */
        if (!cmd.err) {
            /* everything is set.  do the command */
            do_sql(&out[cmd.nerrout - *nout], nout);
        }
        /* Success or failure: Either way, free allocated memory */
        for (i=0; i<NCMDCOLS; i++) {
            if (cmd.cols[i])
                free(cmd.cols[i]);
            if (cmd.updvals[i])
                free(cmd.updvals[i]); /* values for column updates */
            if (cmd.whrcols[i])
                free(cmd.whrcols[i]); /* cols in where */
            if (cmd.whrvals[i])
                free(cmd.whrvals[i]); /* values in where clause */
        }
        free(cmd.tbl);
        dosql_init();   /* and re-init the cmd structure */
        if (cmd.err) {
            yy_delete_buffer(x);
            return;
        }
    }
    /* We are done processing the command and have assembled response */
    /* Tell the other end that we are ready for a new command.  */
    out[cmd.nerrout - *nout] = 'Z';               /* Ready */
    (*nout)--;
    yy_delete_buffer(x);
    return;
}


#ifdef xxxx

#ifdef DEBUG
    printf("N-out = %d\n", *cmd.nout);
    printf("Scanning ---%s---\n", s);
#endif

#ifdef DEBUG
        printf("Limit  = %d\n", cmd.limit);
        printf("Offset = %d\n", cmd.offset);
#endif


#ifdef DEBUG
printf("Command = %d\n", cmd.command);
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
#endif
