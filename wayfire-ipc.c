#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>
#include <cJSON.h>

static int ipc_socket = -1;

void int_to_little_endian(uint32_t value, unsigned char *output)
{
    output[0] = value & 0xFF;
    output[1] = (value >> 8) & 0xFF;
    output[2] = (value >> 16) & 0xFF;
    output[3] = (value >> 24) & 0xFF;
}

int open_wayfire_ipc_connection()
{
    const char *socket_path = getenv("WAYFIRE_SOCKET");
    if (!socket_path)
    {
        fprintf(stderr, "WAYFIRE_SOCKET not set.\n");
        return -1;
    }

    struct sockaddr_un addr;

    ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ipc_socket == -1)
    {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    if (connect(ipc_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect");
        close(ipc_socket);
        ipc_socket = -1;
        return -1;
    }

    return 0;
}

char *send_wayfire_ipc(const char *json_request)
{
    if (ipc_socket == -1)
    {
        fprintf(stderr, "socket not open.\n");
        return NULL;
    }

    uint32_t json_length = strlen(json_request);
    unsigned char header[4];
    int_to_little_endian(json_length, header);

    if (write(ipc_socket, header, 4) != 4)
    {
        perror("write (header)");
        return NULL;
    }

    if (write(ipc_socket, json_request, json_length) != (ssize_t)json_length)
    {
        perror("write (json)");
        return NULL;
    }

    unsigned char response_header[4];
    if (read(ipc_socket, response_header, 4) != 4)
    {
        perror("read (response header)");
        return NULL;
    }

    uint32_t response_length = response_header[0] | (response_header[1] << 8) | (response_header[2] << 16) | (response_header[3] << 24);
    if (response_length == 0)
    {
        fprintf(stderr, "empty response\n");
        return NULL;
    }

    char *response = malloc(response_length + 1);
    if (!response)
    {
        fprintf(stderr, "memory allocation failed\n");
        return NULL;
    }

    if (read(ipc_socket, response, response_length) != (ssize_t)response_length)
    {
        perror("read (response body)");
        return NULL;
    }

    response[response_length] = '\0';
    return response;
}

void close_wayfire_ipc_connection()
{
    if (ipc_socket != -1)
    {
        close(ipc_socket);
        ipc_socket = -1;
    }
}