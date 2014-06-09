/***************************************************************
 * parse.y  -- Yacc parser for our subset of SQL.
 **************************************************************/

%{
#include "do_sql.h"


/* This works around a problem in the yacc output */
typedef char * STRPTR;
#define YYSTYPE STRPTR



/* While we parse the SET and WHERE clause we need someplace
 * to temporarily store the type of relation */
static int  whrrelat;

static int   n;            /* temp/scratch integer */

extern struct Sql_Cmd cmd; /* encoded SQL command (a global) */
extern char *yytext;
extern int   yyleng;
extern void  yyerror(char *);
extern int   yylex();
%}

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

sql:
		select_statement
	|	update_statement
	|	function_call
	;


select_statement:
		SELECT column_list FROM NAME where_clause limit_clause
		{	cmd.command = RTA_SELECT;
			cmd.tbl = $4;
		}
	;

column_list:
		NAME
		{	cmd.cols[cmd.ncols] = $1;
			cmd.ncols++;
		}
	|	column_list "," NAME
		{	cmd.cols[cmd.ncols] = $3;
			cmd.ncols++;
			if (cmd.ncols > NCMDCOLS) {
				/* too many columns in list */
				send_error(LOC, E_BADPARSE);
			}
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
			cmd.whrcols[n] = $1;
			cmd.whrrel[n] = whrrelat;
			cmd.whrvals[n] = $3;
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
		{	cmd.limit  = atoi($2);
			free($2);
		}
	|	LIMIT INTEGER OFFSET INTEGER
		{	cmd.limit  = atoi($2);
			cmd.offset = atoi($4);
			free($2);
			free($4);
		}
	;


update_statement:
		UPDATE NAME SET set_list where_clause limit_clause
		{	cmd.command = RTA_UPDATE;
			cmd.tbl     = $2;
		}
	;

set_list:
		set_list ',' set_list
	|	NAME EQ literal
		{	n = cmd.ncols;
			cmd.cols[n] = $1;
			cmd.updvals[n] = $3;
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
		SELECT NAME '(' ')'
		{	cmd.command = RTA_CALL;
			cmd.tbl = $2;
		}
	;


%%


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

    cmd.tbl = (char *) 0;
    cmd.ptbl = (TBLDEF *) 0;
    cmd.ncols    = 0;
    cmd.nwhrcols = 0;
    for (i=0; i<NCMDCOLS; i++) {
        cmd.cols[i]    = (char *) 0;
        cmd.updvals[i] = (char *) 0;
        cmd.whrcols[i] = (char *) 0;
        cmd.whrvals[i] = (char *) 0;
    }
    cmd.limit  = 1<<30;  /* no real limit */
    cmd.offset = 0;
    cmd.err    = 0;
}

