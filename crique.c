#include "crique.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static crique_chunk_t *crique_idx_as_chunk(crique_t const *cq, size_t idx) {
#ifdef CRIQUE_DEBUG_ASSERT
    if (idx > cq->size - CRIQUE_MIN_CHUNK_SIZE)
        __builtin_trap();
#endif /* ifdef CRIQUE_DEBUG_ASSERT */

    return (crique_chunk_t *)&cq->buf[idx];
}

static crique_write_chunk_state_t crique_write_chunk_state_maybe_init(crique_t *cq, bool init) {
    switch (crique_state(cq)) {
    case CRIQUE_STATE_FRONT:
        if (cq->size - cq->tail >= CRIQUE_MIN_CHUNK_SIZE)
            return CRIQUE_WRITE_CHUNK_STATE_TAIL;

        break;
    case CRIQUE_STATE_FLOAT:
        if (cq->size - cq->tail >= CRIQUE_MIN_CHUNK_SIZE &&
            (!init || crique_idx_as_chunk(cq, cq->tail)->ctrl != CRIQUE_CTRL_DISCARDED))
            return CRIQUE_WRITE_CHUNK_STATE_TAIL;

        if (cq->head >= CRIQUE_MIN_CHUNK_SIZE)
            return CRIQUE_WRITE_CHUNK_STATE_HEAD;

        break;
    case CRIQUE_STATE_FLANK:
        // Note: `- 1` because `head == tail` is reserved for empty float state.
        if ((cq->head - cq->tail) - 1 >= CRIQUE_MIN_CHUNK_SIZE)
            return CRIQUE_WRITE_CHUNK_STATE_MID;

        break;
    }

    return CRIQUE_WRITE_CHUNK_STATE_NULL;
}

static void crique_init_write_chunk(crique_t *cq) {
    switch (crique_write_chunk_state_maybe_init(cq, false)) {
    case CRIQUE_WRITE_CHUNK_STATE_NULL:
    case CRIQUE_WRITE_CHUNK_STATE_HEAD: // Head should always be zeroed if available (see: `crique_pop`).
        break;
    case CRIQUE_WRITE_CHUNK_STATE_TAIL:
    case CRIQUE_WRITE_CHUNK_STATE_MID:
        crique_idx_as_chunk(cq, cq->tail)->ctrl = 0;
    }
}

void crique(crique_t *cq, uint8_t *buf, size_t size) {
    if (cq == NULL)
        return;

    if (buf == NULL || size < CRIQUE_MIN_CHUNK_SIZE) {
        *cq = (crique_t){};
        return;
    }

    ((crique_chunk_t *)buf)->ctrl = 0;

    *cq = (crique_t){
        .buf = buf,
        .size = size,
    };
}

crique_state_t crique_state(crique_t const *cq) {
    if (cq->head == 0)
        return CRIQUE_STATE_FRONT;

    if (cq->head <= cq->tail)
        return CRIQUE_STATE_FLOAT;

    return CRIQUE_STATE_FLANK;
}

bool crique_is_queue_empty(crique_t const *cq) { return cq->head == cq->tail; }

bool crique_is_write_empty(crique_t const *cq) {
    crique_chunk_t *c = crique_get_write_chunk((crique_t *)cq);

    if (c == NULL)
        return true;

    return c->ctrl == 0;
}

bool crique_is_full(crique_t const *cq) {
    switch (crique_state(cq)) {
    case CRIQUE_STATE_FRONT:
        return cq->size - cq->tail < CRIQUE_MIN_CHUNK_SIZE;
    case CRIQUE_STATE_FLOAT:
        // There is always at least CRIQUE_MIN_CHUNK_SIZE in the front.
        return false;
    case CRIQUE_STATE_FLANK:
        // Note: `- 1` because `head == tail` is reserved for empty float state.
        return (cq->head - cq->tail) - 1 < CRIQUE_MIN_CHUNK_SIZE;
    }
}

crique_write_chunk_state_t crique_write_chunk_state(crique_t *cq) {
    return crique_write_chunk_state_maybe_init(cq, true);
}

