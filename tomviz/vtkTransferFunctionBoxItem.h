/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizvtkTransferFunctionBoxItem_h
#define tomvizvtkTransferFunctionBoxItem_h

#include <vtkBlockItem.h>

/**
 * \brief 
 */

class vtkTransferFunctionBoxItem : public vtkBlockItem
{
public:
  static vtkTransferFunctionBoxItem* New();
  vtkTransferFunctionBoxItem(const vtkTransferFunctionBoxItem&) = delete;
  void operator=(const vtkTransferFunctionBoxItem&) = delete;

protected:
  vtkTransferFunctionBoxItem();
  ~vtkTransferFunctionBoxItem() override;
};
#endif // tomvizvtkTransferFunctionBoxItem_h
