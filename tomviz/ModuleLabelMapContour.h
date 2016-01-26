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
#ifndef tomvizModuleLabelMapContour_h
#define tomvizModuleLabelMapContour_h

#include "ModuleContour.h"

#include "vtkWeakPointer.h"

class vtkSMSourceProxy;
class vtkSMProxy;

namespace tomviz
{

/// A module that shows contours around label maps in a segmented volume
class ModuleLabelMapContour : public ModuleContour
{
  Q_OBJECT

  typedef ModuleContour Superclass;

public:
  ModuleLabelMapContour(QObject* parent=nullptr);
  virtual ~ModuleLabelMapContour();

  QString label() const override;
  QIcon icon() const override;
  bool initialize(DataSource* dataSource, vtkSMViewProxy* view) override;
  bool finalize() override;
  void addToPanel(pqProxiesWidget*) override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;

protected:
  void updateColorMap() override;

private:
  Q_DISABLE_COPY(ModuleLabelMapContour)
  vtkWeakPointer<vtkSMSourceProxy> ResampleFilter;
  vtkWeakPointer<vtkSMProxy> LabelMapContourRepresentation;

};

} // namespace tomviz

#endif

