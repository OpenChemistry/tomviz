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

bool DataExchangeFormat::read(const std::string& fileName,
                              DataSource* dataSource,
                              const QVariantMap& options)
{
  vtkNew<vtkImageData> image;
  if (!read(fileName, image, options)) {
    std::cerr << "Failed to read data in: " + fileName + "\n";
    return false;
  }

  dataSource->setData(image);

  // Use the same strides and volume bounds for the dark and white data,
  // except for the tilt axis.
  QVariantMap darkWhiteOptions = options;
  int strides[3];
  int bs[6];
  dataSource->subsampleStrides(strides);
  dataSource->subsampleVolumeBounds(bs);

  QVariantList stridesList = { 1, strides[1], strides[2] };
  QVariantList boundsList = { 0, 1, bs[2], bs[3], bs[4], bs[5] };

  darkWhiteOptions["subsampleStrides"] = stridesList;
  darkWhiteOptions["subsampleVolumeBounds"] = boundsList;
  darkWhiteOptions["askForSubsample"] = false;

  // Read in the dark and white image data as well
  vtkNew<vtkImageData> darkImage, whiteImage;
  readDark(fileName, darkImage, darkWhiteOptions);
  if (darkImage->GetPointData()->GetNumberOfArrays() != 0)
    dataSource->setDarkData(std::move(darkImage));

  readWhite(fileName, whiteImage, darkWhiteOptions);
  if (whiteImage->GetPointData()->GetNumberOfArrays() != 0)
    dataSource->setWhiteData(std::move(whiteImage));

  QVector<double> angles = readTheta(fileName, options);

  if (angles.isEmpty()) {
    // Re-order the data to Fortran ordering
    GenericHDF5Format::reorderData(image, ReorderMode::CToFortran);
    GenericHDF5Format::reorderData(dataSource->darkData(),
                                   ReorderMode::CToFortran);
    GenericHDF5Format::reorderData(dataSource->whiteData(),
                                   ReorderMode::CToFortran);
  } else {
    // No re-order needed. Just re-label the axes.
    GenericHDF5Format::relabelXAndZAxes(image);
    GenericHDF5Format::relabelXAndZAxes(dataSource->darkData());
    GenericHDF5Format::relabelXAndZAxes(dataSource->whiteData());
    dataSource->setTiltAngles(angles);
    dataSource->setType(DataSource::TiltSeries);
  }

  dataSource->dataModified();

  return true;
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

QVector<double> DataExchangeFormat::readTheta(const std::string& fileName,
                                              const QVariantMap& options)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  std::string path = "/exchange/theta";
  if (!reader.isDataSet(path)) {
    // No angles. Just return.
    return {};
  }

  return GenericHDF5Format::readAngles(reader, path, options);
}

namespace {

bool writeData(h5::H5ReadWrite& writer, vtkImageData* image)
{
  vtkNew<vtkImageData> permutedImage;
  if (DataSource::hasTiltAngles(image)) {
    // No deep copies of data needed. Just re-label the axes.
    permutedImage->ShallowCopy(image);
    GenericHDF5Format::relabelXAndZAxes(permutedImage);
  } else {
    // Need to re-order to C ordering before writing
    GenericHDF5Format::reorderData(image, permutedImage,
                                   ReorderMode::FortranToC);
  }

  // Assume /exchange already exists
  return GenericHDF5Format::writeVolume(writer, "/exchange", "data",
                                        permutedImage);
}

bool writeExtraData(h5::H5ReadWrite& writer, vtkImageData* image,
                    const std::string& path, const std::string& name,
                    bool isTiltSeries = false)
{
  vtkNew<vtkImageData> permutedImage;
  if (isTiltSeries) {
    // No deep copying needed. Just re-label the axes.
    permutedImage->ShallowCopy(image);
    GenericHDF5Format::relabelXAndZAxes(permutedImage);
  } else {
    GenericHDF5Format::reorderData(image, permutedImage,
                                   ReorderMode::FortranToC);
  }

  // Assume /exchange already exists
  return GenericHDF5Format::writeVolume(writer, path, name, permutedImage);
}

bool writeDark(h5::H5ReadWrite& writer, vtkImageData* image,
               bool isTiltSeries = false)
{
  return writeExtraData(writer, image, "/exchange", "data_dark", isTiltSeries);
}

bool writeWhite(h5::H5ReadWrite& writer, vtkImageData* image,
                bool isTiltSeries = false)
{
  return writeExtraData(writer, image, "/exchange", "data_white", isTiltSeries);
}

bool writeTheta(h5::H5ReadWrite& writer, vtkImageData* image)
{
  QVector<double> angles = DataSource::getTiltAngles(image);

  if (angles.isEmpty())
    return false;

  // Assume /exchange already exists
  return writer.writeData("/exchange", "theta", { angles.size() },
                          angles.data());
}

} // namespace

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

  bool isTiltSeries = source->hasTiltAngles();

  if (source->darkData()) {
    if (!writeDark(writer, source->darkData(), isTiltSeries))
      return false;
  }

  if (source->whiteData()) {
    if (!writeWhite(writer, source->whiteData(), isTiltSeries))
      return false;
  }

  if (isTiltSeries) {
    return writeTheta(writer, image);
  }

  return true;
}

} // namespace tomviz
