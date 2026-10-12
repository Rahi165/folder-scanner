#ifndef IPC_PROTO_H
#define IPC_PROTO_H

/*
 * common/ipc_proto.h — Shared IPC wire protocol between crawler and indexer.
 *
 * Wire format per message:
 *   [ipc_msg_header_t: 10 bytes, packed]
 *     uint32_t docid
 *     uint16_t depth
 *     uint16_t url_len
 *     uint16_t path_len
 *   [url_len  bytes: URL string, no null terminator]
 *   [path_len bytes: file path string, no null terminator]
 *
 * Sentinel: header with docid == IPC_SENTINEL_DOCID, all lengths == 0.
 * All multi-byte fields are in host byte order (same machine, both processes).
 */

#include <stdint.h>

/* Maximum UNIX socket path length (macOS: 104 bytes, Linux: 108 bytes).
 * We use 103 to leave room for the null terminator on both platforms. */
#define IPC_SOCKET_MAX_PATH 103

/* Sentinel docid value — signals end-of-crawl to the indexer. */
#define IPC_SENTINEL_DOCID  UINT32_MAX

/* Packed binary header sent before each message body. */
#pragma pack(push, 1)
typedef struct {
    uint32_t docid;     /* document ID assigned by the crawler          */
    uint16_t depth;     /* crawl depth at which this page was fetched   */
    uint16_t url_len;   /* length of the URL string (no null byte)      */
    uint16_t path_len;  /* length of the file path string (no null byte)*/
} ipc_msg_header_t;
#pragma pack(pop)

#endif /* IPC_PROTO_H */
