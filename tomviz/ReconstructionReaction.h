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
#ifndef tomvizReconstructionReaction_h
#define tomvizReconstructionReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;

class ReconstructionReaction : public pqReaction
{
  Q_OBJECT

public:
  ReconstructionReaction(QAction* parent);
  ~ReconstructionReaction();

  void recon(DataSource* input = NULL);

protected:
  void updateEnableState();
  void onTriggered() { this->recon(); }

private:
  Q_DISABLE_COPY(ReconstructionReaction)
};
}

#endif
