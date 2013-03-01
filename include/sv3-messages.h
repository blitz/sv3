
#pragma once

#include <iso646.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>


/* Request types */
#define SV3_REQ_PING             0
#define SV3_REQ_CREATE_PORT_TAP  1
#define SV3_REQ_MEMORY_MAP       2
#define SV3_REQ_CREATE_PORT_QP   3
#define SV3_REQ_EVENT_FD         4

struct Sv3Request {
  uint8_t type;

  union {
    struct {
      // nothing...
    } ping;
    struct {
      char buf[32];
    } create_port_tap;
    struct {
      int      fd;

      uint64_t addr;
      uint64_t size;
      off_t    offset;
    } memory_map;
    struct {
      uint64_t qp;            // pointer
    } create_port_qp;
    struct {
      // fd needs to be at same offset as in memory_map
      int fd;
    } event_fd;
  };
};

/* Response types */
#define SV3_RESP_STATUS 0

struct Sv3Response {
  uint8_t type;
  union {
    struct {
      bool success;
    } status;
  };
};

typedef struct Sv3Request  Sv3Request;
typedef struct Sv3Response Sv3Response;

/* Returns zero on success. On failure, errno is set. */
static inline int sv3_call(int fd, Sv3Request *req, Sv3Response *resp)
{
  struct msghdr  hdr;
  struct iovec   iov = { req, sizeof(*req) };
  union {
    struct cmsghdr chdr;
    char           chdr_data[CMSG_SPACE(sizeof(int))];
  } u;

  hdr.msg_name    = NULL;
  hdr.msg_namelen = 0;
  hdr.msg_iov     = &iov;
  hdr.msg_iovlen  = 1;
  hdr.msg_flags   = 0;
    
  if (req->type == SV3_REQ_MEMORY_MAP ||
      req->type == SV3_REQ_EVENT_FD) {
    // Pass file descriptor
    hdr.msg_control    = &u.chdr;
    hdr.msg_controllen = CMSG_LEN(sizeof(int));
    u.chdr.cmsg_len    = CMSG_LEN(sizeof(int));
    u.chdr.cmsg_level  = SOL_SOCKET;
    u.chdr.cmsg_type   = SCM_RIGHTS;

    assert ( &req->memory_map.fd == &req->event_fd.fd );
    *(int *)(CMSG_DATA(&u.chdr)) = req->memory_map.fd;
  } else {
    // No file descriptor to pass
    hdr.msg_control    = NULL;
    hdr.msg_controllen = 0;
  }

  int res = sendmsg(fd, &hdr, MSG_EOR | MSG_NOSIGNAL);
  if (res != sizeof(*req))
    return -1;

    
  res = recv(fd, resp, sizeof(*resp), 0);
  if (res != sizeof(*resp))
    return -1;

  return 0;
}

/* Allocate an anonymous file with the specified length. Returns a
   file descriptor on success, -1 on failure. */
static inline int sv3_memory(size_t len)
{
  char templ[] = "/tmp/sv3-shmem-XXXXXX";
  int tfd = mkstemp(templ);
  if (tfd < 0)           return tfd;
  if (0 > unlink(templ)) return -1;

  if (0 != ftruncate(tfd, len))
    return -1;

  return tfd;

}

/* EOF */
