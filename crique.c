#include "crique.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static crique_chunk_t *crique_front_chunk(crique_t const *cq) { return cq->buf; }

static size_t crique_front_chunk_cap(crique_t const *cq) {
    if (cq->head > sizeof(crique_chunk_t))
        return cq->head - sizeof(crique_chunk_t);

    if (cq->tail == 0)
        return cq->len - sizeof(crique_chunk_t);

    return 0;
}

static crique_chunk_t *crique_back_chunk(crique_t const *cq) {
    if (cq->tail >= cq->len - (sizeof(crique_chunk_t) + 1))
        return NULL;

    return cq->buf + cq->tail;
}

static crique_chunk_t *crique_next_chunk(crique_chunk_t const *c) {
    return (crique_chunk_t *)(((uint8_t *)c) + c->len + sizeof(crique_chunk_t));
}

static bool crique_is_valid_available_chunk(crique_t const *cq, crique_chunk_t const *c) {
    uint8_t *chunk = ((uint8_t *)c), *buffer = ((uint8_t *)cq->buf);

    if (chunk < buffer)
        return false;

    size_t idx = chunk - buffer;

    if (cq->tail >= cq->head) {
        return (idx >= cq->tail && cq->len - cq->tail > CRIQUE_MINIMUM_CHUNK_SIZE) ||
               (idx + CRIQUE_MINIMUM_CHUNK_SIZE < cq->head);
    }

    return (idx >= cq->tail && cq->head - cq->tail > CRIQUE_MINIMUM_CHUNK_SIZE);
}

static size_t crique_get_chunk_idx(crique_t const *cq, crique_chunk_t const *c) {
    return ((uint8_t *)c) - ((uint8_t *)cq->buf);
}

static void crique_init_write_chunk(crique_chunk_t *c) { c->len = 0; }

static crique_chunk_t *crique_write_chunk(crique_t const *cq) {
    crique_chunk_t *c = crique_back_chunk(cq);

    if (!(c == NULL || c->len == CRIQUE_CTRL_BACK_CHUNK_WRITE_DISCARDED))
        return c;

    if (cq->head == 0)
        return NULL;

    return crique_front_chunk(cq);
}

static size_t crique_write_chunk_size(crique_t const *cq) {
    crique_chunk_t *c = crique_write_chunk(cq);

    return c == NULL ? 0 : c->len + sizeof(crique_chunk_t);
}

static bool crique_write_chunk_is_front(crique_t const *cq) {
    return crique_write_chunk(cq) == ((crique_chunk_t *)cq->buf);
}

static crique_chunk_t *crique_write_chunk_swap_to_front(crique_t *cq) {
    if (crique_write_chunk_is_front(cq))
        return crique_write_chunk(cq);

    if (cq->head == 0)
        return NULL;

    if (crique_write_len(cq) > crique_front_chunk_cap(cq))
        return NULL;

    crique_chunk_t *back_chunk = crique_back_chunk(cq);
    crique_chunk_t *front_chunk = crique_front_chunk(cq);

    front_chunk->len = back_chunk->len;
    memcpy(front_chunk->chunk, back_chunk->chunk, back_chunk->len);

    back_chunk->len = CRIQUE_CTRL_BACK_CHUNK_WRITE_DISCARDED;

    return front_chunk;
}

void crique(crique_t *cq, uint8_t *buf, size_t len) {
    ((crique_chunk_t *)buf)->len = 0;

    *cq = (crique_t){
        .buf = buf,
        .len = len,
    };
}

uint8_t *crique_write(crique_t *cq) {
    crique_chunk_t *c = crique_write_chunk(cq);

    if (c == NULL)
        return NULL;

    if (crique_write_remaining(cq) == 0)
        return NULL;

    return c->chunk + c->len;
}

uint8_t *crique_write_ensure_remaining(crique_t *cq, size_t ensure) {
    if (ensure <= crique_write_remaining(cq))
        return crique_write(cq);

    if (crique_write_chunk_is_front(cq) || crique_front_chunk_cap(cq) < ensure)
        return NULL;

    crique_write_chunk_swap_to_front(cq);

    return crique_write(cq);
}

size_t crique_write_remaining(crique_t const *cq) {
    crique_chunk_t *c = crique_write_chunk(cq);

    if (c == crique_front_chunk(cq) && cq->head != 0)
        return cq->head - crique_write_chunk_size(cq);

    return (cq->len - cq->tail) - crique_write_chunk_size(cq);
}

size_t crique_write_len(crique_t const *cq) {
    crique_chunk_t *c = crique_write_chunk(cq);

    return c == NULL ? 0 : c->len;
}

void crique_written(crique_t *cq, size_t nbytes) {
    crique_chunk_t *c = crique_write_chunk(cq);

    if (c == NULL)
        return;

    c->len += nbytes;
}

void crique_push(crique_t *cq) {
    crique_chunk_t *c = crique_next_chunk(crique_write_chunk(cq));
    cq->tail = crique_get_chunk_idx(cq, c);

    if (crique_is_valid_available_chunk(cq, c))
        crique_init_write_chunk(c);
}

static crique_chunk_t *crique_peek_chunk(crique_t const *cq) {
    if (cq->head == cq->tail)
        return NULL;

    return cq->buf + cq->head;
}

uint8_t *crique_peek(crique_t *cq) {
    crique_chunk_t *c;

    return (c = crique_peek_chunk(cq)) == NULL ? NULL : c->chunk;
}

size_t crique_peek_len(crique_t const *cq) {
    crique_chunk_t *c;

    return (c = crique_peek_chunk(cq)) == NULL ? 0 : c->len;
}

void crique_pop(crique_t *cq) {
    crique_chunk_t *c = crique_peek_chunk(cq);

    if (c == NULL)
        return;

    size_t next_chunk_idx = crique_get_chunk_idx(cq, crique_next_chunk(c));

    cq->head = cq->head < cq->tail ? next_chunk_idx : 0;
}
