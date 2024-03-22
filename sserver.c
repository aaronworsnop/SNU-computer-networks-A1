#include <stdio.h>
#include <errno.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

#include "macro.h"

// Function to close the socket and exit
void close_socket(int sockfd)
{
    close(sockfd);
    exit(-1);
}

// Function to handle client connections
void handle_client(int client_sock)
{
    // Receive the request message from the client
    char *header = malloc(MAX_CONT + MAX_HDR);
    int bytes_received = 0;
    int total_bytes_received = 0;
    char *request_body = NULL;

    while ((bytes_received = recv(client_sock, header + total_bytes_received, sizeof(header), 0)) > 0)
    {
        total_bytes_received += bytes_received;

        // Check if the entire header has been received
        if (strstr(header, "\r\n\r\n") != NULL)
        {
            // End of the request header found
            request_body = strstr(header, "\r\n\r\n");
            break;
        }
    }

    if (total_bytes_received < 0)
    {
        TRACE("Error receiving request: %s\r\n", strerror(errno));
        close_socket(client_sock);
    }
    else if (total_bytes_received == 0)
    {
        TRACE("Client closed connection.\n");
        close_socket(client_sock);
    }

    // Parse the request message
    header[total_bytes_received] = '\0';
    char *request_header = strstr(header, "Host");
    if (request_header == NULL)
    {
        TRACE("Malformed request: Empty header.\r\n");
        close_socket(client_sock);
    }

    // Check if the request header is well-formed
    const char *error_response = "SIMPLE/1.0 400 Bad Request\r\n\r\n";

    // Does the request header contain "POST message SIMPLE/1.0"?
    if (strstr(header, "POST message SIMPLE/1.0") == NULL)
    {
        // Respond with an error response for incorrect protocol
        send(client_sock, error_response, strlen(error_response), 0);
        TRACE("Malformed request.\r\n");
    }

    // Convert the request header to lowercase and remove all whitespaces
    for (int i = 0; i < strlen(header); i++)
    {
        if (&header == &request_body)
        {
            break;
        }

        header[i] = tolower(header[i]);
        if (header[i] == ' ')
        {
            // Remove whitespaces
            for (int j = i; j < strlen(header); j++)
            {
                header[j] = header[j + 1];
            }
            i--;
        }
    }

    // Does the request header contain "host"?
    char *host = strstr(header, "host:");
    if (host == NULL)
    {
        // Respond with an error response
        send(client_sock, error_response, strlen(error_response), 0);
        TRACE("Malformed Request.\r\n");
    }

    // Is there content after "host:"?
    if (host[5] == '\0' || host[5] == '\r' || host[5] == '\n')
    {
        // Respond with an error response
        send(client_sock, error_response, strlen(error_response), 0);
        TRACE("Malformed Request. No host name.\r\n");
    }

    // Does the request header contain "content-length"?
    char *content_length = strstr(header, "content-length:");
    if (content_length == NULL)
    {
        // Respond with an error response
        send(client_sock, error_response, strlen(error_response), 0);
    }

    // Is there content after "content-length:"?
    if (content_length[15] == '\0' || content_length[15] == '\r' || content_length[15] == '\n')
    {
        // Respond with an error response
        send(client_sock, error_response, strlen(error_response), 0);
        TRACE("Malformed Request. Content length not provided.\r\n");
    }

    // Extract the content length
    int content_len = atoi(content_length + 15);

    // Is the content length greater than the maximum content length?
    if (content_len > MAX_CONT)
    {
        // Respond with an error response
        send(client_sock, error_response, strlen(error_response), 0);
        TRACE("Content length exceeds maximum.\r\n");
    }

    // Capture what was left of the body from the header
    char *body = malloc(content_len);
    int content_len_offset = 0;
    request_body += 4;

    while (*request_body != '\0')
    {
        body[content_len_offset++] = *request_body++;
    }

    // Receive the rest of the request body
    while (content_len_offset < content_len)
    {
        bytes_received = recv(client_sock, body + content_len_offset, content_len - content_len_offset, 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                // Connection closed
                break;
            }
            else
            {
                // Error receiving data
                perror("recv error");
                break;
            }
        }

        content_len_offset += bytes_received;
    }

    // Check if the received content length matches the expected length
    if (content_len_offset != content_len)
    {
        send(client_sock, error_response, strlen(error_response), 0);
        TRACE("Malformed Request. Incorrect content length.\r\n");
    }

    // Construct the response message
    char *response = malloc(MAX_CONT + MAX_HDR);
    snprintf(response, MAX_CONT + MAX_HDR, "SIMPLE/1.0 200 OK\r\nContent-length: %d\r\n\r\n%s", content_len, body);

    // Send the response message to the client
    int bytes_sent = send(client_sock, response, strlen(response), 0);
    if (bytes_sent < 0)
    {
        TRACE("Error sending response: %s\n", strerror(errno));
        close_socket(client_sock);
    }

    // Close the connection
    close(client_sock);
}

/*--------------------------------------------------------------------------------*/
int main(const int argc, const char **argv)
{
    int i;
    int port = -1;

    /* argument parsing */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 && (i + 1) < argc)
        {
            port = atoi(argv[i + 1]);
            i++;
        }
    }
    if (port <= 0 || port > 65535)
    {
        printf("usage: %s -p port\n", argv[0]);
        exit(-1);
    }

    // Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        TRACE("Socket creation failed: %s\n", strerror(errno));
        exit(-1);
    }

    // Build address
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(port);

    // Bind the socket to the server's IP address and port
    if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        TRACE("Bind failed: %s\n", strerror(errno));
        close_socket(sockfd);
    }

    // Listen for incoming connections
    if (listen(sockfd, 5) < 0)
    {
        TRACE("Listen failed: %s\n", strerror(errno));
        close_socket(sockfd);
    }

    // Accept incoming connections and handle them
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0)
        {
            TRACE("Accept failed: %s\n", strerror(errno));
            close(sockfd);
            exit(-1);
        }

        // Fork a new process to handle the client connection
        pid_t pid = fork();
        if (pid < 0)
        {
            TRACE("Fork failed: %s\n", strerror(errno));
            close(client_sock);
            continue;
        }
        else if (pid == 0)
        {
            // Child process
            close(sockfd);
            handle_client(client_sock);
            exit(0);
        }
        else
        {
            // Parent process
            close(client_sock);
        }
    }

    return 0;
}
