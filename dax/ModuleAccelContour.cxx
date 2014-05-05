/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "dax/ModuleAccelContour.h"

#include "vtkNew.h"
#include "vtkSmartPointer.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"

#include "vtkDataSetAttributes.h"
#include "vtkPointData.h"
#include "vtkTrivialProducer.h"

#include "SubdividedVolume.h"


namespace
{

#define temTemplateMacro(call)                                              \
  vtkTemplateMacroCase(VTK_FLOAT, float, call);                             \
  vtkTemplateMacroCase(VTK_INT, int, call);                                 \
  vtkTemplateMacroCase(VTK_UNSIGNED_INT, unsigned int, call);               \
  vtkTemplateMacroCase(VTK_SHORT, short, call);                             \
  vtkTemplateMacroCase(VTK_UNSIGNED_SHORT, unsigned short, call);           \
  vtkTemplateMacroCase(VTK_CHAR, char, call);                               \
  vtkTemplateMacroCase(VTK_UNSIGNED_CHAR, unsigned char, call)

#define temDataArrayIteratorMacro(_array, _call)                           \
  temTemplateMacro(                                                        \
    vtkAbstractArray *_aa(_array);                                         \
    if (vtkDataArrayTemplate<VTK_TT> *_dat =                               \
        vtkDataArrayTemplate<VTK_TT>::FastDownCast(_aa))                   \
      {                                                                    \
      typedef VTK_TT vtkDAValueType;                                       \
      typedef vtkDataArrayTemplate<vtkDAValueType> vtkDAContainerType;     \
      typedef vtkDAContainerType::Iterator vtkDAIteratorType;              \
      vtkDAIteratorType vtkDABegin(_dat->Begin());                         \
      vtkDAIteratorType vtkDAEnd(_dat->End());                             \
      _call;                                                               \
      }                                                                    \
    )
}

namespace TEM
{

//-----------------------------------------------------------------------------
ContourWorker::ContourWorker(QObject *p) :
    QThread(p),
    input(),
    volume(),
    numSubGridsPerDim(16)
{
}

//-----------------------------------------------------------------------------
ContourWorker::~ContourWorker()
{

}

//-----------------------------------------------------------------------------
template <class IteratorType>
void ContourWorker::createSearchStructure(IteratorType begin,
                           IteratorType end)
  {
  this->volume.reset(new TEM::accel::SubdividedVolume( numSubGridsPerDim,
                                                       input.GetPointer(),
                                                       std::cout ) );
  this->volume->ComputeHighLows( begin, end, std::cout );
  }

//-----------------------------------------------------------------------------
template <class IteratorType>
bool ContourWorker::contour(double v,
             const IteratorType begin,
             const IteratorType end)
{
  const std::size_t totalSubGrids = this->volume->numSubGrids();
  for(std::size_t i=0; i < totalSubGrids; ++i)
    {
    if(this->volume->isValidSubGrid(i, v))
      {
      //need a way to get the output packed tri's
      this->volume->ContourSubGrid( v, i, begin, end, std::cout );
      emit this->computed(i);
      }
    }
  return true;
}

//-----------------------------------------------------------------------------
void ContourWorker::run()
{
  if (input)
    {
    vtkDataArray* inScalars = input->GetPointData()->GetScalars();
    switch (inScalars->GetDataType())
      {
      temDataArrayIteratorMacro( inScalars,
                           this->createSearchStructure(vtkDABegin, vtkDAEnd) );
      default:
        break;
      }
    }
}

//-----------------------------------------------------------------------------
void ContourWorker::computeContour(double isoValue)
{
  if (input)
    {
    vtkDataArray* inScalars = input->GetPointData()->GetScalars();
    switch (inScalars->GetDataType())
      {
      temDataArrayIteratorMacro(inScalars,
                              this->contour(isoValue, vtkDABegin, vtkDAEnd));
      default:
        break;
      }
    }
}


//-----------------------------------------------------------------------------
ModuleAccelContour::ModuleAccelContour(QObject* parentObject) :
  Superclass(parentObject)
{
}

//-----------------------------------------------------------------------------
ModuleAccelContour::~ModuleAccelContour()
{
  this->finalize();
}

//-----------------------------------------------------------------------------
QIcon ModuleAccelContour::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqIsosurface24.png");
}

//-----------------------------------------------------------------------------
bool ModuleAccelContour::initialize(vtkSMSourceProxy* source, vtkSMViewProxy* view)
{
  if (!this->Superclass::initialize(source, view))
    {
    return false;
    }

  //get the input and connect to the threaded contour worker
  vtkTrivialProducer *t = vtkTrivialProducer::SafeDownCast(source->GetClientSideObject());
  vtkImageData *data = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));

  this->Worker.input = data;

  this->Worker.start();
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleAccelContour::finalize()
{
  return true;
}

//-----------------------------------------------------------------------------
bool ModuleAccelContour::setVisibility(bool val)
{
  return true;
}

} // end of namespace TEM
