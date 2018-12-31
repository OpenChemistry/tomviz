/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizImageStackDialog_h
#define tomvizImageStackDialog_h

#include <QDialog>

#include "DataSource.h"
#include "ImageStackModel.h"

#include <QScopedPointer>

namespace Ui {

class ImageStackDialog;
}

namespace tomviz {
class ImageStackDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ImageStackDialog(QWidget* parent = nullptr);
  ~ImageStackDialog() override;

  void setStackSummary(const QList<ImageInfo>& summary, bool sort = true);
  void setStackType(const DataSource::DataSourceType& stackType);
  void processDirectory(const QString& path);
  void processFiles(const QStringList& fileNames);
  QList<ImageInfo> getStackSummary() const;
  DataSource::DataSourceType getStackType() const;

public slots:
  void onOpenFileClick();
  void onOpenFolderClick();
  void onImageToggled(int row, bool value);
  void onStackTypeChanged(int stackType);
  void onCheckSizesClick();

signals:
  void summaryChanged(const QList<ImageInfo>&);
  void stackTypeChanged(const DataSource::DataSourceType&);

protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

private:
  QScopedPointer<Ui::ImageStackDialog> m_ui;

  QList<ImageInfo> m_summary;
  DataSource::DataSourceType m_stackType;
  ImageStackModel m_tableModel;
  void openFileDialog(int mode);
  bool detectVolume(QStringList fileNames, QList<ImageInfo>& summary,
                    bool matchPrefix = true);
  bool detectTilt(QStringList fileNames, QList<ImageInfo>& summary,
                  bool matchPrefix = true);
  void defaultOrder(QStringList fileNames, QList<ImageInfo>& summary);
  QList<ImageInfo> initStackSummary(const QStringList& fileNames);
  static void getImageSize(QStringList fileNames, int iThread, int nThreads,
                           QList<ImageInfo>* summary);
  void checkStackSizes(QList<ImageInfo>& summary);
};
} // namespace tomviz

#endif
