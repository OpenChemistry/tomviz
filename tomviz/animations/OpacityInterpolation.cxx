/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OpacityInterpolation.h"

#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>

#include <algorithm>
#include <vector>

namespace tomviz {

namespace {

// Enough to hold a hard edge without the ramp between two samples reading
// as a visible step, and small enough that rebuilding one every frame costs
// nothing next to the render it feeds.
const int kSamples = 256;

void blendPointwise(vtkPiecewiseFunction* from, vtkPiecewiseFunction* to,
                    double u, vtkPiecewiseFunction* out)
{
  const int count = from->GetSize();
  out->RemoveAllPoints();
  for (int i = 0; i < count; ++i) {
    double a[4];
    double b[4];
    from->GetNodeValue(i, a);
    to->GetNodeValue(i, b);
    // Position, opacity, midpoint and sharpness all interpolate.
    out->AddPoint(a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u,
                  a[2] + (b[2] - a[2]) * u, a[3] + (b[3] - a[3]) * u);
  }
}

void blendThroughTable(vtkPiecewiseFunction* from, vtkPiecewiseFunction* to,
                       double u, const double range[2],
                       vtkPiecewiseFunction* out)
{
  std::vector<double> fromTable(kSamples);
  std::vector<double> toTable(kSamples);
  from->GetTable(range[0], range[1], kSamples, fromTable.data());
  to->GetTable(range[0], range[1], kSamples, toTable.data());

  std::vector<double> blended(kSamples);
  for (int i = 0; i < kSamples; ++i) {
    blended[i] = fromTable[i] + (toTable[i] - fromTable[i]) * u;
  }

  out->RemoveAllPoints();
  out->BuildFunctionFromTable(range[0], range[1], kSamples, blended.data());
}

} // anonymous namespace

void interpolateOpacity(vtkPiecewiseFunction* from, vtkPiecewiseFunction* to,
                        double u, const double range[2], vtkPiecewiseFunction* out)
{
  if (!out || !from || !to) {
    return;
  }

  u = std::min(1.0, std::max(0.0, u));

  // Build into a scratch curve so the caller may pass one of the inputs as
  // the destination.
  vtkNew<vtkPiecewiseFunction> result;

  const int fromCount = from->GetSize();
  const int toCount = to->GetSize();

  if (fromCount == 0 && toCount == 0) {
    out->RemoveAllPoints();
    return;
  }

  // With one side empty there is nothing to blend towards, so the other
  // side stands for the whole animation rather than fading to nothing.
  if (fromCount == 0 || toCount == 0) {
    result->DeepCopy(fromCount == 0 ? to : from);
  } else if (fromCount == toCount) {
    blendPointwise(from, to, u, result);
  } else if (range[1] > range[0]) {
    blendThroughTable(from, to, u, range, result);
  } else {
    // No window to sample over. Rather than emit a curve built from a
    // degenerate range, hold whichever end is nearer.
    result->DeepCopy(u < 0.5 ? from : to);
  }

  out->DeepCopy(result);
}

} // namespace tomviz
