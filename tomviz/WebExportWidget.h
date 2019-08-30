/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizWebExportWidget_h
#define tomvizWebExportWidget_h

#include <QDialog>
#include <QVariantMap>

#include "PythonUtilities.h"
#include "Utilities.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QWidget;

namespace tomviz {

class DataSource;
class SpinBox;

class WebExportWidget : public QDialog
{
  Q_OBJECT

public:
  WebExportWidget(QWidget* parent = nullptr);

  QMap<QString, QVariant> getKeywordArguments();

private slots:
  void onCancel();
  void onExport();
  void onTypeChange(int);

private:
  QVariantMap readSettings();
  void writeSettings(const QVariantMap& settings);
  void writeWidgetSettings();
  void restoreSettings();

  QCheckBox* m_keepData;
  QComboBox* m_exportType;
  QLineEdit* m_multiValue;
  QDialogButtonBox* m_buttonBox;
  QPushButton* m_helpButton;
  QPushButton* m_cancelButton;
  QPushButton* m_exportButton;
  QSpinBox* m_imageHeight;
  QSpinBox* m_imageWidth;
  QSpinBox* m_maxOpacity;
  QSpinBox* m_nbPhi;
  QSpinBox* m_nbTheta;
  QSpinBox* m_scale;
  QSpinBox* m_spanValue;
  QWidget* m_cameraGroup;
  QWidget* m_imageSizeGroup;
  QWidget* m_valuesGroup;
  QWidget* m_volumeExplorationGroup;
  QWidget* m_volumeResampleGroup;

  QVariantMap m_kwargs;
};
} // namespace tomviz

#endif // tomvizWebExportWidget
