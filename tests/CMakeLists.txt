# =============================================================================
# hf-se050-driver — host unit tests
# =============================================================================
# Builds the driver headers against a model of the SE050's T=1 over I2C target and runs the tests with CTest.
# No hardware and no ESP-IDF needed:
#
#   cmake -S . -B build-host -DHF_SE050_BUILD_TESTS=ON
#   cmake --build build-host
#   ctest --test-dir build-host --output-on-failure
# =============================================================================

add_executable(hf_se050_host_tests
  test_main.cpp
  test_t1.cpp
  test_apdu.cpp
  test_scp03.cpp
)

target_link_libraries(hf_se050_host_tests PRIVATE ${HF_SE050_TARGET_NAME})
target_include_directories(hf_se050_host_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(hf_se050_host_tests PRIVATE cxx_std_17)
target_compile_options(hf_se050_host_tests PRIVATE -Wall -Wextra)

add_test(NAME hf_se050_host_tests COMMAND hf_se050_host_tests)

# The code shown on the documentation site (docs/ includes these files with pymdownx.snippets), built so a
# page cannot show code that does not compile against the current headers. Compiled, never linked or run.
add_library(hf_se050_doc_snippets OBJECT
  docs/quickstart.cpp
  docs/api_tasks.cpp
  docs/ports.cpp
  docs/configuration.cpp
)
target_link_libraries(hf_se050_doc_snippets PRIVATE ${HF_SE050_TARGET_NAME})
target_include_directories(hf_se050_doc_snippets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/docs)
target_compile_features(hf_se050_doc_snippets PRIVATE cxx_std_17)
target_compile_options(hf_se050_doc_snippets PRIVATE -Wall -Wextra)
