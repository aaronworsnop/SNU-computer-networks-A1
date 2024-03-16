#include <stdio.h>
#include <sys/types.h> /* See NOTES */
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include "macro.h"
/*--------------------------------------------------------------------------------*/
int main(const int argc, const char **argv)
{
    const char *pserver = NULL;
    int port = -1;
    int i;

    /* argument processing */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 && (i + 1) < argc)
        {
            port = atoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-s") == 0 && (i + 1) < argc)
        {
            pserver = argv[i + 1];
            i++;
        }
    }

    /* check arguments */
    if (port < 0 || pserver == NULL)
    {
        printf("usage: %s -p port -s server-ip\n", argv[0]);
        exit(-1);
    }
    if (port < 1024 || port > 65535)
    {
        printf("port number should be between 1024 ~ 65535.\n");
        exit(-1);
    }

    // Request format
    // POST message SIMPLE/1.0
    // Host: [s-server-domain-name]
    // Content-length: [byte-count]
    // (empty line)
    // [message-content]

    // Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        TRACE("Socket creation failed: %s\n", strerror(errno));
        exit(-1);
    }

    // Get the IP address of the server
    struct hostent *host = gethostbyname(pserver);
    if (host == NULL)
    {
        TRACE("Unknown host: %s\n", pserver);
        close(sockfd);
        exit(-1);
    }

    // Fill in the server's address
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    memcpy(&saddr.sin_addr, host->h_addr, host->h_length);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        TRACE("Connection failed: %s\n", strerror(errno));
        close(sockfd);
        exit(-1);
    }
}