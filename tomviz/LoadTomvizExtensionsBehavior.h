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
#ifndef tomvizLoadTomvizExtensionsBehavior_h
#define tomvizLoadTomvizExtensionsBehavior_h

#include <QObject>

class pqServer;

namespace tomviz
{

class LoadTomvizExtensionsBehavior : public QObject
{
  Q_OBJECT
public:
    LoadTomvizExtensionsBehavior(QObject* p);
    ~LoadTomvizExtensionsBehavior();

private slots:
  void onServerLoaded(pqServer*);

private:
  Q_DISABLE_COPY(LoadTomvizExtensionsBehavior)
};

}

#endif
