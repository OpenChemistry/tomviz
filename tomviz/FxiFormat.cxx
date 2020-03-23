/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "FxiFormat.h"

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

bool FxiFormat::read(const std::string& fileName, vtkImageData* image,
                              const QVariantMap& options)
{
  std::string path = "/img_tomo";
  return readDataSet(fileName, path, image, options);
}

bool FxiFormat::read(const std::string& fileName,
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

bool FxiFormat::readDark(const std::string& fileName,
                                  vtkImageData* image,
                                  const QVariantMap& options)
{
  std::string path = "/img_dark_avg";
  return readDataSet(fileName, path, image, options);
}

bool FxiFormat::readWhite(const std::string& fileName,
                                   vtkImageData* image,
                                   const QVariantMap& options)
{
  std::string path = "/img_bkg_avg";
  return readDataSet(fileName, path, image, options);
}

QVector<double> FxiFormat::readTheta(const std::string& fileName,
                                     const QVariantMap& options)
{
  using h5::H5ReadWrite;
  H5ReadWrite::OpenMode mode = H5ReadWrite::OpenMode::ReadOnly;
  H5ReadWrite reader(fileName.c_str(), mode);

  std::string path = "/angle";
  if (!reader.isDataSet(path)) {
    // No angles. Just return.
    return {};
  }

  return GenericHDF5Format::readAngles(reader, path, options);
}

} // namespace tomviz
