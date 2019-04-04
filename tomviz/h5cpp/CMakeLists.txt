
find_package(HDF5 REQUIRED)

include_directories(${HDF5_INCLUDE_DIRS})

add_library(h5cpp h5readwrite.cpp)

target_link_libraries(h5cpp ${HDF5_LIBRARIES})
