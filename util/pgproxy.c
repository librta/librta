
/*  -includes, defines, and forward references */
#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

 /* Maximum size of a Postgres protocol packet from the UI's */
#define BUFSZ    (1000)

int   listen_on_port(int);
int   connect_to_dest(char *,
                      int);
void  printbuf(char *,
               int);

int
main(int argc,
     char *argv[])
{
  fd_set rfds;         /* bit masks for select statement */
  int   mxfd;          /* Maximum FD for the select statement */
  int   proxy_port;
  int   dest_port;
  int   proxy_srvfd;
  int   proxy_fd;
  int   dest_fd;
  int   incnt;
  int   outcnt;
  int   adrlen;        /* length of an inet socket address */
  struct sockaddr_in cliskt; /* socket to the proxy client */
  char  buf[BUFSZ];

  if ((argc != 3) ||
      (sscanf(argv[1], "%d", &proxy_port) != 1) ||
      (sscanf(argv[2], "%d", &dest_port) != 1))
  {
    fprintf(stderr, "usage: %s proxy_port dest_port\n", argv[0]);
    exit(1);
  }

  dest_fd = connect_to_port("127.0.0.1", dest_port);

  proxy_srvfd = listen_on_port(proxy_port);

  /* wait here for a connection on our proxy port */
  adrlen = sizeof(struct sockaddr_in);
  proxy_fd =
    accept(proxy_srvfd, (struct sockaddr *) &cliskt, &adrlen);
  if (proxy_fd < 0)
  {
    fprintf(stderr, "Unable to accept proxy connection\n");
    exit(1);
  }

  while (1)
  {
    FD_ZERO(&rfds);
    mxfd = 0;
    FD_SET(proxy_fd, &rfds);
    mxfd = (proxy_fd > mxfd) ? proxy_fd : mxfd;
    FD_SET(dest_fd, &rfds);
    mxfd = (dest_fd > mxfd) ? dest_fd : mxfd;

    /* Wait for some something to do */
    (void) select(mxfd + 1, &rfds, (fd_set *) 0, (fd_set *) 0,
                  (struct timeval *) 0);

    /* if data from client, send to dest */
    if (FD_ISSET(proxy_fd, &rfds))
    {
      incnt = read(proxy_fd, buf, BUFSZ);
      if (incnt < 0)
      {
        fprintf(stderr, "Error exit on error %d\n", errno);
        exit(1);
      }
      else if (incnt == 0)
      {
        fprintf(stderr, "Done.\n");
        exit(0);
      }
      outcnt = write(dest_fd, buf, incnt);
      if (outcnt != incnt)
      {
        fprintf(stderr, "Error writing to destination\n");
        exit(1);
      }
      printf("from client to server: %d\n", outcnt);
      printbuf(buf, outcnt);
    }

    /* if data from dest, send to client */
    if (FD_ISSET(dest_fd, &rfds))
    {
      incnt = read(dest_fd, buf, BUFSZ);
      if (incnt < 0)
      {
        fprintf(stderr, "Error exit on error %d\n", errno);
        exit(1);
      }
      else if (incnt == 0)
      {
        fprintf(stderr, "Done.\n");
        exit(0);
      }
      outcnt = write(proxy_fd, buf, incnt);
      if (outcnt != incnt)
      {
        fprintf(stderr, "Error writing to proxy client\n");
        exit(1);
      }
      printf("from server to client: %d\n", outcnt);
      printbuf(buf, outcnt);
    }
  }
}

int
connect_to_port(char *dest_ip,
                int dest_port)
{
  int   fd;
  struct sockaddr_in skt;
  int   adrlen;
  int   flags;

  adrlen = sizeof(struct sockaddr_in);
  (void) memset((void *) &skt, 0, (size_t) adrlen);
  skt.sin_family = AF_INET;
  skt.sin_addr.s_addr = inet_addr(dest_ip);
  skt.sin_port = htons(dest_port);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Unable to get socket to destination\n");
    exit(1);
  }
  if (connect(fd, (struct sockaddr *) &skt, adrlen) < 0)
  {
    fprintf(stderr, "Could not connect to destination\n");
    exit(1);
  }
  return (fd);
}

/*********************************************************************
 * listen_on_port(int port): -  Open a socket to listen for incoming
 *               TCP connections on the port given.  Return the file
 *               descriptor if OK, and -1 on any error.  The calling
 *               routine can handle any error condition.
 *
 * Input:        The interger value of the port number to bind to
 * Output:       The file descriptor of the socket
 * Affects:      none
 *********************************************************************/
int
listen_on_port(int port)
{
  int   srvfd;         /* FD for our listen server socket */
  struct sockaddr_in srvskt;
  int   adrlen;
  int   flags;

  adrlen = sizeof(struct sockaddr_in);
  (void) memset((void *) &srvskt, 0, (size_t) adrlen);
  srvskt.sin_family = AF_INET;
  srvskt.sin_addr.s_addr = INADDR_ANY;
  srvskt.sin_port = htons(port);
  if ((srvfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    fprintf(stderr, "Unable to get socket for port %d.", port);
    exit(1);
  }
  flags = fcntl(srvfd, F_GETFL, 0);
  /* uncomment for non-blocking: flags |= O_NONBLOCK; */
  (void) fcntl(srvfd, F_SETFL, flags);
  if (bind(srvfd, (struct sockaddr *) &srvskt, adrlen) < 0)
  {
    fprintf(stderr, "Unable to bind to port %d\n", port);
    exit(1);
  }
  if (listen(srvfd, 1) < 0)
  {
    fprintf(stderr, "Unable to listen on port %d\n", port);
    exit(1);
  }
  return (srvfd);
}

void
printbuf(char *buf,
         int cnt)
{
  int   i;
  char  strng[17];
  char  hex[49];
  int   pos;

  for (i = 0; i < cnt; i++)
  {
    pos = i % 16;
    sprintf(&hex[pos * 3], " %02x", (0x00ff & (int) buf[i]));
    strng[pos] = (isprint(buf[i])) ? buf[i] : '.';
    if ((i + 1) % 16 == 0)
    {
      strng[16] = (char) 0;
      hex[48] = (char) 0;
      printf("%s    %s\n", hex, strng);
    }
  }
  if (((i % 16) != 0) || (cnt < 16))
  {
    for (pos++; pos < 16; pos++)
    {
      sprintf(&hex[pos * 3], "   ");
      strng[pos] = ' ';
    }
    strng[16] = (char) 0;
    hex[48] = (char) 0;
    printf("%s    %s\n", hex, strng);
  }
}
