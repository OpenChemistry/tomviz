set(classes vtkOMETiffReader)

vtk_module_add_module(vtkOMETiffReader
  CLASSES ${classes})

paraview_add_server_manager_xmls(
  XMLS  TomvizExtensions.xml)
