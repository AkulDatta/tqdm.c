#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tqdm/tqdm.h"

/* Test framework macros */
#define TEST_ASSERT(condition, message)                                      \
  do {                                                                       \
    if (!(condition)) {                                                      \
      fprintf(stderr, "❌ FAIL: %s\n", message);                             \
      return false;                                                          \
    }                                                                        \
  } while (0)

#define TEST_PASS(message)                                                   \
  do {                                                                       \
    printf("✓ %s\n", message);                                               \
    return true;                                                             \
  } while (0)

/* Test counter */
static int iteration_count = 0;

/* Test TQDM_FOR */
static bool test_tqdm_for_basic(void) {
  printf("\n=== Testing TQDM_FOR (Basic Range) ===\n");

  iteration_count = 0;

  TQDM_FOR(int, i, 0, 10) {
    iteration_count++;
    TEST_ASSERT(i >= 0 && i < 10, "Iterator value out of range");
    usleep(1000); /* Small delay to see progress */
  }

  TEST_ASSERT(iteration_count == 10, "Expected 10 iterations");
  TEST_PASS("TQDM_FOR basic range test");
}

/* Test TQDM_FOR with negative range */
static bool test_tqdm_for_negative(void) {
  printf("\n=== Testing TQDM_FOR (Negative Range) ===\n");

  iteration_count = 0;

  TQDM_FOR(int, i, -5, 5) {
    iteration_count++;
    TEST_ASSERT(i >= -5 && i < 5, "Iterator value out of range");
    usleep(500);
  }

  TEST_ASSERT(iteration_count == 10, "Expected 10 iterations");
  TEST_PASS("TQDM_FOR negative range test");
}

/* Test TQDM_FOR_STEP */
static bool test_tqdm_for_step(void) {
  printf("\n=== Testing TQDM_FOR_STEP ===\n");

  iteration_count = 0;

  TQDM_FOR_STEP(int, i, 0, 20, 2) {
    iteration_count++;
    TEST_ASSERT(i % 2 == 0, "Iterator should be even");
    TEST_ASSERT(i >= 0 && i < 20, "Iterator value out of range");
    usleep(2000);
  }

  TEST_ASSERT(iteration_count == 10, "Expected 10 iterations");
  TEST_PASS("TQDM_FOR_STEP test");
}

/* Test TQDM_FOR_STEP with large step */
static bool test_tqdm_for_step_large(void) {
  printf("\n=== Testing TQDM_FOR_STEP (Large Step) ===\n");

  iteration_count = 0;

  TQDM_FOR_STEP(int, i, 0, 100, 25) {
    iteration_count++;
    TEST_ASSERT(i % 25 == 0, "Iterator should be multiple of 25");
    usleep(5000);
  }

  TEST_ASSERT(iteration_count == 4, "Expected 4 iterations");
  TEST_PASS("TQDM_FOR_STEP large step test");
}

/* Test TQDM_FOR_ARRAY */
static bool test_tqdm_for_array(void) {
  printf("\n=== Testing TQDM_FOR_ARRAY ===\n");

  int test_array[] = {10, 20, 30, 40, 50};
  int array_size = sizeof(test_array) / sizeof(test_array[0]);
  int sum = 0;
  iteration_count = 0;

  TQDM_FOR_ARRAY(int *, ptr, test_array, array_size) {
    iteration_count++;
    sum += *ptr;
    TEST_ASSERT(*ptr > 0, "Array value should be positive");
    usleep(3000);
  }

  TEST_ASSERT(iteration_count == 5, "Expected 5 iterations");
  TEST_ASSERT(sum == 150, "Expected sum of 150");
  TEST_PASS("TQDM_FOR_ARRAY test");
}

/* Test TQDM_FOR_ARRAY with strings */
static bool test_tqdm_for_array_strings(void) {
  printf("\n=== Testing TQDM_FOR_ARRAY (Strings) ===\n");

  const char *words[] = {"hello", "world", "tqdm", "progress", "bar"};
  int array_size = sizeof(words) / sizeof(words[0]);
  int total_length = 0;
  iteration_count = 0;

  TQDM_FOR_ARRAY(const char **, word_ptr, words, array_size) {
    iteration_count++;
    total_length += strlen(*word_ptr);
    usleep(2000);
  }

  TEST_ASSERT(iteration_count == 5, "Expected 5 iterations");
  TEST_ASSERT(
      total_length == 25,
      "Expected total length of 25"); /* hello(5) + world(5) + tqdm(4) +
                                         progress(8) + bar(3) = 25 */
  TEST_PASS("TQDM_FOR_ARRAY strings test");
}

