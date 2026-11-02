# Installs the build tree to a scratch prefix and builds tests/install against it with find_package().
# Arguments: -DBUILD_DIR=<binary dir> -DSOURCE_DIR=<repo root> -DWORK_DIR=<scratch> -DCXX=<compiler>
file(REMOVE_RECURSE "${WORK_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${WORK_DIR}/prefix"
                RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "install failed: ${rc}")
endif()
foreach(f include/hf_se050/se050_device.hpp include/hf_se050/se050_version.h
          lib/cmake/hf_se050/hf_se050Config.cmake lib/cmake/hf_se050/hf_se050Targets.cmake)
  if(NOT EXISTS "${WORK_DIR}/prefix/${f}")
    message(FATAL_ERROR "not installed: ${f}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE_DIR}/tests/install" -B "${WORK_DIR}/build"
                        "-DCMAKE_PREFIX_PATH=${WORK_DIR}/prefix" "-DCMAKE_CXX_COMPILER=${CXX}"
                RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "consumer configure failed: ${rc}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${WORK_DIR}/build" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "consumer build failed: ${rc}")
endif()
