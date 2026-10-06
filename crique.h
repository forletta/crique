#ifndef CRIQUE_H
#define CRIQUE_H

#include <stddef.h>
#include <stdint.h>

#define CRIQUE_CTRL_BACK_CHUNK_WRITE_DISCARDED SIZE_MAX

#define CRIQUE_MINIMUM_CHUNK_LEN  (1)
#define CRIQUE_MINIMUM_CHUNK_SIZE (sizeof(crique_chunk_t) + CRIQUE_MINIMUM_CHUNK_LEN)

typedef struct {
    void *buf;
    size_t len;
    size_t head;
    size_t tail;
    // size_t cursor;
} crique_t;

typedef size_t crique_ctrl_t;

typedef struct __attribute__((__packed__)) {
    crique_ctrl_t len;
    uint8_t chunk[];
} crique_chunk_t;

void crique(crique_t *cq, uint8_t *buf, size_t len);

uint8_t *crique_write(crique_t *cq);
uint8_t *crique_write_ensure_remaining(crique_t *cq, size_t ensure);
size_t crique_write_remaining(crique_t const *cq);
size_t crique_write_len(crique_t const *cq);
void crique_written(crique_t *cq, size_t nbytes);
// crique_chunk_t *crique_write_chunk_swap_to_front(crique_t *cq);

void crique_push(crique_t *cq);

uint8_t *crique_peek(crique_t *cq);
size_t crique_peek_len(crique_t const *cq);

void crique_pop(crique_t *cq);

void crique_dbg(crique_t *cq);

#endif // !CRIQUE_H