/* Test TQDM_MANUAL */
static bool test_tqdm_manual(void) {
  printf("\n=== Testing TQDM_MANUAL ===\n");

  iteration_count = 0;

  TQDM_MANUAL(pbar, 15) {
    for (int i = 0; i < 15; i++) {
      iteration_count++;
      /* Simulate work */
      usleep(1500);
      TQDM_UPDATE(pbar);
    }
  }

  TEST_ASSERT(iteration_count == 15, "Expected 15 iterations");
  TEST_PASS("TQDM_MANUAL test");
}

/* Test TQDM_MANUAL with nested loop */
static bool test_tqdm_manual_nested(void) {
  printf("\n=== Testing TQDM_MANUAL (Nested Loop) ===\n");

  iteration_count = 0;

  TQDM_MANUAL(pbar, 12) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 4; j++) {
        iteration_count++;
        /* Simulate complex work */
        usleep(2000);
        TQDM_UPDATE(pbar);
      }
    }
  }

  TEST_ASSERT(iteration_count == 12, "Expected 12 iterations");
  TEST_PASS("TQDM_MANUAL nested loop test");
}

/* Test nested macros */
static bool test_nested_macros(void) {
  printf("\n=== Testing Nested Macro Usage ===\n");

  iteration_count = 0;

  TQDM_MANUAL(outer_pbar, 6) {
    for (int i = 0; i < 2; i++) {
      TQDM_FOR(int, j, 0, 3) {
        (void)j; /* Suppress unused variable warning */
        iteration_count++;
        usleep(1000);
      }
      TQDM_UPDATE(outer_pbar);
    }
  }

  TEST_ASSERT(iteration_count == 6, "Expected 6 iterations");
  TEST_PASS("Nested macro usage test");
}

/* Test macro with break */
static bool test_macro_with_break(void) {
  printf("\n=== Testing Macro with Break Statement ===\n");

  iteration_count = 0;

  TQDM_FOR(int, i, 0, 20) {
    iteration_count++;
    if (i >= 7)
      break;
    usleep(1000);
  }

  TEST_ASSERT(iteration_count == 8, "Expected 8 iterations before break");
  TEST_PASS("Macro with break statement test");
}

/* Test macro with continue */
static bool test_macro_with_continue(void) {
  printf("\n=== Testing Macro with Continue Statement ===\n");

  iteration_count = 0;
  int processed_count = 0;

  TQDM_FOR(int, i, 0, 10) {
    iteration_count++;
    if (i % 2 == 0)
      continue;
    processed_count++;
    usleep(1000);
  }

  TEST_ASSERT(iteration_count == 10, "Expected 10 total iterations");
  TEST_ASSERT(processed_count == 5, "Expected 5 processed iterations");
  TEST_PASS("Macro with continue statement test");
}

/* Test macro memory safety */
static bool test_macro_memory_safety(void) {
  printf("\n=== Testing Macro Memory Safety ===\n");

  /* Test multiple consecutive macro calls */
  for (int test = 0; test < 5; test++) {
    iteration_count = 0;

    TQDM_FOR(int, i, 0, 3) {
      (void)i; /* Suppress unused variable warning */
      iteration_count++;
    }

    TEST_ASSERT(iteration_count == 3, "Each test should have 3 iterations");
  }

  /* Test array macro memory safety */
  for (int test = 0; test < 3; test++) {
    int arr[] = {1, 2, 3};
    iteration_count = 0;

    TQDM_FOR_ARRAY(int *, ptr, arr, 3) {
      (void)ptr; /* Suppress unused variable warning */
      iteration_count++;
    }

    TEST_ASSERT(iteration_count == 3,
                "Each array test should have 3 iterations");
  }

  TEST_PASS("Macro memory safety test");
}

/* Run all tests */
int main(void) {
  printf("=== TQDM MACRO TEST SUITE ===\n");
  printf("Testing automatic progress tracking macros\n");

  bool (*tests[])(void) = {
      test_tqdm_for_basic,      test_tqdm_for_negative,
      test_tqdm_for_step,       test_tqdm_for_step_large,
      test_tqdm_for_array,      test_tqdm_for_array_strings,
      test_tqdm_manual,         test_tqdm_manual_nested,
      test_nested_macros,       test_macro_with_break,
      test_macro_with_continue, test_macro_memory_safety,
  };

  int total_tests = sizeof(tests) / sizeof(tests[0]);
  int passed_tests = 0;

  for (int i = 0; i < total_tests; i++) {
    if (tests[i]()) {
      passed_tests++;
    }
  }

  printf("\n=== MACRO TEST SUMMARY ===\n");
  printf("Total tests run: %d\n", total_tests);
  printf("Tests passed: %d\n", passed_tests);
  printf("Success rate: %.1f%%\n", (float)passed_tests / total_tests * 100);

  if (passed_tests == total_tests) {
    printf("\nAll macro tests passed! 🎉\n");
    return 0;
  } else {
    printf("\n❌ Some tests failed.\n");
    return 1;
  }
}