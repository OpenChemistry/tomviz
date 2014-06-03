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

#ifndef __DataSetConverters_h
#define __DataSetConverters_h

#include <dax/cont/ArrayHandle.h>

//supported iso values array types
#include "vtkDataSetAttributes.h"
#include "vtkCharArray.h"
#include "vtkFloatArray.h"
#include "vtkIntArray.h"
#include "vtkShortArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkUnsignedCharArray.h"


#include "vtkCellArray.h"
#include "vtkNew.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"

namespace {

  vtkDataArray* make_vtkDataArray(float)
    { return vtkFloatArray::New(); }

  vtkDataArray* make_vtkDataArray(int)
    { return vtkIntArray::New(); }
  vtkDataArray* make_vtkDataArray(unsigned int)
    { return vtkUnsignedIntArray::New(); }

  vtkDataArray* make_vtkDataArray(short)
    { return vtkShortArray::New(); }
  vtkDataArray* make_vtkDataArray(unsigned short)
    { return vtkUnsignedShortArray::New(); }

  vtkDataArray* make_vtkDataArray(char)
    { return vtkCharArray::New(); }
  vtkDataArray* make_vtkDataArray(unsigned char)
    { return vtkUnsignedCharArray::New(); }

//------------------------------------------------------------------------------
template<typename GridType>
void convertPoints(GridType& grid, vtkPolyData* output)
{
  //we are dealing with a container type whose memory wasn't allocated by
  //vtk so we have to copy the data into a new vtk memory location just
  //to be safe.

  //determine amount of memory to allocate
  const vtkIdType num_points = grid.GetNumberOfPoints();

  //ask vtkToDax to allocate the vtkPoints so it gets the float vs double
  //settings correct
  vtkNew<vtkPoints> points;
  points->SetDataTypeToFloat();
  points->SetNumberOfPoints(num_points);

  dax::Vector3 *raw_pts = reinterpret_cast<dax::Vector3*>(
                                       points->GetData()->GetVoidPointer(0));

  //get the coord portal from the grid.
  typedef typename GridType::PointCoordinatesType::PortalConstControl
                                                                DaxPortalType;
  DaxPortalType daxPortal = grid.GetPointCoordinates().GetPortalConstControl();

  std::copy(daxPortal.GetIteratorBegin(),
            daxPortal.GetIteratorEnd(),
            raw_pts);
  output->SetPoints( points.GetPointer() );
}

//------------------------------------------------------------------------------
void setCells(vtkCellArray* cells, vtkPolyData* output, dax::CellTagVertex)
{
output->SetVerts(cells);
}

//------------------------------------------------------------------------------
void setCells(vtkCellArray* cells, vtkPolyData* output, dax::CellTagLine)
{
output->SetLines(cells);
}

//------------------------------------------------------------------------------
void setCells(vtkCellArray* cells, vtkPolyData* output, dax::CellTagTriangle)
{
output->SetPolys(cells);
}

//------------------------------------------------------------------------------
template<typename GridType>
void convertCells(GridType& grid, vtkPolyData* output)
{
  //determine the Cell type
  typedef dax::CellTraits<typename GridType::CellTag> CellTraits;

  //determine amount of memory to allocate
  const vtkIdType num_cells = grid.GetNumberOfCells();
  const vtkIdType alloc_size = grid.GetCellConnections().GetNumberOfValues();

  //ask the vtkToDax allocator to make us memory
  vtkNew<vtkCellArray> cells;
  cells->SetNumberOfCells(num_cells);
  cells->GetData()->SetNumberOfTuples(num_cells+alloc_size);

  typedef typename GridType::CellConnectionsType::PortalConstControl
                                                                DaxPortalType;
  DaxPortalType daxPortal = grid.GetCellConnections().GetPortalConstControl();

  vtkIdType* cellPointer = cells->GetPointer();
  vtkIdType daxIndex = 0;
  for(vtkIdType i=0; i < num_cells; ++i)
    {
    *cellPointer = CellTraits::NUM_VERTICES;
    ++cellPointer;
    for(vtkIdType j=0; j < CellTraits::NUM_VERTICES; ++j, ++cellPointer, ++daxIndex)
      {
      *cellPointer = daxPortal.Get( daxIndex );
      }
    }

  setCells(cells.GetPointer(),output,typename GridType::CellTag());
}

}

#endif
