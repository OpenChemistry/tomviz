if(WIN32)
  set(_separator "\\;")
else()
  set(_separator ":")
endif()

function(add_python_test case)
  set(name "python_${case}")
  set(module ${case}_test)

  set(_one_value_args PYTHONPATH)
  cmake_parse_arguments(fn "" "${_one_value_args}" "" ${ARGN})

  add_test(
    NAME ${name}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "${PYTHON_EXECUTABLE}" -m unittest -v ${module}
  )
  set(_pythonpath "${tomviz_python_binary_dir}")
  set(_pythonpath "${_pythonpath}${_separator}${fn_PYTHONPATH}")
  set(_pythonpath "$ENV{PYTHONPATH}${_separator}${_pythonpath}")
  if (WIN32)
    string(REPLACE "\\;" ";" _pythonpath "${_pythonpath}")
    string(REPLACE ";" "\\;" _pythonpath "${_pythonpath}")
  endif()
  set_property(TEST ${name} PROPERTY ENVIRONMENT
    "PYTHONPATH=${_pythonpath}"
  )
endfunction()
