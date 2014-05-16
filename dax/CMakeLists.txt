#--------------------------------------------------
# Find and Use ParaView
#--------------------------------------------------

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if (ParaView_SOURCE_DIR)
  include_directories( ${PARAVIEW_INCLUDE_DIRS} ${VTK_INCLUDE_DIRS})
else()
  find_package(ParaView REQUIRED)
  include(${PARAVIEW_USE_FILE})
endif()

find_package(Dax REQUIRED)
DaxConfigureTBB(REQUIRED)

add_paraview_plugin(StreamingContour "1.0"
  SERVER_MANAGER_XML MatVizStreaming.xml
  SERVER_MANAGER_SOURCES
    vtkStreamingContourRepresentation.cxx
    vtkStreamingContourRepresentation.h
    vtkStreamingContourWorker.cxx
    vtkStreamingContourWorker.h
    vtkStreamingThresholdRepresentation.cxx
    vtkStreamingThresholdRepresentation.h
    vtkStreamingThresholdWorker.cxx
    vtkStreamingThresholdWorker.h
)

set_target_properties(StreamingContour PROPERTIES COMPILE_DEFINITIONS
                      "DAX_DEVICE_ADAPTER=DAX_DEVICE_ADAPTER_TBB")