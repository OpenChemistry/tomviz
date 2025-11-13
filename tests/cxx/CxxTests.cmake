include(CMakeParseArguments)

macro(add_cxx_test name)
  set(_one_value_args PYTHONPATH)
  cmake_parse_arguments(fn "" "" "${_one_value_args}" "" ${ARGN})
  list(APPEND _tomviz_cxx_tests ${name})

  if(fn_PYTHONPATH)
    set(_tomviz_${name}_pythonpath "${fn_PYTHONPATH}")
    message(${_tomviz_${name}_pythonpath})
  endif()
endmacro()

macro(add_cxx_qtest name)
  set(_one_value_args PYTHONPATH)
  cmake_parse_arguments(fn "" "" "${_one_value_args}" "" ${ARGN})

  if(fn_PYTHONPATH)
    set(_pythonpath "${fn_PYTHONPATH}")
    message("PYTHONPATH for ${name}: ${_pythonpath})")
  endif()

  set(_test_src ${name}Test.cxx)
  message(STATUS "Test source file: ${_test_src}")

  set(_executable_name "qtest${name}")
  add_executable(${_executable_name} ${_test_src})
  target_link_libraries(${_executable_name} tomvizlib Qt6::Test)
  # Need to set this property so that in stack traces, tomviz symbols
  # can be resolved.
  set_target_properties(${_executable_name} PROPERTIES ENABLE_EXPORTS TRUE)

  add_test(NAME "${name}" COMMAND ${_executable_name})
  if(_pythonpath)
    if (WIN32)
      string(REPLACE "\\;" ";" "_pythonpath" "${_pythonpath}")
      string(REPLACE ";" "\\;" "_pythonpath" "${_pythonpath}")
    endif()
    set_tests_properties(${name}
      PROPERTIES ENVIRONMENT "PYTHONPATH=${_pythonpath}")
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
    GTest::gtest GTest::gtest_main ${EXTRA_LINK_LIB})

  # Need to set this property so that in stack traces, tomviz symbols
  # can be resolved.
  set_target_properties(${_executable_name} PROPERTIES ENABLE_EXPORTS TRUE)

  foreach(_test_name ${_tomviz_cxx_tests})
    add_test(NAME "${_test_name}"
        COMMAND ${name} "--gtest_filter=${_test_name}Test.*")

    if(_tomviz_${_test_name}_pythonpath)
      if (WIN32)
        string(REPLACE "\\;" ";" "_tomviz_${_test_name}_pythonpath" "${_tomviz_${_test_name}_pythonpath}")
        string(REPLACE ";" "\\;" "_tomviz_${_test_name}_pythonpath" "${_tomviz_${_test_name}_pythonpath}")
      endif()
      set_tests_properties(${_test_name}
        PROPERTIES ENVIRONMENT "PYTHONPATH=${_tomviz_${_test_name}_pythonpath};TOMVIZ_APPLICATION=1")
    endif()
  endforeach()
endmacro()

