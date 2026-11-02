/**
 * @file test_main.cpp
 * @brief Runs every registered host unit test and returns non-zero on failure.
 */
#include "test_harness.hpp"

int main() {
  int failed_cases = 0;
  for (const auto& test : se050_test::registry()) {
    const int before = se050_test::failureCount();
    std::printf("[ RUN  ] %s\n", test.name);
    test.fn();
    const bool ok = se050_test::failureCount() == before;
    std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", test.name);
    if (!ok) {
      ++failed_cases;
    }
  }
  std::printf("%zu test cases, %d failed\n", se050_test::registry().size(), failed_cases);
  return failed_cases == 0 ? 0 : 1;
}
