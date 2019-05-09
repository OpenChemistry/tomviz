/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizH5VtkTypeMaps_h
#define tomvizH5VtkTypeMaps_h

#include <iostream>
#include <map>

#include <vtkType.h>

#include <h5cpp/h5readwrite.h>

namespace h5 {

static const std::map<h5::H5ReadWrite::DataType, int> DataTypeToVtk = {
  { h5::H5ReadWrite::DataType::Int8, VTK_SIGNED_CHAR },
  { h5::H5ReadWrite::DataType::Int16, VTK_SHORT },
  { h5::H5ReadWrite::DataType::Int32, VTK_INT },
  { h5::H5ReadWrite::DataType::Int64, VTK_LONG_LONG },
  { h5::H5ReadWrite::DataType::UInt8, VTK_UNSIGNED_CHAR },
  { h5::H5ReadWrite::DataType::UInt16, VTK_UNSIGNED_SHORT },
  { h5::H5ReadWrite::DataType::UInt32, VTK_UNSIGNED_INT },
  { h5::H5ReadWrite::DataType::UInt64, VTK_UNSIGNED_LONG_LONG },
  { h5::H5ReadWrite::DataType::Float, VTK_FLOAT },
  { h5::H5ReadWrite::DataType::Double, VTK_DOUBLE }
};

class H5VtkTypeMaps
{
public:
  static int dataTypeToVtk(h5::H5ReadWrite::DataType& type)
  {
    auto it = DataTypeToVtk.find(type);

    if (it == DataTypeToVtk.end()) {
      std::cerr << "Could not convert DataType to Vtk!\n";
      return -1;
    }

    return it->second;
  }

  static h5::H5ReadWrite::DataType VtkToDataType(int type)
  {
    auto it = DataTypeToVtk.cbegin();
    while (it != DataTypeToVtk.cend()) {

      if (it->second == type)
        return it->first;

      ++it;
    }

    std::cerr << "Failed to convert Vtk to DataType\n";
    return h5::H5ReadWrite::DataType::None;
  }
};
} // namespace h5

#endif // tomvizH5VtkTypeMaps_h
