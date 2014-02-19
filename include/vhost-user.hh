#pragma once

#include <cstdint>

enum vhost_request_type : uint32_t {
  VHOST_USER_NONE           = 0,
  VHOST_USER_GET_FEATURES   = 1,
  VHOST_USER_SET_FEATURES   = 2,
  VHOST_USER_SET_OWNER      = 3,
  VHOST_USER_RESET_OWNER    = 4,
  VHOST_USER_SET_MEM_TABLE  = 5,
  VHOST_USER_SET_LOG_BASE   = 6,
  VHOST_USER_SET_LOG_FD     = 7,
  VHOST_USER_SET_VRING_NUM  = 8,
  VHOST_USER_SET_VRING_ADDR = 9,
  VHOST_USER_SET_VRING_BASE = 10,
  VHOST_USER_GET_VRING_BASE = 11,
  VHOST_USER_SET_VRING_KICK = 12,
  VHOST_USER_SET_VRING_CALL = 13,
  VHOST_USER_SET_VRING_ERR  = 14,
};

struct vhost_vring_state {
  uint32_t index;
  uint32_t num;
};

struct vhost_vring_addr {
  uint32_t index;
  /* Option flags. */
  uint32_t flags;

  /* Start of array of descriptors (virtually contiguous) */
  uint64_t desc_user_addr;
  /* Used structure address. Must be 32 bit aligned */
  uint64_t used_user_addr;
  /* Available structure address. Must be 16 bit aligned */
  uint64_t avail_user_addr;
  /* Logging support. */
  /* Log writes to used structure, at offset calculated from specified
   * address. Address must be 32 bit aligned. */
  uint64_t log_guest_addr;
};

struct vhost_user_memory_region {
  uint64_t guest_addr;
  uint64_t size;
  uint64_t user_addr;
  // XXX Explicit padding?
} PACKED;

struct vhost_user_memory {
  uint32_t num_regions;
  uint32_t padding;

  static constexpr unsigned max_regions = 128;

  vhost_user_memory_region region[max_regions];
} PACKED;

struct vhost_request_hdr {
  vhost_request_type request;
  uint32_t flags;
  uint32_t size;
} PACKED;
  
struct vhost_request {
  vhost_request_hdr hdr;

  union {
    uint64_t          u64;
    vhost_vring_state state;
    vhost_vring_addr  addr;
    vhost_user_memory memory;
  };

  /// This structure conforms to version 1.
  static constexpr unsigned current_version = 1;

  /// Returns the protocol version.
  unsigned version() const { return hdr.flags & 0x3; }
} PACKED;

// EOF
