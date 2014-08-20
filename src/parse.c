#ifndef lint
static char const 
yyrcsid[] = "$FreeBSD: src/usr.bin/yacc/skeleton.c,v 1.28 2000/01/17 02:04:06 bde Exp $";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
static int yygrowstack();
#define YYPREFIX "yy"
#line 14 "parse.y"
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
#line 43 "parse.c"
#define YYERRCODE 256
#define SELECT 257
#define UPDATE 258
#define FROM 259
#define WHERE 260
#define NAME 261
#define STRING 262
#define INTEGER 263
#define REALNUM 264
#define LIMIT 265
#define OFFSET 266
#define SET 267
#define EQ 268
#define NE 269
#define GT 270
#define LT 271
#define GE 272
#define LE 273
#define AND 274
const short yylhs[] = {                                        -1,
    0,    0,    0,    1,    1,    3,    3,    4,    4,    5,
    5,    7,    7,    7,    8,    8,    8,    8,    8,    8,
    6,    6,    6,    2,    2,   10,   10,    9,    9,    9,
    9,
};
const short yylen[] = {                                         2,
    0,    1,    1,    7,    6,    1,    3,    1,    3,    0,
    2,    3,    3,    3,    1,    1,    1,    1,    1,    1,
    0,    2,    4,    7,    6,    3,    3,    1,    1,    1,
    1,
};
const short yydefred[] = {                                      0,
    0,    0,    0,    2,    3,    6,    0,    0,    0,    0,
    0,    0,    0,    7,    0,    0,    0,    0,    0,    0,
    0,    0,    9,    0,    0,    0,    0,    0,   28,   29,
   30,   31,   27,   26,    0,   15,   16,   17,   18,   19,
   20,    0,    0,    0,    0,    4,   24,   14,   12,   13,
    0,   23,
};
const short yydgoto[] = {                                       3,
    4,    5,    7,   13,   19,   28,   26,   42,   33,   16,
};
const short yysindex[] = {                                   -237,
 -239, -238,    0,    0,    0,    0,  -37, -243, -236, -235,
 -234,  -17, -230,    0, -233,  -39, -229,  -36, -232, -245,
 -234, -232,    0, -258,  -36, -240, -227,  -28,    0,    0,
    0,    0,    0,    0,  -22,    0,    0,    0,    0,    0,
    0, -245,  -41,  -36, -228,    0,    0,    0,    0,    0,
 -224,    0,
};
const short yyrindex[] = {                                     40,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    1,    2,    0,    0,    2,    0,    0,    6,    0,
    0,    6,    0,    0,    0,    3,    0,   41,    0,    0,
    0,    0,    0,    0,   42,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    8,    0,    0,    0,    0,    0,
    0,    0,
};
const short yygindex[] = {                                      0,
    0,    0,    0,    0,   27,   22,  -16,    0,    4,   24,
};
#define YYTABLESIZE 268
const short yytable[] = {                                      49,
    8,   10,   11,   25,   21,   21,   10,   22,   43,   36,
   37,   38,   39,   40,   41,   29,   30,   31,   32,    1,
    2,    6,    8,   11,   12,   14,   15,   50,   17,   18,
   46,   23,   27,   44,   20,   45,   47,   51,   52,    1,
    5,   25,   22,   35,   34,   48,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    8,
   10,   11,    0,    0,   21,    0,   22,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   18,    9,    0,    0,   24,    0,    0,    0,    0,    0,
    0,    0,   44,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    8,    0,    0,    0,    0,    8,   10,   11,
};
const short yycheck[] = {                                      41,
    0,    0,    0,   40,   44,    0,   44,    0,   25,  268,
  269,  270,  271,  272,  273,  261,  262,  263,  264,  257,
  258,  261,  261,  267,  261,  261,  261,   44,   46,  260,
   59,  261,  265,  274,  268,  263,   59,  266,  263,    0,
    0,    0,   16,   22,   21,   42,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   59,
   59,   59,   -1,   -1,   59,   -1,   59,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  260,  259,   -1,   -1,  261,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,  274,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  260,   -1,   -1,   -1,   -1,  265,  265,  265,
};
#define YYFINAL 3
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 274
#if YYDEBUG
const char * const yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,"'('","')'",0,0,"','",0,"'.'",0,0,0,0,0,0,0,0,0,0,0,0,"';'",0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"SELECT",
"UPDATE","FROM","WHERE","NAME","STRING","INTEGER","REALNUM","LIMIT","OFFSET",
"SET","EQ","NE","GT","LT","GE","LE","AND",
};
const char * const yyrule[] = {
"$accept : command",
"command :",
"command : select_statement",
"command : update_statement",
"select_statement : SELECT column_list FROM table_name where_clause limit_clause ';'",
"select_statement : SELECT column_list FROM table_name where_clause limit_clause",
"column_list : NAME",
"column_list : column_list ',' NAME",
"table_name : NAME",
"table_name : NAME '.' NAME",
"where_clause :",
"where_clause : WHERE test_condition",
"test_condition : '(' test_condition ')'",
"test_condition : test_condition AND test_condition",
"test_condition : NAME relation literal",
"relation : EQ",
"relation : NE",
"relation : GT",
"relation : LT",
"relation : GE",
"relation : LE",
"limit_clause :",
"limit_clause : LIMIT INTEGER",
"limit_clause : LIMIT INTEGER OFFSET INTEGER",
"update_statement : UPDATE NAME SET set_list where_clause limit_clause ';'",
"update_statement : UPDATE NAME SET set_list where_clause limit_clause",
"set_list : set_list ',' set_list",
"set_list : NAME EQ literal",
"literal : NAME",
"literal : STRING",
"literal : INTEGER",
"literal : REALNUM",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#if YYDEBUG
#include <stdio.h>
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
#line 208 "parse.y"


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


#line 308 "parse.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss);
    if (newss == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs);
    if (newvs == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab

#ifndef YYPARSE_PARAM
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG void
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif	/* ANSI-C/C++ */
#else	/* YYPARSE_PARAM */
#ifndef YYPARSE_PARAM_TYPE
#define YYPARSE_PARAM_TYPE void *
#endif
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;
#endif	/* ANSI-C/C++ */
#endif	/* ! YYPARSE_PARAM */

int
yyparse (YYPARSE_PARAM_ARG)
    YYPARSE_PARAM_DECL
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate])) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 68 "parse.y"
{ YYABORT; }
break;
case 4:
#line 76 "parse.y"
{	cmd.command = RTA_SELECT;
			YYACCEPT;
		}
