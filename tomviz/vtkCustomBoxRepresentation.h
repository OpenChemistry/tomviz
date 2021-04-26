/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef vtkCustomBoxRepresentation_h
#define vtkCustomBoxRepresentation_h

#include <vtkBoxRepresentation.h>

class vtkActor;

class vtkCustomBoxRepresentation : public vtkBoxRepresentation
{
public:
  vtkTypeMacro(vtkCustomBoxRepresentation, vtkBoxRepresentation)

    static vtkCustomBoxRepresentation* New();

  vtkActor** GetHandle();

protected:
  vtkCustomBoxRepresentation();
  virtual ~vtkCustomBoxRepresentation();

private:
  vtkCustomBoxRepresentation(const vtkCustomBoxRepresentation&);
  void operator=(const vtkCustomBoxRepresentation&);
};

#endif
