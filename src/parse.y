/***************************************************************
 * Run Time Access
 * Copyright (C) 2003-2006 Robert W Smith (bsmith@linuxtoys.org)
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
char *rta_parsestr[MXPARSESTR];

static int   n;            /* temp/scratch integer */
static int   n_values;     /* part of processing for INSERT values */

extern struct Sql_Cmd rta_cmd; /* encoded SQL command (a global) */
extern char *yytext;
extern int   yyleng;
extern void  yyerror(char *);
extern int   yylex();
%}

%token SELECT
%token UPDATE
%token INSERT
%token INTO
%token DELETE
%token FROM
%token WHERE
%token VALUES
%token NAME
%token STRING
%token INTEGER
%token REALNUM
%token LIMIT
%token OFFSET
%token SET
%token GARBAGE
%token TERMINATOR
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
		select_statement
	|	update_statement
	|	insert_statement
	|	delete_statement
	| empty_statement
	;

empty_statement:
	TERMINATOR
	{
		YYABORT;
	}
	;

select_statement:
		SELECT column_list FROM table_name where_clause limit_clause TERMINATOR
		{	rta_cmd.command = RTA_SELECT;
			YYACCEPT;
		}
	;

update_statement:
		UPDATE table_name SET set_list where_clause limit_clause TERMINATOR
		{	rta_cmd.command = RTA_UPDATE;
			YYACCEPT;
		}
	;

insert_statement:
		INSERT INTO table_name '(' insert_cols ')' VALUES '(' value_list ')' TERMINATOR
		{	rta_cmd.command = RTA_INSERT;
			if (n_values != rta_cmd.ncols) {
				/* Must have same number of values as columns */
				rta_send_error(LOC, E_BADPARSE);
        YYABORT;
			}
			YYACCEPT;
		}
	;

delete_statement:
		DELETE FROM table_name where_clause limit_clause TERMINATOR
		{	rta_cmd.command = RTA_DELETE;
			YYACCEPT;
		}
	;

insert_cols:
		/* empty, optional */
	|	column_list
	;

column_list:
		NAME
		{	rta_cmd.cols[rta_cmd.ncols] = rta_parsestr[(int) $1];
			rta_parsestr[(int) $1] = (char *) NULL;
			rta_cmd.ncols++;
		}
	|	column_list ',' NAME
		{	rta_cmd.cols[rta_cmd.ncols] = rta_parsestr[(int) $3];
			rta_parsestr[(int) $3] = (char *) NULL;
			rta_cmd.ncols++;
			if (rta_cmd.ncols > RTA_NCMDCOLS) {
				/* too many columns in list */
				rta_send_error(LOC, E_BADPARSE);
        YYABORT;
			}
		}
	;

table_name:
		NAME
		{
			rta_cmd.tbl = rta_parsestr[(int) $1];
			rta_parsestr[(int) $1] = (char *) NULL;
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
		{	n = rta_cmd.nwhrcols;
			rta_cmd.whrcols[n] = rta_parsestr[(int) $1];
			rta_parsestr[(int) $1] = (char *) NULL;
			rta_cmd.whrrel[n] = whrrelat;
			rta_cmd.whrvals[n] = rta_parsestr[(int) $3];
			rta_parsestr[(int) $3] = (char *) NULL;
			rta_cmd.nwhrcols++;
			if (rta_cmd.nwhrcols > RTA_NCMDCOLS) {
				/* too many columns in list */
				rta_send_error(LOC, E_BADPARSE);
        YYABORT;
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
		{	rta_cmd.limit  = atoi(rta_parsestr[(int) $2]);
			free(rta_parsestr[(int) $2]);
			rta_parsestr[(int) $2] = (char *) NULL;
		}
	|	LIMIT INTEGER OFFSET INTEGER
		{	rta_cmd.limit  = atoi(rta_parsestr[(int) $2]);
			free(rta_parsestr[(int) $2]);
			rta_parsestr[(int) $2] = (char *) NULL;
			rta_cmd.offset = atoi(rta_parsestr[(int) $4]);
			free(rta_parsestr[(int) $4]);
			rta_parsestr[(int) $4] = (char *) NULL;
		}
	;

value_list:
		/* empty -- optional */
	| value_list ',' value_list
	|	literal
		{	n = n_values;
			rta_cmd.updvals[n] = rta_parsestr[(int) $1];
			rta_parsestr[(int) $1] = (char *) NULL;
			n_values++;
			if (n_values > rta_cmd.ncols) {
				/* more values than columns specified */
				rta_send_error(LOC, E_BADPARSE);
        YYABORT;
			}
		}
	;


set_list:
		set_list ',' set_list
	|	NAME EQ literal
		{	n = rta_cmd.ncols;
			rta_cmd.cols[n] = rta_parsestr[(int) $1];
			rta_parsestr[(int) $1] = (char *) NULL;
			rta_cmd.updvals[n] = rta_parsestr[(int) $3];
			rta_parsestr[(int) $3] = (char *) NULL;
			rta_cmd.ncols++;
			if (rta_cmd.ncols > RTA_NCMDCOLS) {
				/* too many columns in list */
				rta_send_error(LOC, E_BADPARSE);
        YYABORT;
			}
		}
	;

literal:
		NAME
	|	STRING
	|	INTEGER
	|	REALNUM
	;

%%


/***************************************************************
 * rta_dosql_init(): - Set up data structures prior to parse of
 * an SQL command.
 *
 * Input:        None.
 * Output:       None.
 * Affects:      structure rta_cmd is initialized
 ***************************************************************/
void rta_dosql_init() {
    int   i;

    for (i=0; i<RTA_NCMDCOLS; i++) {
        if (rta_cmd.cols[i])
            free(rta_cmd.cols[i]);
        if (rta_cmd.updvals[i])
            free(rta_cmd.updvals[i]); /* values for column updates */
        if (rta_cmd.whrcols[i])
            free(rta_cmd.whrcols[i]); /* cols in where */
        if (rta_cmd.whrvals[i])
            free(rta_cmd.whrvals[i]); /* values in where clause */
        rta_cmd.cols[i]    = (char *) 0;
        rta_cmd.updvals[i] = (char *) 0;
        rta_cmd.whrcols[i] = (char *) 0;
        rta_cmd.whrvals[i] = (char *) 0;
    }
    if (rta_cmd.tbl);
        free(rta_cmd.tbl);
    for (i=0; i<MXPARSESTR; i++) {
        if (rta_parsestr[i]) {
            free(rta_parsestr[i]);
            rta_parsestr[i] = (char *) NULL;
        }
    }

    rta_cmd.tbl = (char *) 0;
    rta_cmd.ptbl = (RTA_TBLDEF *) 0;
    rta_cmd.ncols    = 0;
    rta_cmd.nwhrcols = 0;
    rta_cmd.limit  = 1<<30;  /* no real limit */
    rta_cmd.offset = 0;
    rta_cmd.err    = 0;
    n_values       = 0;      /* used in processing VALUES in insert */
}

void yyerror(char *s)
{
    rta_send_error(LOC, E_BADPARSE);
    return;
}


