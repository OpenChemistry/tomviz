/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModulePlot_h
#define tomvizModulePlot_h

#include "Module.h"

class vtkCallbackCommand;
class vtkChartXY;
class vtkPlot;
class vtkPVContextView;
class vtkTable;
class vtkTrivialProducer;


namespace tomviz {

class MoleculeSource;
class OperatorResult;

class ModulePlot : public Module
{
  Q_OBJECT

public:
  ModulePlot(QObject* parent = nullptr);
  ~ModulePlot() override;

  QString label() const override { return "Plot"; }
  QIcon icon() const override;
  using Module::initialize;
  bool initialize(DataSource* data, vtkSMViewProxy* vtkView) override;
  bool initialize(MoleculeSource* data, vtkSMViewProxy* vtkView) override;
  bool initialize(OperatorResult* result, vtkSMViewProxy* view) override;
  bool finalize() override;
  bool setVisibility(bool val) override;
  bool visibility() const override;
  void addToPanel(QWidget*) override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QString exportDataTypeString() override { return ""; }
  vtkDataObject* dataToExport() override;

  void dataSourceMoved(double newX, double newY, double newZ) override;
  void dataSourceRotated(double newX, double newY, double newZ) override;

private:
  static void onResultModified(vtkObject* caller, long unsigned int eventId, void* clientData, void*callData);
  void addAllPlots();
  void removeAllPlots();

  Q_DISABLE_COPY(ModulePlot)
  bool m_visible;
  vtkWeakPointer<vtkPVContextView> m_view;
  vtkNew<vtkCallbackCommand> m_result_modified_cb;
  vtkWeakPointer<vtkTable> m_table;
  vtkWeakPointer<vtkChartXY> m_chart;
  vtkWeakPointer<vtkTrivialProducer> m_producer;
  QList<vtkSmartPointer<vtkPlot>> m_plots;

};
} // namespace tomviz
#endif
