/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimationHelperDialog.h"
#include "ui_AnimationHelperDialog.h"

#include "ActiveObjects.h"
#include "ContourAnimation.h"
#include "ModuleManager.h"
#include "Utilities.h"

#include <pqAnimationCue.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqPVApplicationCore.h>
#include <pqPropertyLinks.h>
#include <pqRenderView.h>
#include <pqSMAdaptor.h>

#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

#include <QDebug>

namespace tomviz {

class AnimationHelperDialog::Internal : public QObject
{
public:
  Ui::AnimationHelperDialog ui;
  pqPropertyLinks pqLinks;
  QPointer<AnimationHelperDialog> parent;
  QList<QPointer<ModuleAnimation>> moduleAnimations;

  Internal(AnimationHelperDialog* p) : QObject(p), parent(p)
  {
    // Must call setupUi() before using p in any way
    ui.setupUi(p);

    // We will change tabs automatically
    ui.modulesTabWidget->tabBar()->hide();

    updateGui();
    setupConnections();
  }

  void setupConnections()
  {
    // Camera animations
    connect(ui.clearCameraAnimations, &QPushButton::clicked, this,
            &Internal::clearCameraAnimations);
    connect(ui.createCameraOrbit, &QPushButton::clicked, this,
            &Internal::createCameraOrbitInternal);

    // Time series
    connect(&activeObjects(),
            &ActiveObjects::timeSeriesAnimationsEnableStateChanged,
            ui.enableTimeSeriesAnimations, &QCheckBox::setChecked);
    connect(ui.enableTimeSeriesAnimations, &QCheckBox::toggled,
            &activeObjects(), &ActiveObjects::enableTimeSeriesAnimations);
    connect(ui.enableTimeSeriesAnimations, &QCheckBox::toggled, this,
            [this](bool b) {
              updateEnableStates();
              if (b) {
                play();
              }
            });

    // Modules
    connect(&moduleManager(), &ModuleManager::dataSourceAdded, this,
            &Internal::onDataSourceAdded);
    connect(&moduleManager(), &ModuleManager::dataSourceRemoved, this,
            &Internal::onDataSourceRemoved);
    connect(ui.selectedDataSource,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::selectedDataSourceChanged);
    connect(&moduleManager(), &ModuleManager::moduleAdded, this,
            &Internal::updateModuleOptions);
    connect(&moduleManager(), &ModuleManager::moduleRemoved, this,
            &Internal::updateModuleOptions);
    connect(ui.selectedModule,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::selectedModuleChanged);
    connect(ui.addModuleAnimation, &QPushButton::clicked, this,
            &Internal::addModuleAnimation);
    connect(ui.clearModuleAnimations, &QPushButton::clicked, this,
            &Internal::clearModuleAnimations);

    // All animations
    pqLinks.addPropertyLink(ui.numberOfFrames, "value",
                            SIGNAL(valueChanged(int)), scene()->getProxy(),
                            scene()->getProxy()->GetProperty("NumberOfFrames"),
                            0);
    connect(ui.numberOfFrames, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &Internal::numberOfFramesModified);
    connect(ui.clearAllAnimations, &QPushButton::clicked, this,
            &Internal::clearAllAnimations);
  }

  void play() { scene()->getProxy()->InvokeCommand("Play"); }

  void updateGui()
  {
    ui.enableTimeSeriesAnimations->setChecked(
      activeObjects().timeSeriesAnimationsEnabled());

    updateDataSourceOptions();
    updateEnableStates();
  }

