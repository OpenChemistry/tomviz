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
#include <QApplication>
#include "pqPVApplicationCore.h"
#include "MainWindow.h"

#include <clocale>

int main(int argc, char** argv)
{
  QCoreApplication::setApplicationName("TomViz");
  QCoreApplication::setApplicationVersion("0.1.0");
  QCoreApplication::setOrganizationName("Kitware");

  QApplication app(argc, argv);
  setlocale(LC_NUMERIC, "C");
  pqPVApplicationCore appCore(argc, argv);
  TEM::MainWindow window;
  window.show();
  return app.exec();
}
