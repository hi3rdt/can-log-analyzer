/**
 * @file    unit_test.h
 * @brief   Minimal host unit-test helpers (no external framework needed).
 *          Each test is a void function; failures are counted and printed
 *          with file:line, and main() returns non-zero if any check failed.
 */
#ifndef UNIT_TEST_H
#define UNIT_TEST_H

#include <stdio.h>

static int ut_failures = 0;
static int ut_checks   = 0;

#define UT_CHECK(cond)                                                       \
    do {                                                                     \
        ut_checks++;                                                         \
        if (!(cond)) {                                                       \
            ut_failures++;                                                   \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                    \
    } while (0)

#define UT_CHECK_EQ(expected, actual)                                        \
    do {                                                                     \
        unsigned long long ut_e = (unsigned long long)(expected);            \
        unsigned long long ut_a = (unsigned long long)(actual);              \
        ut_checks++;                                                         \
        if (ut_e != ut_a) {                                                  \
            ut_failures++;                                                   \
            printf("  FAIL %s:%d: %s == %s (expected 0x%llX, got 0x%llX)\n", \
                   __FILE__, __LINE__, #expected, #actual, ut_e, ut_a);      \
        }                                                                    \
    } while (0)

#define UT_RUN(test_fn)                                                      \
    do {                                                                     \
        printf("[ RUN  ] %s\n", #test_fn);                                   \
        test_fn();                                                           \
    } while (0)

#define UT_REPORT()                                                          \
    (printf("%d checks, %d failures\n", ut_checks, ut_failures),            \
     (ut_failures == 0) ? 0 : 1)

#endif /* UNIT_TEST_H */