crique_chunk_t *crique_get_write_chunk(crique_t *cq) {
    switch (crique_write_chunk_state(cq)) {
    case CRIQUE_WRITE_CHUNK_STATE_NULL:
        return NULL;
    case CRIQUE_WRITE_CHUNK_STATE_HEAD:
        return crique_idx_as_chunk(cq, 0);
    case CRIQUE_WRITE_CHUNK_STATE_TAIL:
    case CRIQUE_WRITE_CHUNK_STATE_MID:
        return crique_idx_as_chunk(cq, cq->tail);
    }
}

uint8_t *crique_get_write_ptr(crique_t *cq) {
    crique_chunk_t *c = crique_get_write_chunk(cq);

    if (c == NULL)
        return NULL;

    if ((void *)(c->chunk + c->ctrl) >= cq->buf + cq->size)
        return NULL;

    return c->chunk + c->ctrl;
}

bool crique_ensure_write_cap(crique_t *cq, size_t additional) {
    if (additional <= crique_write_cap(cq))
        return true;

    if (!(crique_write_chunk_state(cq) == CRIQUE_WRITE_CHUNK_STATE_TAIL && crique_state(cq) == CRIQUE_STATE_FLOAT))
        return false;

    crique_chunk_t *back = crique_get_write_chunk(cq);

    size_t needed_cap = back->ctrl + additional;
    if (cq->head < CRIQUE_CHUNK_OVERHEAD + needed_cap)
        return false;

    crique_chunk_t *front = crique_idx_as_chunk(cq, 0);
    front->ctrl = back->ctrl;
    memcpy(front->chunk, back->chunk, back->ctrl);

    back->ctrl = CRIQUE_CTRL_DISCARDED;

    return true;
}

uint8_t *crique_get_write_ptr_ensure_cap(crique_t *cq, size_t additional) {
    if (crique_ensure_write_cap(cq, additional))
        return crique_get_write_ptr(cq);

    return NULL;
}

size_t crique_write_len(crique_t *cq) {
    crique_chunk_t *c = crique_get_write_chunk(cq);

    if (c == NULL)
        return 0;

    return c->ctrl;
}

size_t crique_write_cap(crique_t *cq) {
    switch (crique_write_chunk_state(cq)) {
    case CRIQUE_WRITE_CHUNK_STATE_NULL:
        return 0;
    case CRIQUE_WRITE_CHUNK_STATE_HEAD:
        return cq->head - CRIQUE_CHUNK_OVERHEAD - crique_idx_as_chunk(cq, 0)->ctrl;
    case CRIQUE_WRITE_CHUNK_STATE_TAIL:
        return (cq->size - cq->tail) - CRIQUE_CHUNK_OVERHEAD - crique_idx_as_chunk(cq, cq->tail)->ctrl;
    case CRIQUE_WRITE_CHUNK_STATE_MID:
        // Note: `- 1` because `head == tail` is reserved for empty float state.
        return (((cq->head - cq->tail)) - 1) - CRIQUE_CHUNK_OVERHEAD - crique_idx_as_chunk(cq, cq->tail)->ctrl;
    }
}

void crique_written(crique_t *cq, size_t nbytes) {
    crique_chunk_t *c = crique_get_write_chunk(cq);

#ifdef CRIQUE_DEBUG_ASSERT
    if (c == NULL || crique_write_cap(cq) < nbytes)
        __builtin_trap();
#endif /* ifdef CRIQUE_DEBUG_ASSERT */

    c->ctrl += nbytes;
}

void crique_push(crique_t *cq) {
    crique_chunk_t *c = crique_get_write_chunk(cq);

    switch (crique_write_chunk_state(cq)) {
    case CRIQUE_WRITE_CHUNK_STATE_NULL:
        return;
    case CRIQUE_WRITE_CHUNK_STATE_HEAD:
        cq->tail = c->ctrl + CRIQUE_CHUNK_OVERHEAD;
        break;
    case CRIQUE_WRITE_CHUNK_STATE_TAIL:
    case CRIQUE_WRITE_CHUNK_STATE_MID:
        cq->tail += c->ctrl + CRIQUE_CHUNK_OVERHEAD;
        break;
    }

    crique_init_write_chunk(cq);
}

