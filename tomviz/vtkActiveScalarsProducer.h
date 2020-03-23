/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef vtkActiveScalarsProducer_h
#define vtkActiveScalarsProducer_h

#include <vtkTrivialProducer.h>

class vtkGarbageCollector;

class vtkActiveScalarsProducer : public vtkTrivialProducer
{
public:
  static vtkActiveScalarsProducer* New();
  vtkTypeMacro(vtkActiveScalarsProducer, vtkTrivialProducer);

  vtkMTimeType GetMTime() override;

  void SetOutput(vtkDataObject* newOutput) override;

  void SetActiveScalars(char* name);

protected:
  vtkActiveScalarsProducer();
  ~vtkActiveScalarsProducer() override;

  vtkDataObject* OriginalOutput = nullptr;

  void ReportReferences(vtkGarbageCollector*) override;

private:
  vtkActiveScalarsProducer(const vtkActiveScalarsProducer&) = delete;
  void operator=(const vtkActiveScalarsProducer&) = delete;
};

#endif
