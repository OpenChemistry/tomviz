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
#include "ModuleFactory.h"

#include "DataSource.h"
#include "ModuleContour.h"
#include "ModuleMolecule.h"
#include "ModuleOrthogonalSlice.h"
#include "ModuleOutline.h"
#include "ModuleRuler.h"
#include "ModuleScaleCube.h"
#include "ModuleSegment.h"
#include "ModuleSlice.h"
#include "ModuleThreshold.h"
#include "ModuleVolume.h"
#include "OperatorResult.h"
#include "Utilities.h"

#include <pqView.h>
#include <vtkSMViewProxy.h>

#include <QtAlgorithms>

namespace tomviz {

ModuleFactory::ModuleFactory() {}

ModuleFactory::~ModuleFactory() {}

QList<QString> ModuleFactory::moduleTypes()
{
  QList<QString> reply;
  reply << "Outline"
        << "Slice"
        << "Ruler"
        << "Scale Cube"
        << "Orthogonal Slice"
        << "Contour"
        << "Volume"
        << "Threshold";
  qSort(reply);
  return reply;
}

bool ModuleFactory::moduleApplicable(const QString& moduleName,
                                     DataSource* dataSource,
                                     vtkSMViewProxy* view)
{
  if (dataSource && view) {
    if (dataSource->getNumberOfComponents() > 1) {
      if (moduleName == "Contour" || moduleName == "Volume" ||
          moduleName == "Threshold") {
        return false;
      }
    }
    return true;
  }

  return false;
}

Module* ModuleFactory::createModule(const QString& type, DataSource* dataSource,
                                    vtkSMViewProxy* view,
                                    OperatorResult* result)
{
  Module* module = nullptr;
  if (type == "Outline") {
    module = new ModuleOutline();
  } else if (type == "Contour") {
    module = new ModuleContour();
  } else if (type == "Volume") {
    module = new ModuleVolume();
  } else if (type == "Slice") {
    module = new ModuleSlice();
  } else if (type == "Orthogonal Slice") {
    module = new ModuleOrthogonalSlice();
  } else if (type == "Threshold") {
    module = new ModuleThreshold();
  } else if (type == "Ruler") {
    module = new ModuleRuler();
  } else if (type == "Scale Cube") {
    module = new ModuleScaleCube();
  } else if (type == "Molecule") {
    module = new ModuleMolecule();
  }

  if (module) {
    // sanity check.
    Q_ASSERT(type == moduleType(module));
    if (dataSource == nullptr && view == nullptr) {
      // don't initialize module if args are NULL.
      return module;
    }

    bool success;
    if (result == nullptr) {
      success = module->initialize(dataSource, view);
    } else {
      success = module->initializeWithResult(dataSource, view, result);
    }

    if (!success) {
      delete module;
      return nullptr;
    }
    pqView* pqview = tomviz::convert<pqView*>(view);
    pqview->render();
  }
  return module;
}

QIcon ModuleFactory::moduleIcon(const QString& type)
{
  QIcon icon;
  Module* mdl = ModuleFactory::createModule(type, nullptr, nullptr);
  if (mdl) {
    icon = mdl->icon();
    delete mdl;
  }
  return icon;
}

const char* ModuleFactory::moduleType(Module* module)
{
  // WARNING: to ensure the most useful result is returned from this
  // function, the if statements should be ordered so that child
  // classes are checked before parent classes. Otherwise, the module
  // type may be reported to be a class's parent.

  if (qobject_cast<ModuleOutline*>(module)) {
    return "Outline";
  }
  if (qobject_cast<ModuleContour*>(module)) {
    return "Contour";
  }
  if (qobject_cast<ModuleVolume*>(module)) {
    return "Volume";
  }
  if (qobject_cast<ModuleSlice*>(module)) {
    return "Slice";
  }
  if (qobject_cast<ModuleOrthogonalSlice*>(module)) {
    return "Orthogonal Slice";
  }
  if (qobject_cast<ModuleThreshold*>(module)) {
    return "Threshold";
  }
  if (qobject_cast<ModuleRuler*>(module)) {
    return "Ruler";
  }
  if (qobject_cast<ModuleScaleCube*>(module)) {
    return "Scale Cube";
  }
  if (qobject_cast<ModuleMolecule*>(module)) {
    return "Molecule";
  }
  return nullptr;
}

} // end of namespace tomviz
