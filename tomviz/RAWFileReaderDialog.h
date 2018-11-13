/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRAWFileReaderDialog
#define tomvizRAWFileReaderDialog

#include <QDialog>
#include <QScopedPointer>

class vtkSMProxy;

namespace Ui {
class RAWFileReaderDialog;
}
namespace tomviz {

class RAWFileReaderDialog : public QDialog
{
  Q_OBJECT
public:
  RAWFileReaderDialog(vtkSMProxy* reader, QWidget* parent = nullptr);
  ~RAWFileReaderDialog();

  void dimensions(size_t*);
  int components();
  int vtkDataType();
private slots:
  void sanityCheckSize();
  void dataTypeChanged();
  void onAccepted();

private:
  QScopedPointer<Ui::RAWFileReaderDialog> m_ui;
  vtkSMProxy* m_reader;
  size_t m_filesize;

  bool isSigned(int vtkType);
  int vtkDataTypeToIndex(int vtkType);
};
} // namespace tomviz

#endif