uint8_t *crique_peek(crique_t *cq) {
    if (crique_is_queue_empty(cq))
        return NULL;

    return crique_idx_as_chunk(cq, cq->head)->chunk;
}

size_t crique_peek_len(crique_t const *cq) {
    if (crique_is_queue_empty(cq))
        return 0;

    return crique_idx_as_chunk(cq, cq->head)->ctrl;
}

void crique_pop(crique_t *cq) {
    if (crique_is_queue_empty(cq))
        return;

    crique_chunk_t *c = crique_idx_as_chunk(cq, cq->head);

    size_t next_chunk_idx = cq->head + c->ctrl + CRIQUE_CHUNK_OVERHEAD;
    bool was_full;

    switch (crique_state(cq)) {
    case CRIQUE_STATE_FRONT:
        // The crique always expects the front chunk, if avaiable, to be zeroed for it's next writing.
        c->ctrl = 0;
    case CRIQUE_STATE_FLOAT:
        cq->head = next_chunk_idx;

        if (cq->head == cq->tail && crique_is_write_empty(cq)) {
            cq->head = 0;
            cq->tail = 0;
        }

        break;
    case CRIQUE_STATE_FLANK:
        was_full = crique_is_full(cq);

        if (next_chunk_idx > cq->size - CRIQUE_MIN_CHUNK_SIZE ||
            crique_idx_as_chunk(cq, next_chunk_idx)->ctrl == CRIQUE_CTRL_DISCARDED) {
            cq->head = 0;
        } else {
            cq->head = next_chunk_idx;
        }

        if (was_full)
            crique_init_write_chunk(cq);
    }
}

#ifdef CRIQUE_TEST
#include "minunit.h"

static bool write_incrementing(crique_t *cq, size_t len, size_t offset) {
    uint8_t *ptr = crique_get_write_ptr(cq);

    if (ptr == NULL || crique_write_cap(cq) < len)
        return false;

    for (size_t i = 0; i < len; i++) {
        ptr[i] = (uint8_t)((offset + i) % 256);
    }

    crique_written(cq, len);

    return true;
}

static bool write_incrementing_ensure(crique_t *cq, size_t len, size_t offset) {
    uint8_t *ptr = crique_get_write_ptr_ensure_cap(cq, len);

    if (ptr == NULL)
        return false;

    for (size_t i = 0; i < len; i++) {
        ptr[i] = (uint8_t)((offset + i) % 256);
    }

    crique_written(cq, len);

    return true;
}

static bool cmp_incrementing(uint8_t *ptr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (ptr[i] != (uint8_t)(i % 256))
            return false;
    }

    return true;
}

MU_TEST(test_init) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(&cq, buf, sizeof(buf));

    mu_check(cq.buf == buf);
    mu_check(cq.size == sizeof(buf));
    mu_check(cq.head == 0);
    mu_check(cq.tail == 0);
}

MU_TEST(test_init_null) {
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(NULL, buf, sizeof(buf));
}

MU_TEST(test_init_null_buf) {
    crique_t cq;

    crique(&cq, NULL, CRIQUE_MIN_CHUNK_SIZE);

    mu_check(cq.buf == NULL);
    mu_check(cq.size == 0);
}

MU_TEST(test_init_too_small) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE - 1];

    crique(&cq, buf, sizeof(buf));

    mu_check(cq.buf == NULL);
    mu_check(cq.size == 0);
}

MU_TEST(test_state_front_contiguous) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    cq.tail = CRIQUE_MIN_CHUNK_SIZE;

    mu_check(crique_state(&cq) == CRIQUE_STATE_FRONT);
}

MU_TEST(test_state_front_contiguous_empty) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(&cq, buf, sizeof(buf));

    mu_check(crique_state(&cq) == CRIQUE_STATE_FRONT);
}

