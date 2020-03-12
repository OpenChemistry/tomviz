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

  vtkDataObject* OriginalOutput;

  void ReportReferences(vtkGarbageCollector*) override;

private:
  vtkActiveScalarsProducer(const vtkActiveScalarsProducer&) = delete;
  void operator=(const vtkActiveScalarsProducer&) = delete;
};

#endif
