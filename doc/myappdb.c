/* A trivial application to demonstrate the RTA-DB package */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>             /* for 'offsetof' */
#include <string.h>             /* for 'strlen' */
#include <unistd.h>             /* for 'read/write/close' */
#include <sys/socket.h>
#include <netinet/in.h>
#include "../src/rta.h"

/* Forward references */
void reverse_str(char *tbl, char *col, char *sql, int rowid);

#define INSZ       500
#define OUTSZ    50000

#define NOTE_LEN   30
struct MyData {
    int    myint;
    float  myfloat;
    char   notes[NOTE_LEN];
    char   seton[NOTE_LEN];
};

#define ROW_COUNT  20
struct MyData mydata[ROW_COUNT];


COLDEF mycolumns[] = {
  {
    "mytable",          /* the table name */
    "myint",            /* the column name */
    RTA_INT,            /* it is an integer */
    sizeof(int),        /* number of bytes */
    offsetof(struct MyData, myint), /* location in struct */
    0,                  /* no flags */
    (void (*)()) 0,     /* called before read */
    (void (*)()) 0,     /* called after write */
    "A sample integer in a table"
  },
  {
    "mytable",          /* the table name */
    "myfloat",          /* the column name */
    RTA_FLOAT,          /* it is a float */
    sizeof(float),      /* number of bytes */
    offsetof(struct MyData, myfloat), /* location in struct */
    0,                  /* no flags */
    (void (*)()) 0,     /* called before read */
    (void (*)()) 0,     /* called after write */
    "A sample float in a table"
  },
  {
    "mytable",          /* the table name */
    "notes",            /* the column name */
    RTA_STR,            /* it is a string */
    NOTE_LEN,           /* number of bytes */
    offsetof(struct MyData, notes), /* location in struct */
    0,                  /* no flags */
    (void (*)()) 0,     /* called before read */
    reverse_str,        /* called after write */
    "A sample note string in a table"
  },
  {
    "mytable",          /* the table name */
    "seton",            /* the column name */
    RTA_STR,            /* it is a string */
    NOTE_LEN,           /* number of bytes */
    offsetof(struct MyData, seton), /* location in struct */
    RTA_READONLY,       /* a read-only column */
    (void (*)()) 0,     /* called before read */
    (void (*)()) 0,     /* called after write */
    "Another sample note string in a table"
  },
};


TBLDEF mytbldef = {
    "mytable",           /* table name */
    mydata,              /* address of table */
    sizeof(struct MyData), /* length of each row */
    ROW_COUNT,           /* number of rows */
    mycolumns,           /* array of column defs */
    sizeof(mycolumns) / sizeof(COLDEF),
                         /* the number of columns */
    "",                  /* no save file */
    "A sample table"
};

int main()
{
    int   i;                   /* a loop counter */
    int   srvfd;               /* FD for our server socket */
    int   connfd;              /* FD for conn to client */
    struct sockaddr_in srvskt; /* server listen socket */
    struct sockaddr_in cliskt; /* socket to the UI/DB client */
    int   adrlen;
    char  inbuf[INSZ];         /* Buffer for incoming SQL commands */
    char  outbuf[OUTSZ];       /* response back to the client */
    int   incnt;               /* SQL command input count */
    int   outcnt;              /* SQL command output count */
    int   dbret;               /* return value from SQL command */

    /* init mydata */
    for (i=0; i<ROW_COUNT; i++) {
        mydata[i].myint    = 0;
        mydata[i].myfloat  = 0.0;
        mydata[i].notes[0] = (char) 0;
        mydata[i].seton[0] = (char) 0;
    }

    /* init the rta package and tell it about mydata */
    rta_init();
    if (rta_add_table(&mytbldef) != RTA_SUCCESS) {
        fprintf(stderr, "Table definition error!\n");
        exit(1);
    }

    /* By-the-way: the following code is pretty horrendous.
     * It uses blocking IO, ignores error conditions, and
     * makes wildly optimistic assumptions about socket IO.
     * My goal is to make the code understandable by getting
     * it into as few lines as possible. */

    /* We now need to open a socket to listen for incoming
     * client connections. */
    adrlen = sizeof (struct sockaddr_in);
    (void) memset ((void *) &srvskt, 0, (size_t) adrlen);
    srvskt.sin_family = AF_INET;
    srvskt.sin_addr.s_addr = INADDR_ANY;
    srvskt.sin_port = htons (8888);
    srvfd = socket(AF_INET, SOCK_STREAM, 0); /* no error checks! */
    bind(srvfd, (struct sockaddr *) &srvskt, adrlen);
    listen (srvfd, 4);

    /* Loop forever accepting client connections */
    while (1) {
        connfd = accept(srvfd, (struct sockaddr *) &cliskt, &adrlen);
        if (connfd < 0) {
            fprintf(stderr, "Error on socket/bind/listen/accept\n");
            exit(1);
        }
        incnt = 0;
        while (connfd >= 0) {
            incnt = read(connfd, &inbuf[incnt], INSZ-incnt);
            if (incnt <= 0) {
                close(connfd);
                connfd = -1;
            }
            outcnt = OUTSZ;
            dbret = dbcommand(inbuf, &incnt, outbuf, &outcnt);
            switch (dbret) {
                case RTA_SUCCESS:
                    write(connfd, outbuf, (OUTSZ - outcnt));
                    incnt = 0;
                    break;
                case RTA_NOCMD:
                    break;
                case RTA_CLOSE:
                    close(connfd);
                    connfd = -1;
                    break;
            }
        }
    }
}


/* reverse_str(), a write callback to replace '<' and '>' with
 * '.', and to store the reversed string of notes into seton. */
void reverse_str(char *tbl, char *col, char *sql, int rowid)
{
    int   i,j;                 /* loop counters */

    i = strlen(mydata[rowid].notes) -1;  /* -1 to ignore NULL */
    for(j=0 ; i>=0; i--,j++) {
        if (mydata[rowid].notes[i] == '<' ||
            mydata[rowid].notes[i] == '>')
            mydata[rowid].notes[i] = '.';
        mydata[rowid].seton[j] = mydata[rowid].notes[i];
    }
    mydata[rowid].seton[j] = (char) 0;
}

