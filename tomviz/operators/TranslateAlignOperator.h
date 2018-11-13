/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTranslateAlignOperator_h
#define tomvizTranslateAlignOperator_h

#include "Operator.h"

#include "vtkVector.h"

#include <QPointer>
#include <QVector>

namespace tomviz {

class DataSource;

class TranslateAlignOperator : public Operator
{
  Q_OBJECT

public:
  TranslateAlignOperator(DataSource* dataSource, QObject* parent = nullptr);

  QString label() const override { return "Translation Align"; }
  QIcon icon() const override;
  Operator* clone() const override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent, vtkSmartPointer<vtkImageData> data) override;

  void setAlignOffsets(const QVector<vtkVector2i>& offsets);
  void setDraftAlignOffsets(const QVector<vtkVector2i>& offsets);
  const QVector<vtkVector2i>& getAlignOffsets() const { return offsets; }
  const QVector<vtkVector2i>& getDraftAlignOffsets() const
  {
    return m_draftOffsets;
  }

  DataSource* getDataSource() const { return this->dataSource; }

  bool hasCustomUI() const override { return true; }

protected:
  bool applyTransform(vtkDataObject* data) override;
  void offsetsToResult();
  void initializeResults();

private:
  QVector<vtkVector2i> offsets;
  QVector<vtkVector2i> m_draftOffsets;
  const QPointer<DataSource> dataSource;
};
} // namespace tomviz

#endif
