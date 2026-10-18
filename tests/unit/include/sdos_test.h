/* Minimal assertion helpers shared by the C unit tests. Each test program
 * prints one line per check and returns the failure count as exit status. */
#ifndef SDOS_TEST_H
#define SDOS_TEST_H

#include <stdio.h>

static int sdos_test_failures;

#define CHECK(name, condition)                                                        \
    do {                                                                              \
        if (condition) {                                                              \
            printf("ok   %s\n", (name));                                              \
        } else {                                                                      \
            printf("FAIL %s: %s (%s:%d)\n", (name), #condition, __FILE__, __LINE__); \
            ++sdos_test_failures;                                                     \
        }                                                                             \
    } while (0)

#define CHECK_EQ(name, got, want)                                                                        \
    do {                                                                                                 \
        long long sdos_got_ = (long long)(got), sdos_want_ = (long long)(want);                          \
        if (sdos_got_ == sdos_want_) {                                                                   \
            printf("ok   %s\n", (name));                                                                 \
        } else {                                                                                         \
            printf("FAIL %s: %s = %lld, expected %lld (%s:%d)\n", (name), #got, sdos_got_, sdos_want_, \
                   __FILE__, __LINE__);                                                                  \
            ++sdos_test_failures;                                                                        \
        }                                                                                                \
    } while (0)

#define TEST_EXIT() (sdos_test_failures == 0 ? 0 : 1)

#endif /* SDOS_TEST_H */