MU_TEST(test_state_mid_contiguous) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE * 2;

    mu_check(crique_state(&cq) == CRIQUE_STATE_FLOAT);
}

MU_TEST(test_state_wrapped) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 2;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE;

    mu_check(crique_state(&cq) == CRIQUE_STATE_FLANK);
}

MU_TEST(test_write_state_back) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    cq.tail = CRIQUE_MIN_CHUNK_SIZE;

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_TAIL);
}

MU_TEST(test_write_state_back_empty) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(&cq, buf, sizeof(buf));

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_TAIL);
}

MU_TEST(test_write_state_back_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(&cq, buf, sizeof(buf));

    cq.tail = CRIQUE_MIN_CHUNK_SIZE;

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_NULL);
}

MU_TEST(test_write_state_back_part_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    cq.tail = CRIQUE_MIN_CHUNK_SIZE + (CRIQUE_MIN_CHUNK_SIZE / 2);

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_NULL);
}

MU_TEST(test_write_state_front_back_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE * 2;

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_HEAD);
}

MU_TEST(test_write_state_front_back_part_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE;
    cq.tail = (CRIQUE_MIN_CHUNK_SIZE * 2) + (CRIQUE_MIN_CHUNK_SIZE / 2);

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_HEAD);
}

MU_TEST(test_write_state_front_back_discarded) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE * 2;

    crique_idx_as_chunk(&cq, cq.tail)->ctrl = CRIQUE_CTRL_DISCARDED;

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_HEAD);
}

MU_TEST(test_write_state_mid) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 4];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 3;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE;

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_MID);
}

MU_TEST(test_write_state_mid_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 2;
    cq.tail = cq.head - 1;

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_NULL);
}

MU_TEST(test_write_state_mid_part_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 2;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE + (CRIQUE_MIN_CHUNK_SIZE / 2);

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_NULL);
}

MU_TEST(test_write_state_mid_at_min_chunk_size) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 2;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE;

    mu_check(crique_write_chunk_state(&cq) == CRIQUE_WRITE_CHUNK_STATE_NULL);
}

MU_TEST(test_write_empty) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(&cq, buf, sizeof(buf));

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == 1);

    write_incrementing(&cq, 1, 0);

    mu_check(crique_get_write_ptr(&cq) == NULL);
    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));
}

MU_TEST(test_write_empty_sequential) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    size_t initial_cap = (CRIQUE_MIN_CHUNK_SIZE * 2) - CRIQUE_CHUNK_OVERHEAD;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == initial_cap);

    write_incrementing(&cq, 1, 0);
    size_t remaining_cap = initial_cap - 1;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == remaining_cap);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));

    write_incrementing(&cq, remaining_cap, 1);

    mu_check(crique_get_write_ptr(&cq) == NULL);
    mu_check(crique_write_len(&cq) == initial_cap);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, initial_cap));
}

MU_TEST(test_write_back) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    cq.tail = CRIQUE_MIN_CHUNK_SIZE;
    crique_idx_as_chunk(&cq, cq.tail)->ctrl = 0;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == 1);

    write_incrementing(&cq, 1, 0);

    mu_check(crique_get_write_ptr(&cq) == NULL);
    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));
}

MU_TEST(test_write_back_sequential) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.tail = CRIQUE_MIN_CHUNK_SIZE;
    crique_idx_as_chunk(&cq, cq.tail)->ctrl = 0;

    size_t initial_cap = (CRIQUE_MIN_CHUNK_SIZE * 2) - CRIQUE_CHUNK_OVERHEAD;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == initial_cap);

    write_incrementing(&cq, 1, 0);
    size_t remaining_cap = initial_cap - 1;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == remaining_cap);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));

    write_incrementing(&cq, remaining_cap, 1);

    mu_check(crique_get_write_ptr(&cq) == NULL);
    mu_check(crique_write_len(&cq) == initial_cap);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, initial_cap));
}

