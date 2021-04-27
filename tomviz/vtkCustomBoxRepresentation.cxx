/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "vtkCustomBoxRepresentation.h"

#include <vtkDoubleArray.h>
#include <vtkObjectFactory.h>
#include <vtkSphereSource.h>

vtkStandardNewMacro(vtkCustomBoxRepresentation)

  vtkCustomBoxRepresentation::vtkCustomBoxRepresentation()
{
}

vtkCustomBoxRepresentation::~vtkCustomBoxRepresentation()
{
}

vtkActor** vtkCustomBoxRepresentation::GetHandle()
{
  return this->Handle;
}
