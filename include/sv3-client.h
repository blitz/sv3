#pragma once

#include <stdint.h>

#define SV3_QUEUE_LENGTH 4096
#define SV3_MAX_JOB_LEN  16     /* Maximum length of DMA program */

#define SV3_DESC_INVAL   0
#define SV3_DESC_RX_DONE 1
#define SV3_DESC_TX_DONE 2
#define SV3_DESC_TX_CON  3
#define SV3_DESC_TX_FIN  4

#define SV3_PACKED __attribute__((packed))

struct SV3_PACKED Sv3Desc {
  uint64_t  buf_ptr;
  uint32_t  len;
  uint16_t  type;
  uint16_t _dummy;
};

typedef struct Sv3Desc Sv3Desc;

struct SV3_PACKED Sv3Queue {
  Sv3Desc d[SV3_QUEUE_LENGTH];
  
  uint16_t head;
  uint16_t tail;

  /* For alignment */
  uint32_t _dummy;
};

typedef struct Sv3Queue Sv3Queue;

struct SV3_PACKED Sv3QueuePair {
  /* Set to true if client is blocked. */
  uint8_t  blocked;

  Sv3Queue tx;
  Sv3Queue rx;
  Sv3Queue done;

};

typedef struct Sv3QueuePair Sv3QueuePair;

static inline bool sv3_queue_is_full(struct Sv3Queue *q)
{
  return (((q->tail + 1) & (SV3_QUEUE_LENGTH - 1)) == q->head);
}

static inline bool sv3_queue_is_empty(struct Sv3Queue *q)
{
  return q->tail == q->head;
}

static inline unsigned sv3_queue_room(struct Sv3Queue *q)
{
  unsigned head = q->head;
  unsigned tail = q->tail;

  /* Head is volatile. Be sure to read it only once, as dequeuer could
     modify it concurrently. */
  asm ("" : "+m" (q->head));

  /* Keep one slot unused to differentiate empty and full cases. */
  return ((tail >= head) ? SV3_QUEUE_LENGTH : 0) - tail + head - 1;
}

static inline bool sv3_queue_enqueue(struct Sv3Queue *q, struct Sv3Desc *d)
{
  if (sv3_queue_is_full(q)) return false;

  q->d[q->tail] = *d;
  asm ("incw %0\n" : "+m" (q->tail) : "m" (q->d[q->tail]));
  return true;
}

static inline bool sv3_queue_enqueue_multi(struct Sv3Queue *q, struct Sv3Desc *d, unsigned n)
{
  unsigned tail = q->tail;
  if (sv3_queue_room(q) < n)
    return false;

  for ( ; n ; n-- , tail++ )
    q->d[tail & (SV3_QUEUE_LENGTH-1)] = *d;
  
  asm ("" ::: "memory");
  q->tail = tail;
  return true;
}

static inline bool sv3_queue_dequeue(struct Sv3Queue *q, struct Sv3Desc *d)
{
  if (sv3_queue_is_empty(q)) return false;
  
  *d = q->d[q->head];
  asm ("incw %0\n" : "+m" (q->head) : "m" (q->d[q->head]));
  return true;
}

/* EOF */
