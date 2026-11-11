/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineTranslateAlignTransform_h
#define tomvizPipelineTranslateAlignTransform_h

#include "TransformNode.h"

#include <vtkVector.h>

#include <QVector>

class vtkImageData;

namespace tomviz {
namespace pipeline {

/// Transform that applies per-slice 2D translation offsets to align a
/// tilt series or image stack.
class TranslateAlignTransform : public TransformNode
{
  Q_OBJECT

public:
  TranslateAlignTransform(QObject* parent = nullptr);
  ~TranslateAlignTransform() override = default;

  void setAlignOffsets(const QVector<vtkVector2i>& offsets);
  void setDraftAlignOffsets(const QVector<vtkVector2i>& offsets);
  const QVector<vtkVector2i>& getAlignOffsets() const { return m_offsets; }
  const QVector<vtkVector2i>& getDraftAlignOffsets() const
  {
    return m_draftOffsets;
  }

  /// Offsets from the per-image shifts some readers keep with the data
  /// (the "alignment_shifts" field array: one (x, y) pair per image, in
  /// pixels of an image of "alignment_shifts_image_size", as the ZEISS
  /// reader stores the shifts ZEISS's software applies), rounded to whole
  /// pixels. Empty unless there is a finite pair for every image and the
  /// images are still that size.
  static QVector<vtkVector2i> offsetsFromAlignmentShifts(vtkImageData* image);

  bool hasPropertiesWidget() const override;
  EditNodeWidget* createPropertiesWidget(Pipeline* pipeline,
                                         QWidget* parent) override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

protected:
  QMap<QString, PortData> transform(
    const QMap<QString, PortData>& inputs) override;

private:
  QVector<vtkVector2i> m_offsets;
  QVector<vtkVector2i> m_draftOffsets;
};

} // namespace pipeline
} // namespace tomviz

#endif
