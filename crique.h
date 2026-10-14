#ifndef CRIQUE_H
#define CRIQUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRIQUE_CHUNK_OVERHEAD (sizeof(crique_chunk_t))
#define CRIQUE_MIN_CHUNK_LEN  (1)
#define CRIQUE_MIN_CHUNK_SIZE (CRIQUE_CHUNK_OVERHEAD + CRIQUE_MIN_CHUNK_LEN)

#define CRIQUE_CTRL_DISCARDED SIZE_MAX

typedef struct {
    void *buf;
    size_t size;
    size_t head;
    size_t tail;
} crique_t;

typedef enum {
    CRIQUE_STATE_FRONT,
    CRIQUE_STATE_FLOAT,
    CRIQUE_STATE_FLANK,
} crique_state_t;

typedef size_t crique_ctrl_t;

typedef struct __attribute__((__packed__)) {
    crique_ctrl_t ctrl;
    uint8_t chunk[];
} crique_chunk_t;

typedef enum {
    CRIQUE_WRITE_CHUNK_STATE_NULL,
    CRIQUE_WRITE_CHUNK_STATE_TAIL,
    CRIQUE_WRITE_CHUNK_STATE_HEAD,
    CRIQUE_WRITE_CHUNK_STATE_MID,
} crique_write_chunk_state_t;

void crique(crique_t *cq, uint8_t *buf, size_t len);
crique_state_t crique_state(crique_t const *cq);

bool crique_is_queue_empty(crique_t const *cq);
bool crique_is_write_empty(crique_t const *cq);
bool crique_is_full(crique_t const *cq);

crique_write_chunk_state_t crique_write_chunk_state(crique_t *cq);
crique_chunk_t *crique_get_write_chunk(crique_t *cq);
uint8_t *crique_get_write_ptr(crique_t *cq);
uint8_t *crique_get_write_ptr_ensure_cap(crique_t *cq, size_t additional);
size_t crique_write_len(crique_t *cq);
size_t crique_write_cap(crique_t *cq);

void crique_written(crique_t *cq, size_t nbytes);

void crique_push(crique_t *cq);
uint8_t *crique_peek(crique_t *cq);
size_t crique_peek_len(crique_t const *cq);
void crique_pop(crique_t *cq);

#endif // !CRIQUE_H
