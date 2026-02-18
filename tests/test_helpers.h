#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

/*
 * test_helpers.h - minimal self-contained test harness
 */

#include <stdio.h>

static int _tests_run    = 0;
static int _tests_failed = 0;

#define FAIL_OUTPUT stderr

#define ASSERT(cond)                                                \
    do {                                                            \
        _tests_run++;                                               \
        if (!(cond)) {                                              \
            fprintf(FAIL_OUTPUT, "  FAIL  %s:%d  %s\n",           \
                    __FILE__, __LINE__, #cond);                     \
            _tests_failed++;                                        \
        }                                                           \
    } while (0)

#define ASSERT_EQ(a, b)                                             \
    do {                                                            \
        _tests_run++;                                               \
        if ((a) != (b)) {                                           \
            fprintf(FAIL_OUTPUT, "  FAIL  %s:%d  %s == %s\n",     \
                    __FILE__, __LINE__, #a, #b);                    \
            _tests_failed++;                                        \
        }                                                           \
    } while (0)

#define ASSERT_STR_EQ(a, b)                                         \
    do {                                                            \
        _tests_run++;                                               \
        if (strcmp((a), (b)) != 0) {                               \
            fprintf(FAIL_OUTPUT,                                    \
                    "  FAIL  %s:%d  \"%s\" == \"%s\"\n",           \
                    __FILE__, __LINE__, (a), (b));                  \
            _tests_failed++;                                        \
        }                                                           \
    } while (0)

#define RUN_TEST(fn)                                                \
    do {                                                            \
        printf("[ TEST ] " #fn "\n");                              \
        fn();                                                       \
    } while (0)

#define PRINT_RESULTS()                                             \
    do {                                                            \
        printf("\n%d/%d tests passed\n",                           \
               _tests_run - _tests_failed, _tests_run);            \
        if (_tests_failed > 0) {                                    \
            printf("FAILED\n");                                     \
            return 1;                                               \
        }                                                           \
        printf("OK\n");                                             \
    } while (0)

#endif /* TEST_HELPERS_H */
