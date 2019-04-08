set(classes vtkOMETiffReader)

vtk_module_add_module(vtkOMETiffReaderModule
  CLASSES ${classes})

paraview_add_server_manager_xmls(
  XMLS  TomvizExtensions.xml)
