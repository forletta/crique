#include "crique.h"
#include "minunit.h"
#include <stdint.h>
#include <string.h>

#define TEST_CHUNK_T(n)                                                                                                \
    struct __attribute__((__packed__)) {                                                                               \
        size_t len;                                                                                                    \
        uint8_t chunk[(n)];                                                                                            \
    }

MU_TEST(test_write_no_push) {
    uint8_t buf[32];
    crique_t cq;
    crique(&cq, buf, sizeof(buf));

    mu_check(crique_write_remaining(&cq) == sizeof(buf) - sizeof(crique_chunk_t));

    uint8_t data[5] = {1, 2, 3, 4, 5};
    uint8_t *write;

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    mu_check(crique_write_len(&cq) == sizeof(data));
    mu_check(crique_write_remaining(&cq) == sizeof(buf) - sizeof(crique_chunk_t) - sizeof(data));
    mu_check(crique_peek_len(&cq) == 0);
    mu_check(crique_peek(&cq) == NULL);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    mu_check(crique_write_len(&cq) == sizeof(data) * 2);
    mu_check(crique_write_remaining(&cq) == sizeof(buf) - sizeof(crique_chunk_t) - sizeof(data) * 2);
    mu_check(crique_peek_len(&cq) == 0);
    mu_check(crique_peek(&cq) == NULL);
}

MU_TEST(test_write_push_peek_pop) {
    uint8_t buf[32];
    crique_t cq;
    crique(&cq, buf, sizeof(buf));

    uint8_t data[5] = {1, 2, 3, 4, 5};
    uint8_t *write;

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    crique_push(&cq);

    uint8_t d1[5] = {1, 2, 3, 4, 5};
    mu_check(crique_peek_len(&cq) == sizeof(d1));
    mu_check(!memcmp(crique_peek(&cq), &d1, sizeof(d1)));

    crique_pop(&cq);
    mu_check(crique_peek(&cq) == NULL);
}

MU_TEST(test_write_many_push_peek_pop) {
    uint8_t buf[32];
    crique_t cq;
    crique(&cq, buf, sizeof(buf));

    uint8_t data[5] = {1, 2, 3, 4, 5};
    uint8_t *write;

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    crique_push(&cq);

    uint8_t d1[10] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    mu_check(crique_peek_len(&cq) == sizeof(d1));
    mu_check(!memcmp(crique_peek(&cq), &d1, sizeof(d1)));

    crique_pop(&cq);

    mu_check(crique_peek(&cq) == NULL);
}

MU_TEST(test_push_peek_pop_many) {
    uint8_t buf[32];
    crique_t cq;
    crique(&cq, buf, sizeof(buf));

    uint8_t data[5] = {1, 2, 3, 4, 5};
    uint8_t *write;

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    crique_push(&cq);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    crique_push(&cq);

    uint8_t d1[10] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    mu_check(crique_peek_len(&cq) == sizeof(d1));
    mu_check(!memcmp(crique_peek(&cq), &d1, sizeof(d1)));

    crique_pop(&cq);

    uint8_t d2[5] = {1, 2, 3, 4, 5};
    mu_check(crique_peek_len(&cq) == sizeof(d2));
    mu_check(!memcmp(crique_peek(&cq), &d2, sizeof(d2)));

    crique_pop(&cq);

    mu_check(crique_peek(&cq) == NULL);
}

MU_TEST(test_write_wrap_around_no_copy) {
    uint8_t buf[64];
    crique_t cq;
    crique(&cq, buf, sizeof(buf));

    uint8_t data[5] = {1, 2, 3, 4, 5};
    uint8_t *write;

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));
    crique_push(&cq);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));
    crique_push(&cq);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));
    crique_push(&cq);

    crique_pop(&cq);
    crique_pop(&cq);

    mu_check((write = crique_write_ensure_remaining(&cq, 18)) != NULL);
    mu_check(crique_write_remaining(&cq) >= 18);
}

MU_TEST(test_write_wrap_around_empty) {
    uint8_t buf[64];
    crique_t cq;
    crique(&cq, buf, sizeof(buf));

    uint8_t *write;
    mu_check((write = crique_write_ensure_remaining(&cq, 1)) != NULL);
    mu_check(crique_write_remaining(&cq) >= 1);
}

MU_TEST(test_write_wrap_around_with_copy) {
    uint8_t buf[64];
    crique_t cq;
    crique(&cq, buf, sizeof(buf));

    uint8_t data[5] = {1, 2, 3, 4, 5};
    uint8_t *write;

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));
    crique_push(&cq);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));
    crique_push(&cq);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));
    crique_push(&cq);

    crique_pop(&cq);
    crique_pop(&cq);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    mu_check((write = crique_write_ensure_remaining(&cq, 13)) != NULL);
    mu_check(crique_write_remaining(&cq) >= 13);

    mu_check((write = crique_write(&cq)) != NULL);
    memcpy(write, data, sizeof(data));
    crique_written(&cq, sizeof(data));

    crique_push(&cq);

    uint8_t d1[5] = {1, 2, 3, 4, 5};
    mu_check(crique_peek_len(&cq) == sizeof(d1));
    mu_check(!memcmp(crique_peek(&cq), &d1, sizeof(d1)));

    crique_pop(&cq);

    uint8_t d2[10] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    mu_check(crique_peek_len(&cq) == sizeof(d2));
    mu_check(!memcmp(crique_peek(&cq), &d2, sizeof(d2)));

    crique_pop(&cq);
    mu_check(crique_peek_len(&cq) == 0);
}

MU_TEST_SUITE(test_suite) {
    MU_RUN_TEST(test_write_no_push);
    MU_RUN_TEST(test_write_push_peek_pop);
    MU_RUN_TEST(test_write_many_push_peek_pop);
    MU_RUN_TEST(test_push_peek_pop_many);
    MU_RUN_TEST(test_write_wrap_around_no_copy);
    MU_RUN_TEST(test_write_wrap_around_empty);
    MU_RUN_TEST(test_write_wrap_around_with_copy);
}

int main(void) {
    MU_RUN_SUITE(test_suite);
    MU_REPORT();
    return MU_EXIT_CODE;
}
