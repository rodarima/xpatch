# Copyright (c) 2024-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

set(XPATCH_TEST_SOURCE_DIR "${CMAKE_SOURCE_DIR}/test")
set(XPATCH_TEST_BUILD_DIR "${CMAKE_BINARY_DIR}/test")

function(xpatch_test source)
  set(switches DISABLED)
  set(single NAME)
  set(multi ENV)

  cmake_parse_arguments(
    XPATCH_TEST "${switches}" "${single}" "${multi}" ${ARGN})

  list(APPEND XPATCH_TEST_ENV
    "XPATCH_BIN_DIR=${CMAKE_BINARY_DIR}/src")

  list(APPEND XPATCH_TEST_ENV
    "XPATCH_INPUTS_DIR=${XPATCH_TEST_SOURCE_DIR}/in")

  list(APPEND XPATCH_TEST_ENV
    "XPATCH_TEST_PATH=${XPATCH_TEST_SOURCE_DIR}/${source}")

  set(XPATCH_TEST_NAME ${source})
  set(driver "${XPATCH_TEST_SOURCE_DIR}/driver.sh")

  add_test(NAME "${XPATCH_TEST_NAME}"
    COMMAND
      "${driver}"
      "${XPATCH_TEST_NAME}"
    WORKING_DIRECTORY "${XPATCH_TEST_BUILD_DIR}")

  set_tests_properties("${XPATCH_TEST_NAME}"
    PROPERTIES
      TIMEOUT 5
      RUN_SERIAL TRUE
      ENVIRONMENT "${XPATCH_TEST_ENV}"
      WORKING_DIRECTORY "${XPATCH_TEST_BUILD_DIR}")

  if(XPATCH_TEST_DISABLED)
    set_property(TEST "${XPATCH_TEST_NAME}" PROPERTY DISABLED TRUE)
  endif()
endfunction(xpatch_test)

xpatch_test(test1.sh)
xpatch_test(test2.sh)
