/* A trivial application to demonstrate the RTA-FS package */
/* Build with 'gcc myappfs.c -lrtafs -lrtadb' */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>             /* for 'offsetof' */
#include <string.h>             /* for 'strlen' */
#include <unistd.h>             /* for 'read/write/close' */
#include "../src/rta.h"

/* Forward references */
int reverse_str(char *tbl, char *col, char *sql, void *pr,
                int rowid, void *poldrow);


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
    (int (*)()) 0,      /* called before read */
    (int (*)()) 0,      /* called after write */
    "A sample integer in a table"
  },
  {
    "mytable",          /* the table name */
    "myfloat",          /* the column name */
    RTA_FLOAT,          /* it is a float */
    sizeof(float),      /* number of bytes */
    offsetof(struct MyData, myfloat), /* location in struct */
    0,                  /* no flags */
    (int (*)()) 0,     /* called before read */
    (int (*)()) 0,      /* called after write */
    "A sample float in a table"
  },
  {
    "mytable",          /* the table name */
    "notes",            /* the column name */
    RTA_STR,            /* it is a string */
    NOTE_LEN,           /* number of bytes */
    offsetof(struct MyData, notes), /* location in struct */
    0,                  /* no flags */
    (int (*)()) 0,      /* called before read */
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
    (int (*)()) 0,      /* called before read */
    (int (*)()) 0,      /* called after write */
    "Another sample note string in a table"
  },
};


TBLDEF mytbldef = {
    "mytable",           /* table name */
    mydata,              /* address of table */
    sizeof(struct MyData), /* length of each row */
    ROW_COUNT,           /* number of rows */
    (void *) NULL,       /* linear array; no need for an iterator */
    (void *) NULL,       /* no iterator callback data either */
    mycolumns,           /* array of column defs */
    sizeof(mycolumns) / sizeof(COLDEF),
                         /* the number of columns */
    "",                  /* no save file */
    "A sample table"
};

int main()
{
    fd_set   rfds;       /* read bit masks for select statement */
    int   i;             /* a loop counter */
    int   fsfd;          /* FD to the file system */

    /* init mydata */
    for (i=0; i<ROW_COUNT; i++) {
        mydata[i].myint    = 0;
        mydata[i].myfloat  = 0.0;
        mydata[i].notes[0] = (char) 0;
        mydata[i].seton[0] = (char) 0;
    }

    /* init the rta package and tell it about mydata */
    if (rta_add_table(&mytbldef) != RTA_SUCCESS) {
        fprintf(stderr, "Table definition error!\n");
        exit(1);
    }

    /* Mount the tables as a file system. */
    fsfd = rtafs_init("/tmp/mydir");

    /* Loop forever processing file system requests */
    while (1) {
        FD_ZERO(&rfds);
        FD_SET(fsfd, &rfds);
        (void) select(fsfd + 1, &rfds, (fd_set *) 0, (fd_set *) 0,
                (struct timeval *) 0);
        if ((fsfd >= 0) && (FD_ISSET(fsfd, &rfds)))
        {
          do_rtafs();
        }
    }
}


/* reverse_str(), a write callback to replace '<' and '>' with
 * '.', and to store the reversed string of notes into seton. */
int reverse_str(char *tbl, char *col, char *sql, void *pr,
                int rowid, void *poldrow)
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

