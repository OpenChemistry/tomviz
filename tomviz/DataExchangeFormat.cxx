/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataExchangeFormat.h"

#include "DataSource.h"
#include "GenericHDF5Format.h"

#include <h5cpp/h5readwrite.h>
#include <h5cpp/h5vtktypemaps.h>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTrivialProducer.h>

#include <string>
#include <vector>

#include <iostream>

namespace tomviz {

bool DataExchangeFormat::read(const std::string& fileName, vtkImageData* image,
                              const QVariantMap& options)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  std::string deDataNode = "/exchange/data";
  // If it isn't a data set, we are done
  if (!reader.isDataSet(deDataNode))
    return false;

  return GenericHDF5Format::readVolume(reader, deDataNode, image, options);
}

bool DataExchangeFormat::write(const std::string& fileName, DataSource* source)
{
  auto t = source->producer();
  auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  return this->write(fileName, image);
}

bool DataExchangeFormat::write(const std::string& fileName, vtkImageData* image)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::WriteOnly;
  H5ReadWrite writer(fileName, mode);

  // Now create an "/exchange" group
  writer.createGroup("/exchange");

  return GenericHDF5Format::writeVolume(writer, "/exchange", "data", image);
}

} // namespace tomviz
