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
    // Ignore the SIGPIPE signal
    signal(SIGPIPE, SIG_IGN);

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

    // Read and parse content from stdin
    char *message = malloc(MAX_CONT);
    int message_len = 0;
    int c;
    int eof_found = 0;
    while (message_len < MAX_CONT)
    {
        c = getchar();
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

    // Truncate the message to 10MB if it exceeds that length
    if (message_len >= MAX_CONT)
    {
        message[MAX_CONT - 1] = '\0';
    }
    else
    {
        message[message_len] = '\0';
    }

    // Construct the request message
    // Content-length is the byte count of the message
    char *request = malloc(MAX_CONT + MAX_HDR);
    snprintf(request, MAX_CONT + MAX_HDR, "POST message SIMPLE/1.0\r\nHost: %s\r\nContent-length: %d\r\n\r\n%s", pserver, message_len, message);

    // Send the request message to the server
    int bytes_sent = send(sockfd, request, strlen(request), 0);
    if (bytes_sent < 0)
    {
        TRACE("Error sending request: %s\n", strerror(errno));
        close_socket(sockfd);
    }

    // Receive the response message from the server
    char *response = malloc(MAX_CONT + MAX_HDR);
    int total_bytes_received = 0;
    char *response_body = NULL;
    int bytes_received = 0;
    while ((bytes_received = recv(sockfd, response + total_bytes_received, sizeof(response), 0)) > 0)
    {
        total_bytes_received += bytes_received;

        if (strstr(response, "\r\n\r\n") != NULL)
        {
            // End of the response header found
            response_body = strstr(response, "\r\n\r\n");
        }
    }

    if (total_bytes_received < 0)
    {
        TRACE("Error receiving response: %s\n", strerror(errno));
        close_socket(sockfd);
    }
    else if (total_bytes_received == 0)
    {
        TRACE("Connection closed by server.\n");
        close_socket(sockfd);
    }

    // Parse the response message
    response[total_bytes_received] = '\0';
    char *response_header = strtok(response, "\r\n");
    if (response_header == NULL)
    {
        TRACE("Malformed response: Empty header.\n");
        close_socket(sockfd);
    }

    // Does the response header contain "200 OK"?
    if (strstr(response_header, "200 OK") != NULL)
    {
        // Response is successful, write response body to stdout
        if (response_body == NULL)
        {
            TRACE("Malformed response: No body.\n");
            close_socket(sockfd);
        }
        else
        {
            response_body += 4;
            printf("%s\r\n\r\n", response_body);
        }
    }
    else
    {
        // Response is an error, write entire response header to stdout
        printf("%s\r\n\r\n", response_header);
    }

    free(response);
    free(request);
    free(message);

    // Close the socket and exit
    close_socket(sockfd);
    return 0;
}