# one ctest per .sts file, all driven by run.sh
file(GLOB_RECURSE STEAV_TEST_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.sts)
foreach(test_file ${STEAV_TEST_FILES})
  file(RELATIVE_PATH test_name ${CMAKE_CURRENT_SOURCE_DIR} ${test_file})
  add_test(NAME ${test_name}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run.sh $<TARGET_FILE:steav> ${test_file})
endforeach()