MU_TEST(test_write_back_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(&cq, buf, sizeof(buf));

    cq.tail = CRIQUE_MIN_CHUNK_SIZE;

    mu_check(crique_get_write_ptr(&cq) == NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == 0);
}

MU_TEST(test_write_front) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE * 2;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == 1);

    write_incrementing(&cq, 1, 0);

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));
}

MU_TEST(test_write_front_sequential) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 2;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE * 3;

    size_t initial_cap = (CRIQUE_MIN_CHUNK_SIZE * 2) - CRIQUE_CHUNK_OVERHEAD;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == initial_cap);

    write_incrementing(&cq, 1, 0);
    size_t remaining_cap = initial_cap - 1;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == remaining_cap);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));

    write_incrementing(&cq, remaining_cap, 1);

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == initial_cap);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, initial_cap));
}

MU_TEST(test_write_mid) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 4];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 3;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE;
    crique_idx_as_chunk(&cq, cq.tail)->ctrl = 0;

    size_t initial_cap = ((CRIQUE_MIN_CHUNK_SIZE * 2) - CRIQUE_CHUNK_OVERHEAD) - 1;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == initial_cap);

    write_incrementing(&cq, initial_cap, 0);

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == initial_cap);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, initial_cap));
}

MU_TEST(test_write_mid_sequential) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 5];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 4;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE;
    crique_idx_as_chunk(&cq, cq.tail)->ctrl = 0;

    size_t initial_cap = ((CRIQUE_MIN_CHUNK_SIZE * 3) - CRIQUE_CHUNK_OVERHEAD) - 1;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == initial_cap);

    write_incrementing(&cq, 1, 0);
    size_t remaining_cap = initial_cap - 1;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == remaining_cap);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));

    write_incrementing(&cq, remaining_cap, 1);

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == initial_cap);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, initial_cap));
}

MU_TEST(test_write_mid_full) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 3;
    cq.tail = cq.head - 1;

    mu_check(crique_get_write_ptr(&cq) == NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == 0);
}

MU_TEST(test_write_ensure_cap_empty) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE];

    crique(&cq, buf, sizeof(buf));

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == 1);

    mu_check(!crique_get_write_ptr_ensure_cap(&cq, CRIQUE_MIN_CHUNK_LEN + 1));

    mu_check(write_incrementing_ensure(&cq, 1, 0));

    mu_check(crique_write_len(&cq) == 1);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));
}

MU_TEST(test_write_ensure_cap_mid_contiguous) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3 + CRIQUE_MIN_CHUNK_LEN];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE + CRIQUE_MIN_CHUNK_LEN;
    cq.tail = cq.head + CRIQUE_MIN_CHUNK_SIZE;
    crique_idx_as_chunk(&cq, cq.tail)->ctrl = 0;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == CRIQUE_MIN_CHUNK_LEN);

    mu_check(write_incrementing(&cq, CRIQUE_MIN_CHUNK_LEN, 0));

    mu_check(crique_write_len(&cq) == CRIQUE_MIN_CHUNK_LEN);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, 1));

    mu_check(!crique_get_write_ptr_ensure_cap(&cq, CRIQUE_MIN_CHUNK_LEN * 2));

    mu_check(write_incrementing_ensure(&cq, CRIQUE_MIN_CHUNK_LEN, CRIQUE_MIN_CHUNK_LEN));

    mu_check(crique_write_len(&cq) == CRIQUE_MIN_CHUNK_LEN * 2);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, CRIQUE_MIN_CHUNK_LEN * 2));
}

