/***************************************************************
 * Run Time Access
 * Copyright (C) 2003 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/***************************************************************
 * parse.y  -- Yacc parser for our subset of SQL.
 **************************************************************/

%{
#include <stdlib.h>
#include "do_sql.h"


#define YYSTYPE int


/* While we parse the SET and WHERE clause we need someplace
 * to temporarily store the type of relation */
static int  whrrelat;

/* We don't want to pass pointers to allocated memory on the */
/* yacc stack, since the memory might not be freed when an */
/* error is detected.  Instead, we allocate the memory and */
/* put the pointer into the following table where it is easy */
/* free on error.  */
char *parsestr[MXPARSESTR];

static int   n;            /* temp/scratch integer */

extern struct Sql_Cmd cmd; /* encoded SQL command (a global) */
extern char *yytext;
extern int   yyleng;
extern void  yyerror(char *);
extern int   yylex();
%}

%token SQLBEGIN
%token SQLCOMMIT
%token SELECT
%token UPDATE
%token FROM
%token WHERE
%token NAME
%token STRING
%token INTEGER
%token REALNUM
%token LIMIT
%token OFFSET
%token SET
		/* relations for the where clause */
%token EQ
%token NE
%token GT
%token LT
%token GE
%token LE


%left AND
%left ','

%%


command:
		{ YYABORT; }   
	|	begin_statement
	|	commit_statement
	|	select_statement
	|	update_statement
	|	function_call
	;


begin_statement:
		SQLBEGIN ';'
		{	cmd.command = RTA_BEGIN;
			YYACCEPT;
		}
	|	SQLBEGIN
		{	cmd.command = RTA_BEGIN;
			YYACCEPT;
		}
	;

commit_statement:
		SQLCOMMIT ';'
		{	cmd.command = RTA_COMMIT;
			YYACCEPT;
		}
	|	SQLCOMMIT
		{	cmd.command = RTA_COMMIT;
			YYACCEPT;
		}
	;

select_statement:
		SELECT column_list FROM table_name where_clause limit_clause ';'
		{	cmd.command = RTA_SELECT;
			YYACCEPT;
		}
	|	SELECT column_list FROM table_name where_clause limit_clause
		{	cmd.command = RTA_SELECT;
			YYACCEPT;
		}
	;

column_list:
		NAME
		{	cmd.cols[cmd.ncols] = parsestr[(int) $1];
			parsestr[(int) $1] = (char *) NULL;
			cmd.ncols++;
		}
	|	column_list "," NAME
		{	cmd.cols[cmd.ncols] = parsestr[(int) $3];
			parsestr[(int) $3] = (char *) NULL;
			cmd.ncols++;
			if (cmd.ncols > NCMDCOLS) {
				/* too many columns in list */
				send_error(LOC, E_BADPARSE);
			}
		}
	;

table_name:
		NAME
		{
			cmd.tbl = parsestr[(int) $1];
			parsestr[(int) $1] = (char *) NULL;
		}
	|	NAME "." NAME
		{
			cmd.tbl = parsestr[(int) $3];
			parsestr[(int) $3] = (char *) NULL;
		}
	;


where_clause:
		/* empty, optional */
	|	WHERE test_condition
	;


test_condition:
		'(' test_condition ')'
	|	test_condition AND test_condition
	|	NAME relation literal
		{	n = cmd.nwhrcols;
			cmd.whrcols[n] = parsestr[(int) $1];
			parsestr[(int) $1] = (char *) NULL;
			cmd.whrrel[n] = whrrelat;
			cmd.whrvals[n] = parsestr[(int) $3];
			parsestr[(int) $3] = (char *) NULL;
			cmd.nwhrcols++;
			if (cmd.nwhrcols > NCMDCOLS) {
				/* too many columns in list */
				send_error(LOC, E_BADPARSE);
			}
		}
	;


relation:
		EQ		{	whrrelat = RTA_EQ; }
	|	NE		{	whrrelat = RTA_NE; }
	|	GT		{	whrrelat = RTA_GT; }
	|	LT		{	whrrelat = RTA_LT; }
	|	GE		{	whrrelat = RTA_GE; }
	|	LE		{	whrrelat = RTA_LE; }
	;


