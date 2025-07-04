#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

#include "tqdm/tqdm.h"

// Configs
static int tests_run = 0;       /* total executed so far */
static int tests_passed = 0;
static int tests_failed = 0;
static char last_error[256] = {0};

#define TEST_SIZE 100
#define SLEEP_MS(ms) usleep((ms) * 1000)
#define LARGE_TEST_SIZE 1000000

#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_RESET "\033[0m"

/* Test framework macros */
#define TEST_START(name) \
    do { \
        tests_run++; \
        printf(COLOR_BLUE "=== %s ===" COLOR_RESET "\n", name); \
        last_error[0] = '\0'; \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            snprintf(last_error, sizeof(last_error), "Assertion failed: %s", message); \
            goto test_fail; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, message)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == NULL, message)

#define TEST_ASSERT_EQ(a, b, message) \
    TEST_ASSERT((a) == (b), message)

#define TEST_ASSERT_NE(a, b, message) \
    TEST_ASSERT((a) != (b), message)

#define TEST_ASSERT_FLOAT_EQ(a, b, tolerance, message) \
    TEST_ASSERT(fabs((a) - (b)) < (tolerance), message)

#define TEST_ASSERT_STR_EQ(a, b, message) \
    TEST_ASSERT(strcmp((a), (b)) == 0, message)

#define TEST_PASS() \
    do { \
        tests_passed++; \
        printf(COLOR_GREEN "✓ Test passed" COLOR_RESET "\n\n"); \
        return; \
    } while(0)

#define TEST_FAIL(message) \
    do { \
        snprintf(last_error, sizeof(last_error), "%s", message); \
        goto test_fail; \
    } while(0)

#define TEST_CLEANUP() \
    test_fail: \
    tests_failed++; \
    printf(COLOR_RED "✗ Test failed: %s" COLOR_RESET "\n\n", last_error);

/* Helper functions */
double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void print_test_summary(void) {
    printf("\n" COLOR_BLUE "=== TEST SUMMARY ===" COLOR_RESET "\n");
    printf("Total tests run: %d\n", tests_run);
    printf(COLOR_GREEN "Tests passed: %d" COLOR_RESET "\n", tests_passed);
    if (tests_failed > 0) {
        printf(COLOR_RED "Tests failed: %d" COLOR_RESET "\n", tests_failed);
    }
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    if (tests_failed == 0) {
        printf(COLOR_GREEN "\nAll tests passed! 🎉" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "\nSome tests failed! 😞" COLOR_RESET "\n");
    }
}