  void updateEnableStates()
  {
    bool hasCameraAnimations = false;
    for (auto* cue : scene()->getCues()) {
      if (cue->getSMName().startsWith("CameraAnimationCue")) {
        hasCameraAnimations = true;
        break;
      }
    }

    bool hasTimeSeries = false;
    for (auto* ds : moduleManager().allDataSources()) {
      if (ds->hasTimeSteps()) {
        hasTimeSeries = true;
        break;
      }
    }

    bool timeSeriesEnabled =
      ui.enableTimeSeriesAnimations->isChecked() && hasTimeSeries;

    bool hasDataSourceOptions = ui.selectedDataSource->count() != 0;
    bool hasModuleOptions = ui.selectedModule->count() != 0;
    bool moduleSelected = selectedModule() != nullptr;
    bool hasModuleAnimations = !moduleAnimations.empty();

    bool hasAnyAnimations =
      hasCameraAnimations || timeSeriesEnabled || hasModuleAnimations;

    ui.clearCameraAnimations->setEnabled(hasCameraAnimations);
    ui.enableTimeSeriesAnimations->setEnabled(hasTimeSeries);
    ui.addModuleAnimation->setEnabled(moduleSelected);
    ui.selectedDataSource->setEnabled(hasDataSourceOptions);
    ui.selectedModule->setEnabled(hasModuleOptions);
    ui.clearModuleAnimations->setEnabled(hasModuleAnimations);
    ui.clearAllAnimations->setEnabled(hasAnyAnimations);
  }

  // Camera
  void clearCameraAnimations()
  {
    clearCameraCues();
    updateEnableStates();
  }

  void createCameraOrbitInternal()
  {
    auto* renderView = activeObjects().activePqRenderView();

    // Remove all previous camera cues, and create the orbit
    clearCameraCues(renderView->getRenderViewProxy());
    createCameraOrbit(renderView->getRenderViewProxy());

    updateEnableStates();
    play();
  }

  QStringList moduleTabTexts()
  {
    QStringList types;
    for (int i = 0; i < ui.modulesTabWidget->count(); ++i) {
      types.append(ui.modulesTabWidget->tabText(i));
    }
    return types;
  }

  QStringList allowedModuleTypes()
  {
    // This is based upon tab texts
    return moduleTabTexts();
  }

  // Data sources
  void updateDataSourceOptions()
  {
    QSignalBlocker blocked(ui.selectedDataSource);
    auto* previouslySelected = selectedDataSource();
    int previouslySelectedIndex = -1;

    ui.selectedDataSource->clear();

    auto& manager = moduleManager();

    auto dataSources = manager.allDataSourcesDepthFirst();
    auto labels = manager.createUniqueLabels(dataSources);
    for (int i = 0; i < dataSources.size(); ++i) {
      auto* dataSource = dataSources[i];
      auto label = labels[i];

      QVariant data;
      data.setValue(dataSource);
      ui.selectedDataSource->addItem(label, data);

      if (dataSource == previouslySelected) {
        previouslySelectedIndex = i;
      }
    }

    if (previouslySelectedIndex != -1) {
      ui.selectedDataSource->setCurrentIndex(previouslySelectedIndex);
    } else {
      selectedDataSourceChanged();
    }

    updateEnableStates();
  }

  DataSource* selectedDataSource()
  {
    if (ui.selectedDataSource->count() == 0) {
      return nullptr;
    }

    return ui.selectedDataSource->currentData().value<DataSource*>();
  }

  // Modules
  void updateModuleOptions()
  {
    QSignalBlocker blocked(ui.selectedModule);
    auto* previouslySelected = selectedModule();
    int previouslySelectedIndex = -1;

    ui.selectedModule->clear();

    auto* dataSource = selectedDataSource();
    if (!dataSource) {
      return;
    }

    auto& manager = moduleManager();

    QStringList labels;
    QList<Module*> modules;
    auto allowedTypes = allowedModuleTypes();
    for (auto* module : manager.findModulesGeneric(dataSource, nullptr)) {
      if (allowedTypes.contains(module->label())) {
        modules.append(module);

        int i = 1;
        auto label = module->label();
        while (labels.contains(label)) {
          label = module->label() + " " + QString::number(++i);
        }
        labels.append(label);
      }
    }

    for (int i = 0; i < modules.size(); ++i) {
      auto* module = modules[i];
      auto label = labels[i];

      QVariant data;
      data.setValue(module);
      ui.selectedModule->addItem(label, data);

      if (module == previouslySelected) {
        previouslySelectedIndex = i;
      }
    }

    if (previouslySelectedIndex != -1) {
      ui.selectedModule->setCurrentIndex(previouslySelectedIndex);
    } else {
      selectedModuleChanged();
    }

    updateEnableStates();
  }