MU_TEST(test_write_ensure_cap_wrap_around) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 4];

    crique(&cq, buf, sizeof(buf));

    cq.head = CRIQUE_MIN_CHUNK_SIZE * 3;
    cq.tail = CRIQUE_MIN_CHUNK_SIZE;
    crique_idx_as_chunk(&cq, cq.tail)->ctrl = 0;

    size_t initial_cap = ((CRIQUE_MIN_CHUNK_SIZE * 2) - CRIQUE_CHUNK_OVERHEAD) - 1;

    mu_check(crique_get_write_ptr(&cq) != NULL);
    mu_check(crique_write_len(&cq) == 0);
    mu_check(crique_write_cap(&cq) == initial_cap);

    mu_check(!write_incrementing_ensure(&cq, initial_cap + 1, 0));
    mu_check(write_incrementing_ensure(&cq, initial_cap, 0));

    mu_check(crique_write_len(&cq) == initial_cap);
    mu_check(crique_write_cap(&cq) == 0);
    mu_check(cmp_incrementing(crique_get_write_chunk(&cq)->chunk, initial_cap));
}

MU_TEST(test_ppp) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 2];

    crique(&cq, buf, sizeof(buf));

    mu_check(write_incrementing(&cq, CRIQUE_MIN_CHUNK_SIZE, 0));

    mu_check(crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) == NULL);
    mu_check(crique_peek_len(&cq) == 0);

    crique_push(&cq);

    mu_check(!crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) != NULL);
    mu_check(crique_peek_len(&cq) == CRIQUE_MIN_CHUNK_SIZE);
    mu_check(cmp_incrementing(crique_peek(&cq), CRIQUE_MIN_CHUNK_SIZE));

    mu_check(crique_write_cap(&cq) == 0);

    crique_pop(&cq);

    mu_check(crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) == NULL);
    mu_check(crique_peek_len(&cq) == 0);
}

MU_TEST(test_ppp_sequential) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    mu_check(write_incrementing(&cq, CRIQUE_MIN_CHUNK_SIZE, 0));

    mu_check(crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) == NULL);
    mu_check(crique_peek_len(&cq) == 0);

    crique_push(&cq);

    mu_check(!crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) != NULL);
    mu_check(crique_peek_len(&cq) == CRIQUE_MIN_CHUNK_SIZE);
    mu_check(cmp_incrementing(crique_peek(&cq), CRIQUE_MIN_CHUNK_SIZE));

    mu_check(crique_write_cap(&cq) != 0);
    mu_check(crique_write_len(&cq) == 0);

    mu_check(write_incrementing(&cq, CRIQUE_MIN_CHUNK_LEN, 0));

    crique_push(&cq);

    mu_check(crique_write_cap(&cq) == 0);

    crique_pop(&cq);

    mu_check(crique_peek(&cq) != NULL);
    mu_check(crique_peek_len(&cq) == CRIQUE_MIN_CHUNK_LEN);
    mu_check(cmp_incrementing(crique_peek(&cq), CRIQUE_MIN_CHUNK_LEN));

    crique_pop(&cq);

    mu_check(crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) == NULL);
    mu_check(crique_peek_len(&cq) == 0);
}

MU_TEST(test_ppp_empty_but_written) {
    crique_t cq;
    uint8_t buf[CRIQUE_MIN_CHUNK_SIZE * 3];

    crique(&cq, buf, sizeof(buf));

    mu_check(write_incrementing(&cq, CRIQUE_MIN_CHUNK_SIZE, 0));

    mu_check(crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) == NULL);
    mu_check(crique_peek_len(&cq) == 0);

    crique_push(&cq);

    mu_check(!crique_is_queue_empty(&cq));
    mu_check(crique_peek(&cq) != NULL);
    mu_check(crique_peek_len(&cq) == CRIQUE_MIN_CHUNK_SIZE);
    mu_check(cmp_incrementing(crique_peek(&cq), CRIQUE_MIN_CHUNK_SIZE));

    mu_check(crique_write_cap(&cq) != 0);
    mu_check(crique_write_len(&cq) == 0);

    mu_check(write_incrementing(&cq, CRIQUE_MIN_CHUNK_LEN, 0));

    crique_pop(&cq);

    mu_check(crique_is_queue_empty(&cq));
    mu_check(!crique_is_write_empty(&cq));
    mu_check(crique_peek(&cq) == NULL);
    mu_check(crique_peek_len(&cq) == 0);

    crique_push(&cq);

    mu_check(crique_peek(&cq) != NULL);
    mu_check(crique_peek_len(&cq) == CRIQUE_MIN_CHUNK_LEN);
    mu_check(cmp_incrementing(crique_peek(&cq), CRIQUE_MIN_CHUNK_LEN));
}

