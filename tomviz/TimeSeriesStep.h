/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTimeSeriesStep_h
#define tomvizTimeSeriesStep_h

#include <QString>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

namespace tomviz {

struct TimeSeriesStep
{
  TimeSeriesStep() {}
  TimeSeriesStep(QString l, vtkImageData* img, double t)
    : label(l), image(img), time(t)
  {
  }

  TimeSeriesStep clone()
  {
    // Return an identical time series step with a deep copy of the data
    auto* copy = image->NewInstance();
    copy->DeepCopy(image);

    return TimeSeriesStep(label, copy, time);
  }

  QString label;
  vtkSmartPointer<vtkImageData> image;
  double time = 0;
};

} // namespace tomviz

#endif
