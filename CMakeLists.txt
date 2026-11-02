# =============================================================================
# hf-se050-driver — Root CMakeLists.txt
# =============================================================================
# NXP SE050 / SE050A secure element (I²C). Header-only interface library, installable as hf::se050.
# For ESP-IDF, use the component under examples/esp32/components/hf_se050.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

project(hf_se050
  VERSION 0.3.0
  LANGUAGES CXX
)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/hf_se050_build_settings.cmake)

include(GNUInstallDirs)

# One version everywhere: build settings, project() and the ESP-IDF component manifest.
if(NOT PROJECT_VERSION VERSION_EQUAL HF_SE050_VERSION)
  message(FATAL_ERROR "project() version ${PROJECT_VERSION} != build settings ${HF_SE050_VERSION}")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/examples/esp32/components/hf_se050/idf_component.yml"
     _hf_se050_idf_version REGEX "^version:")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" _hf_se050_idf_version "${_hf_se050_idf_version}")
if(NOT _hf_se050_idf_version VERSION_EQUAL HF_SE050_VERSION)
  message(FATAL_ERROR "idf_component.yml version ${_hf_se050_idf_version} != build settings ${HF_SE050_VERSION}")
endif()

add_library(${HF_SE050_TARGET_NAME} INTERFACE)
target_include_directories(${HF_SE050_TARGET_NAME} INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>"
  "$<BUILD_INTERFACE:${HF_SE050_VERSION_HEADER_DIR}>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/hf_se050>"
)
target_compile_features(${HF_SE050_TARGET_NAME} INTERFACE cxx_std_17)
set_target_properties(${HF_SE050_TARGET_NAME} PROPERTIES EXPORT_NAME se050)

add_library(hf::se050 ALIAS ${HF_SE050_TARGET_NAME})

option(HF_SE050_ENABLE_WARNINGS "Enable compiler warnings for SE050 consumers" OFF)
if(HF_SE050_ENABLE_WARNINGS)
  target_compile_options(${HF_SE050_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ── Host unit tests (T=1 model; no hardware) ────────────────────────────────
option(HF_SE050_BUILD_TESTS "Build the host unit tests in tests/" OFF)
if(HF_SE050_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/hf_se050ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hf_se050Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/hf_se050Config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_se050
)

# ── Install: headers, generated version header, exported target hf::se050 ──
install(TARGETS ${HF_SE050_TARGET_NAME} EXPORT hf_se050Targets)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/inc/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hf_se050
  FILES_MATCHING PATTERN "*.hpp"
)
install(FILES "${HF_SE050_VERSION_HEADER}" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hf_se050)
install(EXPORT hf_se050Targets
  NAMESPACE hf::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_se050
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/hf_se050Config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/hf_se050ConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_se050
)
