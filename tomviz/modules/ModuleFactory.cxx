/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleFactory.h"

#include "DataSource.h"
#include "ModuleClip.h"
#include "ModuleContour.h"
#include "ModuleMolecule.h"
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
        << "Contour"
        << "Volume"
        << "Threshold"
        << "Molecule"
        << "Clip";
  qSort(reply);
  return reply;
}

bool ModuleFactory::moduleApplicable(const QString& moduleName,
                                     DataSource* dataSource,
                                     vtkSMViewProxy* view)
{
  if (moduleName == "Molecule") {
    return false;
  }

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

bool ModuleFactory::moduleApplicable(const QString& moduleName,
                                     MoleculeSource* moleculeSource,
                                     vtkSMViewProxy* view)
{
  if (moleculeSource && view) {
    if (moduleName == "Molecule") {
      return true;
    }
  }
  return false;
}

Module* ModuleFactory::allocateModule(const QString& type)
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
    // Keep this to be able to open older state files.
    module = new ModuleSlice();
  } else if (type == "Threshold") {
    module = new ModuleThreshold();
  } else if (type == "Ruler") {
    module = new ModuleRuler();
  } else if (type == "Scale Cube") {
    module = new ModuleScaleCube();
  } else if (type == "Molecule") {
    module = new ModuleMolecule();
  } else if (type== "Clip") {
    module = new ModuleClip();
  }
  return module;
}

Module* ModuleFactory::createModule(const QString& type, DataSource* dataSource,
                                    vtkSMViewProxy* view)
{
  auto module = allocateModule(type);
  if (module) {
    // sanity check.
    Q_ASSERT(type == moduleType(module));
    if (dataSource == nullptr && view == nullptr) {
      // don't initialize module if args are NULL.
      return module;
    }

    bool success;
    success = module->initialize(dataSource, view);
    if (!success) {
      delete module;
      return nullptr;
    }
    pqView* pqview = tomviz::convert<pqView*>(view);
    pqview->render();
  }
  return module;
}

Module* ModuleFactory::createModule(const QString& type,
                                    MoleculeSource* moleculeSource,
                                    vtkSMViewProxy* view)
{
  auto module = allocateModule(type);
  if (module) {
    // sanity check.
    Q_ASSERT(type == moduleType(module));
    if (moleculeSource == nullptr && view == nullptr) {
      // don't initialize module if args are NULL.
      return module;
    }

    bool success;
    success = module->initialize(moleculeSource, view);
    if (!success) {
      delete module;
      return nullptr;
    }
    pqView* pqview = tomviz::convert<pqView*>(view);
    pqview->render();
  }
  return module;
}

Module* ModuleFactory::createModule(const QString& type, OperatorResult* result,
                                    vtkSMViewProxy* view)
{
  auto module = allocateModule(type);
  if (module) {
    // sanity check.
    Q_ASSERT(type == moduleType(module));
    if (result == nullptr && view == nullptr) {
      // don't initialize module if args are NULL.
      return module;
    }

    bool success;
    success = module->initialize(result, view);
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
  DataSource* d = nullptr;
  Module* mdl = ModuleFactory::createModule(type, d, nullptr);
  if (mdl) {
    icon = mdl->icon();
    delete mdl;
  }
  return icon;
}

const char* ModuleFactory::moduleType(const Module* module)
{
  // WARNING: to ensure the most useful result is returned from this
  // function, the if statements should be ordered so that child
  // classes are checked before parent classes. Otherwise, the module
  // type may be reported to be a class's parent.

  if (qobject_cast<const ModuleOutline*>(module)) {
    return "Outline";
  }
  if (qobject_cast<const ModuleContour*>(module)) {
    return "Contour";
  }
  if (qobject_cast<const ModuleVolume*>(module)) {
    return "Volume";
  }
  if (qobject_cast<const ModuleSlice*>(module)) {
    return "Slice";
  }
  if (qobject_cast<const ModuleThreshold*>(module)) {
    return "Threshold";
  }
  if (qobject_cast<const ModuleRuler*>(module)) {
    return "Ruler";
  }
  if (qobject_cast<const ModuleScaleCube*>(module)) {
    return "Scale Cube";
  }
  if (qobject_cast<const ModuleMolecule*>(module)) {
    return "Molecule";
  }
  if (qobject_cast<const ModuleClip*>(module)) {
    return "Clip";
  }
  return nullptr;
}

} // end of namespace tomviz
