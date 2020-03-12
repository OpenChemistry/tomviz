#include "vtkActiveScalarsProducer.h"

#include <vtkExecutive.h>
#include <vtkGarbageCollector.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

vtkStandardNewMacro(vtkActiveScalarsProducer);

vtkActiveScalarsProducer::vtkActiveScalarsProducer()
{
  this->OriginalOutput = nullptr;
  this->Output = nullptr;
}

vtkActiveScalarsProducer::~vtkActiveScalarsProducer()
{
  this->SetOutput(nullptr);
}

//----------------------------------------------------------------------------
void vtkActiveScalarsProducer::SetOutput(vtkDataObject* newOutput)
{
  vtkDataObject* oldOutput = this->OriginalOutput;
  if (newOutput != oldOutput) {
    if (newOutput) {
      newOutput->Register(this);
      this->Output = vtkImageData::New();
      this->Output->ShallowCopy(newOutput);
    } else {
      this->Output->Delete();
      this->Output = nullptr;
    }

    this->OriginalOutput = newOutput;

    this->GetExecutive()->SetOutputData(0, this->Output);
    if (oldOutput) {
      oldOutput->UnRegister(this);
    }
    this->Modified();
  }
}

void vtkActiveScalarsProducer::SetActiveScalars(char* name)
{
  vtkImageData* data = vtkImageData::SafeDownCast(this->Output);
  if (data) {
    data->GetPointData()->SetActiveScalars(name);
    data->Modified();
  }
}

vtkMTimeType vtkActiveScalarsProducer::GetMTime()
{
  vtkMTimeType mtime = this->Superclass::GetMTime();
  if (this->OriginalOutput) {
    vtkMTimeType omtime = this->OriginalOutput->GetMTime();
    if (omtime > mtime) {
      mtime = omtime;
    }
  }
  if (this->Output) {
    vtkMTimeType omtime = this->Output->GetMTime();
    if (omtime > mtime) {
      mtime = omtime;
    }
  }
  return mtime;
}

//----------------------------------------------------------------------------
void vtkActiveScalarsProducer::ReportReferences(vtkGarbageCollector* collector)
{
  this->Superclass::ReportReferences(collector);
  vtkGarbageCollectorReport(collector, this->OriginalOutput, "OriginalOutput");
}
