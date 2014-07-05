/***************************************************************
 * Run Time Access Library
 * Copyright (C) 2003-2014 Robert W Smith (bsmith@linuxtoys.org)
 *
 *  This program is distributed under the terms of the GNU LGPL.
 *  See the file COPYING file.
 **************************************************************/

/*
 *  libpq sample program
 *  gcc librta_client.c -o librta_client -lpq
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>    /* libpq header file */

char cmd1[] ="UPDATE mytable SET myint=43";
char cmd2[] ="SELECT myint, myfloat, notes FROM mytable";
 
int
main()
{
    PGconn     *conn;               /* holds database connection */
    PGresult   *res;                /* holds query result */
    int         i;                  /* generic loop counter */

    /* Connect to the application */
    conn = PQconnectdb("host=localhost port=8888");
    if (PQstatus(conn) == CONNECTION_BAD)        {
        fprintf(stderr, "Connection to application failed.\n");
        fprintf(stderr, "%s", PQerrorMessage(conn));
        exit(1);
    }

    /* send the first command */ 
    res = PQexec(conn, cmd1);   /* send the query */
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "UPDATE command failed.\n");
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }
    PQclear(res);    /* free result */

    /* send the second command */
    res = PQexec(conn, cmd2);   /* send the query */
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT query failed.\n");
        PQclear(res);
        PQfinish(conn);
        exit(1);
    }

    /* display the results of the second command */ 
    printf("\n");
    for (i = 0; i < PQntuples(res); i++) {
        /* loop through all rows returned */
        printf("%s\t", PQgetvalue(res, i, 0));
        printf("%s\t", PQgetvalue(res, i, 1));
        printf("%s", PQgetvalue(res, i, 2));
        printf("\n");
    }

    PQclear(res);    /* free result */
    PQfinish(conn);  /* disconnect from the database */

    exit(0);
}
