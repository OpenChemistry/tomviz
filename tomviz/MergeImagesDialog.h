/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMergeImagesDialog_h
#define tomvizMergeImagesDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class MergeImagesDialog;
}

namespace tomviz {

class MergeImagesDialog : public QDialog
{
  Q_OBJECT

public:
  MergeImagesDialog(QWidget* parent = nullptr);
  ~MergeImagesDialog() override;

  enum MergeMode : int
  {
    Arrays,
    Components
  };

  MergeMode getMode();

private:
  Q_DISABLE_COPY(MergeImagesDialog)
  QScopedPointer<Ui::MergeImagesDialog> m_ui;
};
} // namespace tomviz

#endif
