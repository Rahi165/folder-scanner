#include "indexer/ipc_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

// read_exact()
// Helper function that tries to read exactly `nbytes` from a file descriptor. 
// It keeps looping until:
// - we hit EOF, OR
// - we get a real error.

// Returns:
// >0  → number of bytes read (should equal nbytes)
// 0  → connection closed
// -1  → error //

static ssize_t read_exact(int fd, void *buf, size_t nbytes)
{
    size_t total = 0;

    while (total < nbytes) {
        ssize_t r = read(fd, (char *)buf + total, nbytes - total);

        if (r < 0) {
            if (errno == EINTR) {
                continue;   // interrupted by signal, retry
            }
            return -1;      // real error
        }

        if (r == 0) {
            return 0;       // remote side closed connection
        }

        total += (size_t)r;
    }

    return (ssize_t)total;
}

// ipc_server_init()
// Sets up the UNIX domain socket the indexer listens on.
// Steps:
// 1. Clear out any old socket file (in case last run crashed)
// 2. Create socket()
// 3. Bind to the given path
// 4. Listen for incoming connections
// If anything fails, we print an error and return -1. //

int ipc_server_init(ipc_server_t *srv, const char *sockpath)
{
    srv->server_fd = -1;
    srv->client_fd = -1;

    // Save the socket path so we can unlink it later
    strncpy(srv->socket_path, sockpath, IPC_SOCKET_MAX_PATH);
    srv->socket_path[IPC_SOCKET_MAX_PATH] = '\0';

    // Remove leftover socket file from previous runs
    unlink(sockpath);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("ipc_server: socket");
        return -1;
    }

    // SO_REUSEADDR doesn't really matter for UNIX sockets,
    // but it doesn't hurt to set it.
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    // Copy the path into the sockaddr
    strncpy(addr.sun_path, sockpath, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("ipc_server: bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        perror("ipc_server: listen");
        close(fd);
        return -1;
    }

    srv->server_fd = fd;
    return 0;
}

// ipc_server_accept()
// Blocks until the crawler connects. Once accept() succeeds, we store the connected socket in client_fd. //

int ipc_server_accept(ipc_server_t *srv)
{
    srv->client_fd = accept(srv->server_fd, NULL, NULL);
    if (srv->client_fd < 0) {
        perror("ipc_server: accept");
        return -1;
    }
    return 0;
}

// ipc_server_recv()
// Reads one full message from the crawler. The message format is defined in ipc_proto.h, and includes:
// - header (docid, depth, lengths)
// - URL string
// - path string
// Also checks for the sentinel docid, which means the crawler is finished sending work.

// Returns:
// 1 → normal message
// 0 → sentinel (crawler done)
// -1 → error or unexpected disconnect //

int ipc_server_recv(ipc_server_t *srv,
                    uint32_t *docid, uint16_t *depth,
                    char *url_buf,  size_t url_buf_sz,
                    char *path_buf, size_t path_buf_sz)
{
    ipc_msg_header_t hdr;

    // First read the fixed-size header
    ssize_t r = read_exact(srv->client_fd, &hdr, sizeof(hdr));
    if (r <= 0) {
        return -1; // error or disconnect
    }

    // Sentinel means "no more documents"
    if (hdr.docid == IPC_SENTINEL_DOCID) {
        return 0;
    }

    *docid = hdr.docid;
    *depth = hdr.depth;

    // Read the URL string.
    // Only copy as much as fits in url_buf, and discard the rest.
    // Prevents buffer overflow if the crawler sends a long URL. //

    size_t url_copy = (hdr.url_len < url_buf_sz) ? hdr.url_len : url_buf_sz - 1;

    if (url_copy > 0) {
        if (read_exact(srv->client_fd, url_buf, url_copy) <= 0) {
            return -1;
        }
    }

    // Throw away extra bytes if URL was too long
    if (hdr.url_len > url_copy) {
        size_t leftover = hdr.url_len - url_copy;
        char dump[64];

        while (leftover > 0) {
            size_t chunk = leftover < sizeof(dump) ? leftover : sizeof(dump);
            if (read_exact(srv->client_fd, dump, chunk) <= 0) {
                return -1;
            }
            leftover -= chunk;
        }
    }

    url_buf[url_copy] = '\0';

    // Same logic for the file path string. //

    size_t path_copy = (hdr.path_len < path_buf_sz) ? hdr.path_len : path_buf_sz - 1;

    if (path_copy > 0) {
        if (read_exact(srv->client_fd, path_buf, path_copy) <= 0) {
            return -1;
        }
    }

    if (hdr.path_len > path_copy) {
        size_t leftover = hdr.path_len - path_copy;
        char dump[64];

        while (leftover > 0) {
            size_t chunk = leftover < sizeof(dump) ? leftover : sizeof(dump);
            if (read_exact(srv->client_fd, dump, chunk) <= 0) {
                return -1;
            }
            leftover -= chunk;
        }
    }

    path_buf[path_copy] = '\0';

    return 1;
}

// ipc_server_close()
// Cleans up both the listening socket and the accepted connection.
// Also removes the socket file from the filesystem so the next run doesn't complain about an existing socket. //

void ipc_server_close(ipc_server_t *srv)
{
    if (srv->client_fd >= 0) {
        close(srv->client_fd);
        srv->client_fd = -1;
    }

    if (srv->server_fd >= 0) {
        close(srv->server_fd);
        srv->server_fd = -1;
    }

    if (srv->socket_path[0] != '\0') {
        unlink(srv->socket_path);
        srv->socket_path[0] = '\0';
    }
}
