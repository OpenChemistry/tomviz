# Generate an .h file containing the XML data
file(READ TomvizExtensions.xml XML_DATA)

# To generate a valid c string, remove \n and replace " with \"
string(REPLACE "\n" "" XML_DATA ${XML_DATA})
string(REPLACE "\"" "\\\"" XML_DATA ${XML_DATA})

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/TomvizExtensionsXml.h
     "static const char* tomvizExtensionsXml = \"${XML_DATA}\";")
