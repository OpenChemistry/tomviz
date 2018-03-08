set(ACQUISITION_SOURCES
  AcquisitionWidget.cxx
  AcquisitionWidget.h
  AcquisitionClient.cxx
  AcquisitionClient.h
  Connection.cxx
  Connection.h
  ConnectionsWidget.cxx
  ConnectionsWidget.h
  ConnectionDialog.cxx
  ConnectionDialog.h
  PassiveAcquisitionWidget.cxx
  PassiveAcquisitionWidget.h
)

# We need this otherwise Qt will complain, not sure why!
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(acquisition OBJECT ${ACQUISITION_SOURCES})