/* Individual test functions */
void test_init(void) {
    TEST_START("Init");
    
    // Test simple creation
    int data[TEST_SIZE];
    for (int i = 0; i < TEST_SIZE; i++) {
        data[i] = i;
    }
    
    tqdm_t *tqdm = tqdm_create(data, data + TEST_SIZE, sizeof(int));
    TEST_ASSERT_NOT_NULL(tqdm, "tqdm_create should not return NULL");
    
    // Test that basic properties are set correctly
    TEST_ASSERT_EQ(tqdm->count, 0, "Initial count should be 0");
    TEST_ASSERT_EQ(tqdm->n, 0, "Initial n should be 0");
    TEST_ASSERT_EQ(tqdm->closed, false, "Should not be closed initially");
    
    tqdm_destroy(tqdm);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_iteration(void) {
    TEST_START("Iteration");
    
    int data[TEST_SIZE];
    for (int i = 0; i < TEST_SIZE; i++) {
        data[i] = i * 2;  // Use different values to test retrieval
    }
    
    tqdm_t *tqdm = tqdm_create(data, data + TEST_SIZE, sizeof(int));
    TEST_ASSERT_NOT_NULL(tqdm, "tqdm_create should not return NULL");
    
    int count = 0;
    while (tqdm_has_next(tqdm)) {
        int *val = (int *)tqdm_next(tqdm);
        TEST_ASSERT_NOT_NULL(val, "tqdm_next should not return NULL");
        TEST_ASSERT_EQ(*val, count * 2, "Values should match expected sequence");
        count++;
        
        if (count % 20 == 0) {
            SLEEP_MS(10);  // Simulate some work
        }
    }
    
    TEST_ASSERT_EQ(count, TEST_SIZE, "Should iterate through all elements");
    TEST_ASSERT_EQ(tqdm->n, TEST_SIZE, "Progress counter should match iteration count");
    
    tqdm_destroy(tqdm);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_range(void) {
    TEST_START("Range");
    
    // Test basic range
    range_iterator_t *range = trange(50);
    TEST_ASSERT_NOT_NULL(range, "trange should not return NULL");
    
    int count = 0;
    while (range_has_next(range)) {
        int val = range_next(range);
        TEST_ASSERT_EQ(val, count, "Range values should be sequential");
        count++;
    }
    TEST_ASSERT_EQ(count, 50, "Should iterate through 50 values");
    
    range_destroy(range);
    
    // Test range with bounds
    range = trange_with_bounds(10, 20);
    TEST_ASSERT_NOT_NULL(range, "trange_with_bounds should not return NULL");
    
    count = 0;
    int expected = 10;
    while (range_has_next(range)) {
        int val = range_next(range);
        TEST_ASSERT_EQ(val, expected, "Range values should match bounds");
        expected++;
        count++;
    }
    TEST_ASSERT_EQ(count, 10, "Should iterate through 10 values");
    
    range_destroy(range);
    
    // Test range with step
    range = trange_with_step(0, 20, 3);
    TEST_ASSERT_NOT_NULL(range, "trange_with_step should not return NULL");
    
    count = 0;
    expected = 0;
    while (range_has_next(range)) {
        int val = range_next(range);
        TEST_ASSERT_EQ(val, expected, "Range values should match step sequence");
        expected += 3;
        count++;
    }
    TEST_ASSERT_EQ(count, 7, "Should iterate through correct number of steps"); // 0,3,6,9,12,15,18
    
    range_destroy(range);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_params(void) {
    TEST_START("Params");
    
    tqdm_params_t params = tqdm_default_params();
    params.desc = strdup("Custom test");
    params.total = 200;
    params.unit = strdup("items");
    params.mininterval = 0.05f;
    params.unit_scale = true;
    params.leave = true;
    
    tqdm_t *tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "tqdm_create_with_params should not return NULL");
    
    // Verify parameters were set
    TEST_ASSERT_STR_EQ(tqdm->params.desc, "Custom test", "Description should be set");
    TEST_ASSERT_EQ(tqdm->params.total, 200, "Total should be set");
    TEST_ASSERT_STR_EQ(tqdm->params.unit, "items", "Unit should be set");
    TEST_ASSERT_FLOAT_EQ(tqdm->params.mininterval, 0.05f, 0.001f, "Min interval should be set");
    TEST_ASSERT_EQ(tqdm->params.unit_scale, true, "Unit scale should be set");
    TEST_ASSERT_EQ(tqdm->params.leave, true, "Leave should be set");
    
    // Test updating
    for (int i = 0; i < 200; i++) {
        tqdm_update(tqdm);
        if (i % 25 == 0) {
            SLEEP_MS(10);
        }
    }
    
    TEST_ASSERT_EQ(tqdm->n, 200, "Should complete all updates");
    
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_update(void) {
    TEST_START("Update");
    
    tqdm_params_t params = tqdm_default_params();
    params.desc = strdup("Update test");
    params.total = 1000;
    
    tqdm_t *tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "tqdm_create_with_params should not return NULL");
    
    // Test single update
    tqdm_update(tqdm);
    TEST_ASSERT_EQ(tqdm->n, 1, "Single update should increment by 1");
    
    // Test update_n
    tqdm_update_n(tqdm, 49);
    TEST_ASSERT_EQ(tqdm->n, 50, "update_n should add specified amount");
    
    // Test update_to
    bool display_triggered = tqdm_update_to(tqdm, 100);
    TEST_ASSERT_EQ(tqdm->n, 100, "update_to should set to specified value");
    (void)display_triggered; // Suppress unused variable warning
    
    // Test that update_to can go forward again
    tqdm_update_to(tqdm, 150);
    TEST_ASSERT_EQ(tqdm->n, 150, "update_to should work multiple times");
    
    // Test reset
    tqdm_reset(tqdm, 500);
    TEST_ASSERT_EQ(tqdm->n, 0, "Reset should set n to 0");
    TEST_ASSERT_EQ(tqdm->params.total, 500, "Reset should update total");
    
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_postfix(void) {
    TEST_START("Postfix");
    
    tqdm_params_t params = tqdm_default_params();
    params.desc = strdup("Postfix test");
    params.total = 100;
    
    tqdm_t *tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "tqdm_create_with_params should not return NULL");
    
    // Test postfix dictionary - postfix_create returns NULL for empty list
    postfix_entry_t *postfix = postfix_create();
    TEST_ASSERT_NULL(postfix, "postfix_create should return NULL for empty list");
    
    postfix_add(&postfix, "loss", "0.123");
    postfix_add_int(&postfix, "epoch", 1);
    postfix_add_float(&postfix, "lr", 0.001);
    
    tqdm_set_postfix(tqdm, postfix);
    
    // Update with postfix changes
    for (int i = 0; i < 100; i++) {
        tqdm_update(tqdm);
        
        if (i % 25 == 0 && i > 0) {
            // Update postfix dynamically
            postfix_destroy(postfix);
            postfix = postfix_create();  // Reset to NULL
            postfix_add_float(&postfix, "loss", 0.123 - i * 0.001);
            postfix_add_int(&postfix, "epoch", 1 + i / 25);
            postfix_add_float(&postfix, "lr", 0.001 * pow(0.9, i / 25.0));
            tqdm_set_postfix(tqdm, postfix);
        }
        
        SLEEP_MS(5);
    }
    
    TEST_ASSERT_EQ(tqdm->n, 100, "Should complete all updates with postfix");
    
    postfix_destroy(postfix);
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_description(void) {
    TEST_START("Description");
    
    tqdm_params_t params = tqdm_default_params();
    params.desc = strdup("Initial desc");
    params.total = 50;
    
    tqdm_t *tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "tqdm_create_with_params should not return NULL");
    
    // Test description updates
    for (int i = 0; i < 50; i++) {
        if (i == 10) {
            tqdm_set_description(tqdm, "Updated desc");
        }
        if (i == 25) {
            tqdm_set_description_str(tqdm, "Final desc", true);
        }
        
        tqdm_update(tqdm);
        SLEEP_MS(20);
    }
    
    TEST_ASSERT_EQ(tqdm->n, 50, "Should complete all updates");
    
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_edge(void) {
    TEST_START("Edge"); // Tests some edge cases
    
    // Test with zero total
    tqdm_params_t params = tqdm_default_params();
    params.desc = strdup("Zero total");
    params.total = 0;
    
    tqdm_t *tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "Should handle zero total");
    
    tqdm_update_n(tqdm, 10);
    TEST_ASSERT_EQ(tqdm->n, 10, "Should handle updates with zero total");
    
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    // Test with disabled progress bar
    params = tqdm_default_params();
    params.desc = strdup("Disabled");
    params.disable = true;
    
    tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "Should handle disabled progress bar");
    
    tqdm_update_n(tqdm, 50);
    TEST_ASSERT_EQ(tqdm->n, 0, "Disabled progress bar should not track updates");
    
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    // Test with very large numbers
    params = tqdm_default_params();
    params.desc = strdup("Large numbers");
    params.total = 1000000;  // Large but reasonable number
    
    tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "Should handle large totals");
    
    tqdm_update_n(tqdm, 100000);
    TEST_ASSERT_EQ(tqdm->n, 100000, "Should handle large updates");
    
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_memory(void) {
    TEST_START("Memory");
    
    // Test multiple create/destroy cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        tqdm_params_t params = tqdm_default_params();
        params.desc = strdup("Memory test");
        params.unit = strdup("bytes");
        params.total = 100;
        
        tqdm_t *tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
        TEST_ASSERT_NOT_NULL(tqdm, "Should create tqdm successfully in loop");
        
        for (int i = 0; i < 100; i++) {
            tqdm_update(tqdm);
        }
        
        TEST_ASSERT_EQ(tqdm->n, 100, "Should complete updates in loop");
        
        tqdm_destroy(tqdm);
        tqdm_cleanup_params(&params);
    }
    
    // Test postfix memory management
    for (int cycle = 0; cycle < 5; cycle++) {
        postfix_entry_t *postfix = postfix_create();
        
        for (int i = 0; i < 10; i++) {
            char key[32], value[32];
            snprintf(key, sizeof(key), "key%d", i);
            snprintf(value, sizeof(value), "value%d", i);
            postfix_add(&postfix, key, value);
        }
        
        // Verify we can iterate through the list (basic sanity check)
        postfix_entry_t *current = postfix;
        int count = 0;
        while (current) {
            count++;
            current = current->next;
        }
        TEST_ASSERT_EQ(count, 10, "Should have 10 postfix entries");
        
        postfix_destroy(postfix);
    }
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_threading(void) {
    TEST_START("Threading");
    
    // Test basic thread safety features (without actual threading for simplicity)
    tqdm_params_t params = tqdm_default_params();
    params.desc = strdup("Thread test");
    params.total = 100;
    
    tqdm_t *tqdm = tqdm_create_with_params(NULL, NULL, 1, &params);
    TEST_ASSERT_NOT_NULL(tqdm, "Should create tqdm for threading test");
    
    // Test lock functionality
    pthread_mutex_t custom_lock = PTHREAD_MUTEX_INITIALIZER;
    tqdm_set_lock(&custom_lock);
    
    // Basic updates that would use the lock
    for (int i = 0; i < 100; i++) {
        tqdm_update(tqdm);
        if (i % 10 == 0) {
            tqdm_set_description_str(tqdm, "Updated in thread", false);
        }
    }
    
    TEST_ASSERT_EQ(tqdm->n, 100, "Should complete threaded updates");
    
    // Reset to default lock
    tqdm_set_lock(NULL);
    
    tqdm_destroy(tqdm);
    tqdm_cleanup_params(&params);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_format(void) {
    TEST_START("Format");
    
    // Test sizeof formatting
    char *size_str = tqdm_format_sizeof(1536.0, "B", 1024);
    TEST_ASSERT_NOT_NULL(size_str, "format_sizeof should not return NULL");
    TEST_ASSERT(strstr(size_str, "1.5") != NULL, "Should contain '1.5' for 1536 bytes");
    free(size_str);
    
    // Test interval formatting
    char *interval_str = tqdm_format_interval(3661.5);
    TEST_ASSERT_NOT_NULL(interval_str, "format_interval should not return NULL");
    free(interval_str);  // Don't test exact format as it might vary
    
    // Test number formatting
    char *num_str = tqdm_format_num(1234567.89);
    TEST_ASSERT_NOT_NULL(num_str, "format_num should not return NULL");
    free(num_str);
    
    // Test meter formatting
    char *meter = tqdm_format_meter(750, 1000, 30.5, 80, "Processing", false,
                                   "items", false, 24.6, NULL, "acc=0.95",
                                   1000, 0, NULL);
    TEST_ASSERT_NOT_NULL(meter, "format_meter should not return NULL");
    TEST_ASSERT(strstr(meter, "Processing") != NULL, "Should contain description");
    TEST_ASSERT(strstr(meter, "75%") != NULL, "Should contain percentage");
    free(meter);
    
    TEST_PASS();
    TEST_CLEANUP();
}

void test_env(void) {
    TEST_START("Env");
    
    // Save original environment
    char *orig_mininterval = getenv("TQDM_MININTERVAL");
    char *orig_unit = getenv("TQDM_UNIT");
    
    // Set test environment variables
    setenv("TQDM_MININTERVAL", "0.2", 1);
    setenv("TQDM_UNIT", "bytes", 1);
    setenv("TQDM_UNIT_SCALE", "true", 1);
    
    tqdm_params_t params = tqdm_default_params();
    tqdm_load_env_vars(&params);
    
    TEST_ASSERT_FLOAT_EQ(params.mininterval, 0.2f, 0.001f, "Should load mininterval from env");
    TEST_ASSERT_STR_EQ(params.unit, "bytes", "Should load unit from env");
    TEST_ASSERT_EQ(params.unit_scale, true, "Should load unit_scale from env");
    
    unsetenv("TQDM_MININTERVAL");
    unsetenv("TQDM_UNIT");
    unsetenv("TQDM_UNIT_SCALE");
    
    // Reset env
    if (orig_mininterval) setenv("TQDM_MININTERVAL", orig_mininterval, 1);
    if (orig_unit) setenv("TQDM_UNIT", orig_unit, 1);
    
    tqdm_cleanup_params(&params);
    
    TEST_PASS();
    TEST_CLEANUP();
}

/* Main test runner */
int main(void) {
    printf(COLOR_BLUE "=== Comprehensive tqdm.c Test Suite ===" COLOR_RESET "\n");
    printf("Testing C implementation of tqdm progress bars\n\n");
    
    test_init();
    test_iteration();
    test_range();
    test_params();
    test_update();
    test_postfix();
    test_description();
    test_format();
    test_env();
    test_edge();
    test_memory();
    test_threading();
    
    print_test_summary();
    
    return tests_failed > 0 ? 1 : 0;
} 