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
  JsonRpcClient.cxx
  JsonRpcClient.h
  PassiveAcquisitionWidget.cxx
  PassiveAcquisitionWidget.h
  RegexGroupDialog.cxx
  RegexGroupDialog.h
  RegexGroupsWidget.cxx
  RegexGroupsWidget.h
  RegexGroupSubstitution.cxx
  RegexGroupSubstitution.h
  RegexGroupSubstitutionDialog.cxx
  RegexGroupSubstitutionDialog.h
  RegexGroupsSubstitutionsWidget.cxx
  RegexGroupsSubstitutionsWidget.h
  StartServerDialog.cxx
  StartServerDialog.h
)

# We need this otherwise Qt will complain, not sure why!
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(acquisition OBJECT ${ACQUISITION_SOURCES})
