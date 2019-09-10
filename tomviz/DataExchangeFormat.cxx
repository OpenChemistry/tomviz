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

static bool readDataSet(const std::string& fileName, const std::string& path,
                        vtkImageData* image, const QVariantMap& options)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  // If it isn't a data set, we are done
  if (!reader.isDataSet(path))
    return false;

  return GenericHDF5Format::readVolume(reader, path, image, options);
}

bool DataExchangeFormat::read(const std::string& fileName, vtkImageData* image,
                              const QVariantMap& options)
{
  std::string path = "/exchange/data";
  return readDataSet(fileName, path, image, options);
}

bool DataExchangeFormat::readDark(const std::string& fileName,
                                  vtkImageData* image,
                                  const QVariantMap& options)
{
  std::string path = "/exchange/data_dark";
  return readDataSet(fileName, path, image, options);
}

bool DataExchangeFormat::readWhite(const std::string& fileName,
                                   vtkImageData* image,
                                   const QVariantMap& options)
{
  std::string path = "/exchange/data_white";
  return readDataSet(fileName, path, image, options);
}

static bool writeData(h5::H5ReadWrite& writer, vtkImageData* image)
{
  // Assume /exchange already exists
  return GenericHDF5Format::writeVolume(writer, "/exchange", "data", image);
}

static bool writeDark(h5::H5ReadWrite& writer, vtkImageData* image)
{
  // Assume /exchange already exists
  return GenericHDF5Format::writeVolume(writer, "/exchange", "data_dark",
                                        image);
}

static bool writeWhite(h5::H5ReadWrite& writer, vtkImageData* image)
{
  // Assume /exchange already exists
  return GenericHDF5Format::writeVolume(writer, "/exchange", "data_white",
                                        image);
}

bool DataExchangeFormat::write(const std::string& fileName, DataSource* source)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::WriteOnly;
  H5ReadWrite writer(fileName, mode);

  // Create a "/exchange" group
  writer.createGroup("/exchange");

  auto t = source->producer();
  auto image = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
  if (!writeData(writer, image))
    return false;

  if (source->darkData()) {
    if (!writeDark(writer, source->darkData()))
      return false;
  }

  if (source->whiteData())
    return writeWhite(writer, source->whiteData());

  return true;
}

} // namespace tomviz
