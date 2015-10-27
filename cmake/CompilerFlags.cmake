if (UNIX)
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-std=c++11" tomviz_have_cxx11)

  if (NOT tomviz_have_cxx11)
    message(FATAL_ERROR "Your compiler does not support C++11.  Try again with a newer compiler")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pedantic -Wshadow -Wextra")
elseif (WIN32)
  if (MSVC AND MSVC12)
    set(CMAKE_CXX_FLAGS_STD_CPP)
  else()
    # ParaView doesn't support MSVC 2015 yet
    message(FATAL_ERROR "Only the MSVC 2013 compiler is supported on windows.")
  endif()
endif()

if(CMAKE_COMPILER_IS_GNUCXX)

  include(CheckCXXCompilerFlag)

  # Addtional warnings for GCC
  set(CMAKE_CXX_FLAGS_WARN "-Wnon-virtual-dtor -Wno-long-long -Wcast-align -Wchar-subscripts -Wall -Wpointer-arith -Wformat-security -Woverloaded-virtual -fno-check-new -fno-common")

  # This flag is useful as not returning from a non-void function is an error
  # with MSVC, but it is not supported on all GCC compiler versions
  check_cxx_compiler_flag("-Werror=return-type" HAVE_GCC_ERROR_RETURN_TYPE)
  if(HAVE_GCC_ERROR_RETURN_TYPE)
    set(CMAKE_CXX_FLAGS_ERROR "-Werror=return-type")
  endif()

  # If we are compiling on Linux then set some extra linker flags too
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(CMAKE_SHARED_LINKER_FLAGS
      "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS
      "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${CMAKE_MODULE_LINKER_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS
      "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${CMAKE_EXE_LINKER_FLAGS}")
  endif()

  set(CMAKE_CXX_FLAGS_WARN "${CMAKE_CXX_FLAGS_WARN}")
  # Set up the debug CXX_FLAGS for extra warnings
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO
    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_FLAGS_WARN}")
  set(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_FLAGS_WARN} ${CMAKE_CXX_FLAGS_ERROR}")
endif()