limit_clause:
		/* empty, optional */
	|	LIMIT INTEGER
		{	cmd.limit  = atoi(parsestr[(int) $2]);
            free(parsestr[(int) $2]);
			parsestr[(int) $2] = (char *) NULL;
		}
	|	LIMIT INTEGER OFFSET INTEGER
		{	cmd.limit  = atoi(parsestr[(int) $2]);
            free(parsestr[(int) $2]);
			parsestr[(int) $2] = (char *) NULL;
			cmd.offset = atoi(parsestr[(int) $4]);
            free(parsestr[(int) $4]);
			parsestr[(int) $4] = (char *) NULL;
		}
	;


update_statement:
		UPDATE NAME SET set_list where_clause limit_clause ';'
		{	cmd.command = RTA_UPDATE;
			cmd.tbl     = parsestr[(int) $2];
			parsestr[(int) $2] = (char *) NULL;
		}
	|	UPDATE NAME SET set_list where_clause limit_clause
		{	cmd.command = RTA_UPDATE;
			cmd.tbl     = parsestr[(int) $2];
			parsestr[(int) $2] = (char *) NULL;
		}
	;

set_list:
		set_list ',' set_list
	|	NAME EQ literal
		{	n = cmd.ncols;
			cmd.cols[n] = parsestr[(int) $1];
			parsestr[(int) $1] = (char *) NULL;
			cmd.updvals[n] = parsestr[(int) $3];
			parsestr[(int) $3] = (char *) NULL;
			cmd.ncols++;
			if (cmd.ncols > NCMDCOLS) {
				/* too many columns in list */
				send_error(LOC, E_BADPARSE);
			}
		}
	;

literal:
		NAME
	|	STRING
	|	INTEGER
	|	REALNUM
	;


function_call:
		SELECT NAME '(' ')' ';'
		{	cmd.command = RTA_CALL;
			cmd.tbl = parsestr[(int) $2];
			parsestr[(int) $2] = (char *) NULL;
			YYACCEPT;
		}
	|	SELECT NAME '(' ')'
		{	cmd.command = RTA_CALL;
			cmd.tbl = parsestr[(int) $2];
			parsestr[(int) $2] = (char *) NULL;
			YYACCEPT;
		}
	;


%%


/***************************************************************
 * freesql(): - Free allocated memory from previous command.
 *
 * Input:        None.
 * Output:       None.
 * Affects:      Frees memory from last command
 ***************************************************************/


/***************************************************************
 * dosql_init(): - Set up data structures prior to parse of
 * an SQL command.
 *
 * Input:        None.
 * Output:       None.
 * Affects:      structure cmd is initialized
 ***************************************************************/
void dosql_init() {
    int   i;

    for (i=0; i<NCMDCOLS; i++) {
        if (cmd.cols[i])
            free(cmd.cols[i]);
        if (cmd.updvals[i])
            free(cmd.updvals[i]); /* values for column updates */
        if (cmd.whrcols[i])
            free(cmd.whrcols[i]); /* cols in where */
        if (cmd.whrvals[i])
            free(cmd.whrvals[i]); /* values in where clause */
        cmd.cols[i]    = (char *) 0;
        cmd.updvals[i] = (char *) 0;
        cmd.whrcols[i] = (char *) 0;
        cmd.whrvals[i] = (char *) 0;
    }
    if (cmd.tbl);
        free(cmd.tbl);
    for (i=0; i<MXPARSESTR; i++) {
        if (parsestr[i]) {
            free(parsestr[i]);
            parsestr[i] = (char *) NULL;
        }
    }

    cmd.tbl = (char *) 0;
    cmd.ptbl = (TBLDEF *) 0;
    cmd.ncols    = 0;
    cmd.nwhrcols = 0;
    cmd.limit  = 1<<30;  /* no real limit */
    cmd.offset = 0;
    cmd.err    = 0;
}

void yyerror(char *s)
{
    send_error(LOC, E_BADPARSE);
    return;
}