break;
case 5:
#line 80 "parse.y"
{	cmd.command = RTA_SELECT;
			YYACCEPT;
		}
break;
case 6:
#line 87 "parse.y"
{	cmd.cols[cmd.ncols] = parsestr[(int) yyvsp[0]];
			parsestr[(int) yyvsp[0]] = (char *) NULL;
			cmd.ncols++;
		}
break;
case 7:
#line 92 "parse.y"
{	cmd.cols[cmd.ncols] = parsestr[(int) yyvsp[0]];
			parsestr[(int) yyvsp[0]] = (char *) NULL;
			cmd.ncols++;
			if (cmd.ncols > NCMDCOLS) {
				/* too many columns in list */
				send_error(LOC, E_BADPARSE);
			}
		}
break;
case 8:
#line 104 "parse.y"
{
			cmd.tbl = parsestr[(int) yyvsp[0]];
			parsestr[(int) yyvsp[0]] = (char *) NULL;
		}
break;
case 9:
#line 109 "parse.y"
{
			cmd.tbl = parsestr[(int) yyvsp[0]];
			parsestr[(int) yyvsp[0]] = (char *) NULL;
		}
break;
case 14:
#line 126 "parse.y"
{	n = cmd.nwhrcols;
			cmd.whrcols[n] = parsestr[(int) yyvsp[-2]];
			parsestr[(int) yyvsp[-2]] = (char *) NULL;
			cmd.whrrel[n] = whrrelat;
			cmd.whrvals[n] = parsestr[(int) yyvsp[0]];
			parsestr[(int) yyvsp[0]] = (char *) NULL;
			cmd.nwhrcols++;
			if (cmd.nwhrcols > NCMDCOLS) {
				/* too many columns in list */
				send_error(LOC, E_BADPARSE);
			}
		}
break;
case 15:
#line 142 "parse.y"
{	whrrelat = RTA_EQ; }
break;
case 16:
#line 143 "parse.y"
{	whrrelat = RTA_NE; }
break;
case 17:
#line 144 "parse.y"
{	whrrelat = RTA_GT; }
break;
case 18:
#line 145 "parse.y"
{	whrrelat = RTA_LT; }
break;
case 19:
#line 146 "parse.y"
{	whrrelat = RTA_GE; }
break;
case 20:
#line 147 "parse.y"
{	whrrelat = RTA_LE; }
break;
case 22:
#line 154 "parse.y"
{	cmd.limit  = atoi(parsestr[(int) yyvsp[0]]);
            free(parsestr[(int) yyvsp[0]]);
			parsestr[(int) yyvsp[0]] = (char *) NULL;
		}
break;
case 23:
#line 159 "parse.y"
{	cmd.limit  = atoi(parsestr[(int) yyvsp[-2]]);
            free(parsestr[(int) yyvsp[-2]]);
			parsestr[(int) yyvsp[-2]] = (char *) NULL;
			cmd.offset = atoi(parsestr[(int) yyvsp[0]]);
            free(parsestr[(int) yyvsp[0]]);
			parsestr[(int) yyvsp[0]] = (char *) NULL;
		}
break;
case 24:
#line 171 "parse.y"
{	cmd.command = RTA_UPDATE;
			cmd.tbl     = parsestr[(int) yyvsp[-5]];
			parsestr[(int) yyvsp[-5]] = (char *) NULL;
		}
break;
case 25:
#line 176 "parse.y"
{	cmd.command = RTA_UPDATE;
			cmd.tbl     = parsestr[(int) yyvsp[-4]];
			parsestr[(int) yyvsp[-4]] = (char *) NULL;
		}
break;
case 27:
#line 185 "parse.y"
{	n = cmd.ncols;
			cmd.cols[n] = parsestr[(int) yyvsp[-2]];
			parsestr[(int) yyvsp[-2]] = (char *) NULL;
			cmd.updvals[n] = parsestr[(int) yyvsp[0]];
			parsestr[(int) yyvsp[0]] = (char *) NULL;
			cmd.ncols++;
			if (cmd.ncols > NCMDCOLS) {
				/* too many columns in list */
				send_error(LOC, E_BADPARSE);
			}
		}
break;
#line 635 "parse.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
