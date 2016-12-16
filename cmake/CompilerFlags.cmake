if (UNIX)
  include(CheckCXXCompilerFlag)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wshadow -Wextra")
elseif (WIN32)
  if (MSVC AND MSVC14)
    set(CMAKE_CXX_FLAGS_STD_CPP)
  else()
    message(FATAL_ERROR "Only the MSVC 2015 compiler is supported on windows.")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")

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