MU_TEST(test_ppp_interleaved) {
    static const size_t max_buf_size =
        (((4 + 3 + 2 + 1) + (3 + 2 + 1)) * 2 * CRIQUE_MIN_CHUNK_LEN) + (CRIQUE_CHUNK_OVERHEAD * 7) + 16;

    crique_t cq;
    uint8_t buf[max_buf_size];

    crique(&cq, buf, sizeof(buf));

    const size_t nphases = 3;
    size_t ops_per_phase[3] = {5, 3, 1};

    for (size_t i = 0; i < nphases; i++) {
        size_t push_ops = ops_per_phase[i];
        size_t pop_ops = ops_per_phase[nphases - (i + 1)];

        for (size_t write_chunk_size = push_ops; write_chunk_size > 0; write_chunk_size--) {
            mu_check(write_incrementing_ensure(&cq, write_chunk_size, 0));
            mu_check(write_incrementing_ensure(&cq, write_chunk_size, write_chunk_size));

            crique_push(&cq);
        }

        for (size_t j = 0; j < pop_ops; j++) {
            mu_check(crique_peek(&cq) != NULL);
            mu_check(cmp_incrementing(crique_peek(&cq), crique_peek_len(&cq)));

            crique_pop(&cq);
        }
    }

    mu_check(crique_is_queue_empty(&cq));
}

MU_TEST_SUITE(test_suite) {
    MU_RUN_TEST(test_init);
    MU_RUN_TEST(test_init_null);
    MU_RUN_TEST(test_init_null_buf);
    MU_RUN_TEST(test_init_too_small);
    MU_RUN_TEST(test_state_front_contiguous);
    MU_RUN_TEST(test_state_front_contiguous_empty);
    MU_RUN_TEST(test_state_mid_contiguous);
    MU_RUN_TEST(test_state_wrapped);
    MU_RUN_TEST(test_write_state_back);
    MU_RUN_TEST(test_write_state_back_empty);
    MU_RUN_TEST(test_write_state_back_full);
    MU_RUN_TEST(test_write_state_back_part_full);
    MU_RUN_TEST(test_write_state_front_back_full);
    MU_RUN_TEST(test_write_state_front_back_part_full);
    MU_RUN_TEST(test_write_state_front_back_discarded);
    MU_RUN_TEST(test_write_state_mid);
    MU_RUN_TEST(test_write_state_mid_full);
    MU_RUN_TEST(test_write_state_mid_part_full);
    MU_RUN_TEST(test_write_state_mid_at_min_chunk_size);
    MU_RUN_TEST(test_write_empty);
    MU_RUN_TEST(test_write_empty_sequential);
    MU_RUN_TEST(test_write_back);
    MU_RUN_TEST(test_write_back_sequential);
    MU_RUN_TEST(test_write_back_full);
    MU_RUN_TEST(test_write_front);
    MU_RUN_TEST(test_write_front_sequential);
    MU_RUN_TEST(test_write_mid);
    MU_RUN_TEST(test_write_mid_sequential);
    MU_RUN_TEST(test_write_mid_full);
    MU_RUN_TEST(test_write_ensure_cap_empty);
    MU_RUN_TEST(test_write_ensure_cap_mid_contiguous);
    MU_RUN_TEST(test_write_ensure_cap_wrap_around);
    MU_RUN_TEST(test_ppp);
    MU_RUN_TEST(test_ppp_sequential);
    MU_RUN_TEST(test_ppp_empty_but_written);
    MU_RUN_TEST(test_ppp_interleaved);
}

int main(void) {
    MU_RUN_SUITE(test_suite);
    MU_REPORT();
    return MU_EXIT_CODE;
}

#endif /* ifdef CRIQUE_TEST */