  Module* selectedModule()
  {
    if (ui.selectedModule->count() == 0) {
      return nullptr;
    }

    return ui.selectedModule->currentData().value<Module*>();
  }

  void selectedDataSourceChanged() { updateModuleOptions(); }

  void selectedModuleChanged()
  {
    // Show animation options for the selected module
    auto* module = selectedModule();
    auto tabIndex = 0;
    if (module) {
      tabIndex = moduleTabTexts().indexOf(module->label());
    }

    ui.modulesTabWidget->setCurrentIndex(tabIndex);

    if (qobject_cast<ModuleContour*>(module)) {
      setupContourTab();
    }

    updateEnableStates();
  }

  void onDataSourceAdded()
  {
    // This is done later because when the dataSourceAdded
    // signal gets emitted, the data source does not yet have a label,
    // but it gets added later. It appears to have the label, though,
    // if we simple post this to the event loop to be performed later.
    QTimer::singleShot(0, this, &Internal::updateDataSourceOptions);
    updateEnableStates();
  }

  void onDataSourceRemoved()
  {
    updateDataSourceOptions();
    updateEnableStates();
  }

  void setupContourTab()
  {
    auto* module = qobject_cast<ModuleContour*>(selectedModule());
    double range[2];
    module->isoRange(range);

    ui.contourStart->setMinimum(range[0]);
    ui.contourStart->setMaximum(range[1]);
    ui.contourStop->setMinimum(range[0]);
    ui.contourStop->setMaximum(range[1]);

    // Set reasonable default values
    ui.contourStart->setValue((range[1] - range[0]) / 3 + range[0]);
    ui.contourStop->setValue((range[1] - range[0]) * 2 / 3 + range[0]);
  }

  void addModuleAnimation()
  {
    auto* module = selectedModule();
    if (!module) {
      return;
    }

    for (int i = 0; i < moduleAnimations.size(); ++i) {
      if (module == moduleAnimations[i]->baseModule) {
        // We only allow one animation per module
        // Remove the old one
        moduleAnimations[i]->deleteLater();
        moduleAnimations.removeAt(i);
        --i;
      }
    }

    if (qobject_cast<ModuleContour*>(module)) {
      addContourAnimation();
    } else {
      qDebug() << "Unknown module type: " << module;
    }

    updateEnableStates();
    play();
  }

  void addContourAnimation()
  {
    auto start = ui.contourStart->value();
    auto stop = ui.contourStop->value();
    auto* module = qobject_cast<ModuleContour*>(selectedModule());
    moduleAnimations.append(new ContourAnimation(module, start, stop));
  }

  void clearModuleAnimations()
  {
    for (auto animation : moduleAnimations) {
      animation->deleteLater();
    }

    moduleAnimations.clear();

    updateEnableStates();
  }

  // All animations
  void numberOfFramesModified()
  {
    // The number of frames only makes sense if the play mode is a sequence.
    // If the user modified the number of frames, set the play mode to
    // sequence.
    pqSMAdaptor::setEnumerationProperty(
      scene()->getProxy()->GetProperty("PlayMode"), "Sequence");
  }

  void clearAllAnimations()
  {
    clearCameraAnimations();
    if (ui.enableTimeSeriesAnimations->isEnabled()) {
      ui.enableTimeSeriesAnimations->setChecked(false);
    }
    clearModuleAnimations();

    updateEnableStates();
  }

  ActiveObjects& activeObjects() { return ActiveObjects::instance(); }

  ModuleManager& moduleManager() { return ModuleManager::instance(); }

  pqAnimationScene* scene()
  {
    return pqPVApplicationCore::instance()
      ->animationManager()
      ->getActiveScene();
  }
}; // end class Internal

AnimationHelperDialog::AnimationHelperDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
}

AnimationHelperDialog::~AnimationHelperDialog() = default;

} // namespace tomviz
