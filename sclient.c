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

// Function to close the socket and exit
void close_socket(int sockfd)
{
    close(sockfd);
    exit(-1);
}

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
        close_socket(sockfd);
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
        close_socket(sockfd);
    }

    // Read and parse input from stdin

    // Do not assume that the input message is always a text message.It can contain
    //     arbitrary content that includes valid “zeros” in the binary content.This means that 5 you should be careful to use string functions(e.g., strlen(), strstr()) on the input
    //         message.
    char message[MAX_CONT];
    int message_len = 0;
    int c;
    int eof_found = 0;
    while ((c = getchar()) != EOF && message_len < MAX_CONT)
    {
        if (c == EOF)
        {
            eof_found = 1;
            break;
        }
        else
        {
            message[message_len++] = c;
        }
    }

    if (message_len < 1)
    {
        // Input message is empty
        TRACE("Input message is empty.\n");
        close_socket(sockfd);
    }
    else if (message_len >= MAX_CONT)
    {
        // Input message is too long
        printf("Warning: Input message exceeds 10MB. Only the first 10MB will be sent.\n");
    }
    else if (eof_found == 0)
    {
        // Input message is a valid length, but does not end with `EOF`.
        TRACE("Input message does not end with `EOF`.\n");
        close_socket(sockfd);
    }

    // DELETE ME
    printf("Input message: %s\n", message);
}