include(CMakeParseArguments)

macro(add_cxx_test name)
  set(_one_value_args PYTHONPATH)
  cmake_parse_arguments(fn "" "${_one_value_args}" "" ${ARGN})
  list(APPEND _tomviz_cxx_tests ${name})

  if(fn_PYTHONPATH)
    set(_tomviz_${name}_pythonpath ${fn_PYTHONPATH})
    message(${_tomviz_${name}_pythonpath})
  endif()
endmacro()

macro(create_test_executable name)
  set(_test_srcs "")

  foreach(_test_name ${_tomviz_cxx_tests})
    message(STATUS "Adding ${_test_name} test.")
    list(APPEND _test_srcs ${_test_name}Test.cxx)
  endforeach()
  message(STATUS "Test source files: ${_test_srcs}")

  add_executable(${name} ${_test_srcs})
  target_link_libraries(${name} tomvizlib
    ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY} ${EXTRA_LINK_LIB})

  foreach(_test_name ${_tomviz_cxx_tests})
    add_test(NAME "${_test_name}"
        COMMAND ${name} "--gtest_filter=${_test_name}Test.*")

    if(_tomviz_${_test_name}_pythonpath)
      set_tests_properties(${_test_name}
        PROPERTIES ENVIRONMENT "PYTHONPATH=${_tomviz_${_test_name}_pythonpath}")
    endif()
  endforeach()
endmacro()
