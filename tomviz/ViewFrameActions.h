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

#ifndef tomvizViewFrameActions_h
#define tomvizViewFrameActions_h

#include <pqStandardViewFrameActionsImplementation.h>

namespace tomviz {

class ViewFrameActions : public pqStandardViewFrameActionsImplementation
{
  Q_OBJECT
  Q_INTERFACES(pqViewFrameActionsInterface)

public:
  explicit ViewFrameActions(QObject* parent = nullptr);

protected:
  QList<ViewType> availableViewTypes() override;
  bool isButtonVisible(const std::string& buttonName, pqView* view) override;

private:
  Q_DISABLE_COPY(ViewFrameActions)
};
} // namespace tomviz

#endif // tomvizViewFrameActions_h
