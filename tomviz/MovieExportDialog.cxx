/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "MovieExportDialog.h"

#include "animations/AnimationSceneGuard.h"
#include "animations/ModuleAnimations.h"

#include "ActiveObjects.h"
#include "Utilities.h"

#include <pqActiveObjects.h>
#include <pqAnimationManager.h>
#include <pqAnimationProgressDialog.h>
#include <pqAnimationScene.h>
#include <pqApplicationCore.h>
#include <pqPVApplicationCore.h>
#include <pqProgressManager.h>
#include <pqRenderView.h>
#include <pqServer.h>
#include <pqServerManagerModel.h>
#include <pqTimeKeeper.h>

#include <vtkCompositeAnimationPlayer.h>
#include <vtkNew.h>
#include <vtkSMAnimationScene.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMProperty.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyListDomain.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkSMSaveAnimationProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QTimer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVBoxLayout>

namespace tomviz {

namespace {

QString findFFmpegExecutable()
{
  QString name = "ffmpeg";
#ifdef Q_OS_WIN
  name = "ffmpeg.exe";
#endif
  // Prefer an ffmpeg shipped next to the tomviz executable (bundled
  // installs and conda environments put both in the same bin dir).
  QDir appDir(QCoreApplication::applicationDirPath());
  if (appDir.exists(name)) {
    return appDir.absoluteFilePath(name);
  }
  QString found = QStandardPaths::findExecutable(name);
#ifdef Q_OS_MAC
  if (found.isEmpty()) {
    // Apps launched from Finder get a minimal PATH that excludes
    // Homebrew and MacPorts.
    found = QStandardPaths::findExecutable(
      name, { "/opt/homebrew/bin", "/usr/local/bin", "/opt/local/bin" });
  }
#endif
  return found;
}

} // anonymous namespace

class MovieExportDialog::Internal : public QObject
{
public:
  QPointer<MovieExportDialog> parent;

  QComboBox* format;
  QLineEdit* fileName;
  QComboBox* resolution;
  QSpinBox* customWidth;
  QSpinBox* customHeight;
  QLabel* customTimes;
  QSpinBox* frameRate;
  QComboBox* quality;
  QLabel* info;

  QString ffmpegPath;

  enum class Result
  {
    Success,
    Canceled,
    Error
  };

  Internal(MovieExportDialog* p) : QObject(p), parent(p)
  {
    p->setWindowTitle("Export Movie");
    ffmpegPath = findFFmpegExecutable();

    auto* layout = new QVBoxLayout(p);
    auto* form = new QFormLayout;
    layout->addLayout(form);

    format = new QComboBox(p);
    if (!ffmpegPath.isEmpty()) {
      format->addItem("MP4 video (H.264)", QString("mp4"));
    }
    if (writerPrototype("OggTheora")) {
      format->addItem("Ogg Theora video", QString("ogv"));
    }
    format->addItem("PNG image sequence", QString("png"));
    form->addRow("Format:", format);

    auto* fileRow = new QHBoxLayout;
    fileName = new QLineEdit(p);
    fileName->setText(defaultFileName());
    auto* browse = new QPushButton("Browse...", p);
    fileRow->addWidget(fileName);
    fileRow->addWidget(browse);
    form->addRow("File:", fileRow);

    auto* resolutionRow = new QHBoxLayout;
    resolution = new QComboBox(p);
    // QSize(0, 0) = keep the view's size; QSize(-1, -1) = custom. A
    // default-constructed QSize is (-1, -1), so it can't mark "current".
    resolution->addItem("Current view size", QSize(0, 0));
    resolution->addItem("1280 x 720", QSize(1280, 720));
    resolution->addItem("1920 x 1080", QSize(1920, 1080));
    resolution->addItem("2560 x 1440", QSize(2560, 1440));
    resolution->addItem("3840 x 2160", QSize(3840, 2160));
    resolution->addItem("Custom", QSize(-1, -1));
    customWidth = new QSpinBox(p);
    customWidth->setRange(16, 8192);
    customWidth->setValue(1920);
    customHeight = new QSpinBox(p);
    customHeight->setRange(16, 8192);
    customHeight->setValue(1080);
    resolutionRow->addWidget(resolution);
    resolutionRow->addWidget(customWidth);
    customTimes = new QLabel("x", p);
    resolutionRow->addWidget(customTimes);
    resolutionRow->addWidget(customHeight);
    form->addRow("Resolution:", resolutionRow);

    frameRate = new QSpinBox(p);
    frameRate->setRange(1, 120);
    frameRate->setValue(30);
    frameRate->setSuffix(" fps");
    form->addRow("Frame rate:", frameRate);

    quality = new QComboBox(p);
    quality->addItem("High");
    quality->addItem("Medium");
    quality->addItem("Low");
    form->addRow("Quality:", quality);

    info = new QLabel(p);
    layout->addWidget(info);

    if (ffmpegPath.isEmpty()) {
      auto* hint = new QLabel(
        "MP4 export requires the ffmpeg executable, which was not found.", p);
      hint->setWordWrap(true);
      layout->addWidget(hint);
    }

    auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, p);
    buttons->button(QDialogButtonBox::Ok)->setText("Export");
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, p, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, p, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, &Internal::browseForFile);
    connect(format, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::formatChanged);
    connect(resolution, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Internal::updateEnableStates);
    connect(frameRate, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Internal::refreshInfo);

    updateEnableStates();
    refreshInfo();
  }

  QString currentFormat() { return format->currentData().toString(); }

  QString defaultFileName()
  {
    QString dir =
      QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (dir.isEmpty()) {
      dir = QDir::homePath();
    }
    QString ext =
      format->count() > 0 ? format->itemData(0).toString() : "png";
    return QDir(dir).absoluteFilePath("tomviz-movie." + ext);
  }

  void formatChanged()
  {
    // Keep the file name's extension in sync with the format.
    QFileInfo fi(fileName->text());
    if (!fi.fileName().isEmpty()) {
      QString base = fi.completeBaseName();
      QString dir = fi.path();
      fileName->setText(
        QDir(dir).absoluteFilePath(base + "." + currentFormat()));
    }
    updateEnableStates();
  }

  void updateEnableStates()
  {
    bool custom = resolution->currentData().toSize() == QSize(-1, -1);
    customWidth->setVisible(custom);
    customTimes->setVisible(custom);
    customHeight->setVisible(custom);

    // An image sequence has no encoder settings.
    bool isVideo = currentFormat() != "png";
    quality->setEnabled(isVideo);
    frameRate->setEnabled(isVideo);
  }

  void browseForFile()
  {
    QString fmt = currentFormat();
    QString filter;
    if (fmt == "mp4") {
      filter = "MP4 video (*.mp4)";
    } else if (fmt == "ogv") {
      filter = "Ogg Theora video (*.ogv)";
    } else {
      filter = "PNG images (*.png)";
    }

    QString picked = QFileDialog::getSaveFileName(parent, "Export Movie",
                                                  fileName->text(), filter);
    if (picked.isEmpty()) {
      return;
    }
    if (QFileInfo(picked).suffix().isEmpty()) {
      picked += "." + fmt;
    }
    fileName->setText(picked);
  }

  vtkSMSessionProxyManager* proxyManager()
  {
    auto* server = pqActiveObjects::instance().activeServer();
    return server ? server->proxyManager() : nullptr;
  }

  vtkSMProxy* writerPrototype(const char* name)
  {
    auto* pxm = proxyManager();
    return pxm ? pxm->GetPrototypeProxy("animation_writers", name) : nullptr;
  }

  pqAnimationScene* scene()
  {
    return pqPVApplicationCore::instance()
      ->animationManager()
      ->getActiveScene();
  }

  // The active view isn't necessarily a render view (a Plot module
  // makes a chart view active); fall back to the first render view.
  pqRenderView* renderViewForExport()
  {
    if (auto* renderView = ActiveObjects::instance().activePqRenderView()) {
      return renderView;
    }
    auto* smModel = pqApplicationCore::instance()->getServerManagerModel();
    auto renderViews = smModel->findItems<pqRenderView*>();
    return renderViews.isEmpty() ? nullptr : renderViews.first();
  }

  int sceneFrameCount()
  {
    auto* scenePq = scene();
    if (!scenePq) {
      return 0;
    }
    auto* proxy = scenePq->getProxy();
    int playMode = vtkSMPropertyHelper(proxy, "PlayMode").GetAsInt();
    if (playMode == vtkCompositeAnimationPlayer::SNAP_TO_TIMESTEPS) {
      auto* tk = ActiveObjects::instance().activeTimeKeeper();
      return tk ? static_cast<int>(tk->getTimeSteps().size()) : 0;
    }
    return vtkSMPropertyHelper(proxy, "NumberOfFrames").GetAsInt();
  }

  void refreshInfo()
  {
    int frames = sceneFrameCount();
    double seconds =
      frameRate->value() > 0
        ? static_cast<double>(frames) / frameRate->value()
        : 0;
    info->setText(QString("%1 frames, %2 s at the current frame rate")
                    .arg(frames)
                    .arg(seconds, 0, 'f', 1));
  }

  int crf()
  {
    // Map the three quality steps onto sensible x264 CRF values:
    // 18 is visually near-lossless, 23 is the x264 default, 28 is
    // small-file territory.
    switch (quality->currentIndex()) {
      case 1:
        return 23;
      case 2:
        return 28;
      default:
        return 18;
    }
  }

  int oggQuality()
  {
    // vtkOggTheoraWriter quality is 0 (low) to 2 (high).
    switch (quality->currentIndex()) {
      case 1:
        return 1;
      case 2:
        return 0;
      default:
        return 2;
    }
  }

  Result doExport(QString& error)
  {
    QString fmt = currentFormat();
    QString target = fileName->text();

    auto* pqview = renderViewForExport();
    if (!pqview) {
      error = "No render view is available to export from.";
      return Result::Error;
    }
    auto* scenePq = scene();
    if (!scenePq) {
      error = "No animation scene is available.";
      return Result::Error;
    }

    auto* pxm = pqview->getServer()->proxyManager();
    vtkSmartPointer<vtkSMProxy> proxy;
    proxy.TakeReference(pxm->NewProxy("misc", "SaveAnimation"));
    auto* ahProxy = vtkSMSaveAnimationProxy::SafeDownCast(proxy.Get());
    if (!ahProxy) {
      error = "Failed to create the SaveAnimation helper proxy.";
      return Result::Error;
    }

    // MP4 renders PNG frames into a temporary directory first; the
    // other formats write the target file directly.
    QTemporaryDir tempDir;
    QString writeTarget = target;
    if (fmt == "mp4") {
      if (!tempDir.isValid()) {
        error = "Failed to create a temporary directory for frames.";
        return Result::Error;
      }
      writeTarget = tempDir.path() + "/frame.png";
    }

    vtkNew<vtkSMParaViewPipelineController> controller;
    controller->PreInitializeProxy(ahProxy);
    vtkSMPropertyHelper(ahProxy, "View").Set(pqview->getRenderViewProxy());
    vtkSMPropertyHelper(ahProxy, "AnimationScene").Set(scenePq->getProxy());
    ahProxy->UpdateDefaultsAndVisibilities(writeTarget.toUtf8().data());
    controller->PostInitializeProxy(ahProxy);

    int res[2];
    vtkSMPropertyHelper resolutionHelper(ahProxy, "ImageResolution");
    resolutionHelper.Get(res, 2);
    QSize selected = resolution->currentData().toSize();
    if (selected == QSize(-1, -1)) {
      selected = QSize(customWidth->value(), customHeight->value());
    }
    if (!selected.isEmpty()) {
      res[0] = selected.width();
      res[1] = selected.height();
    }
    if (fmt == "mp4") {
      // yuv420p subsampling needs even dimensions.
      res[0] -= res[0] % 2;
      res[1] -= res[1] % 2;
    }
    resolutionHelper.Set(res, 2);

    vtkSMPropertyHelper(ahProxy, "FrameRate").Set(frameRate->value());

    // Writer-specific options live on the format proxies in the
    // "Format" proxy-list domain; WriteAnimation picks the proxy
    // matching the file extension from that same list.
    auto* pld =
      ahProxy->GetProperty("Format")->FindDomain<vtkSMProxyListDomain>();
    if (pld) {
      if (auto* png = pld->FindProxy("animation_writers", "PNG")) {
        // A fixed-width counter the ffmpeg input pattern can match. The
        // suffix is a printf format (the writer's own default is ".%04d");
        // a brace-style one is not expanded, it is written literally, so
        // every frame would land on the same file.
        vtkSMPropertyHelper(png, "SuffixFormat", true).Set(".%06d");
        png->UpdateVTKObjects();
      }
      if (fmt == "ogv") {
        if (auto* ogv = pld->FindProxy("animation_writers", "OggTheora")) {
          vtkSMPropertyHelper(ogv, "Quality", true).Set(oggQuality());
          ogv->UpdateVTKObjects();
        }
      }
    }
    ahProxy->UpdateVTKObjects();

    bool canceled = false;
    {
      pqAnimationProgressDialog progress(
        fmt == "mp4" ? "Rendering frames..." : "Saving animation...", "Abort",
        0, 100, parent);
      progress.setWindowTitle("Export Movie");
      progress.setAnimationScene(scenePq);
      progress.show();

      // pqProgressManager blocks interaction while progress events are
      // pending; unblock so the dialog's abort button stays clickable
      // (same dance as pqSaveAnimationReaction).
      auto* pgm = pqApplicationCore::instance()->getProgressManager();
      const auto prev = pgm->unblockEvents(true);
      // Let the render guard size every frame for the most expensive point
      // of the animation. Tracking the cost faithfully would be sharper on
      // average but would change sharpness between frames, which reads as
      // the picture breathing once the frames are played back.
      ModuleAnimations::instance().pinExportQuality();
      bool ok = ahProxy->WriteAnimation(writeTarget.toUtf8().data());
      ModuleAnimations::instance().releaseExportQuality();
      pgm->unblockEvents(prev);

      canceled = progress.wasCanceled();
      progress.hide();
      if (!canceled && !ok) {
        error = "Failed to render the animation frames.";
        return Result::Error;
      }
    }
    if (canceled) {
      return Result::Canceled;
    }

    if (fmt != "mp4") {
      return Result::Success;
    }

    // Match the counter width exactly, so a naming mismatch is reported
    // here rather than as an ffmpeg failure further down.
    int numFrames =
      QDir(tempDir.path())
        .entryList({ "frame.??????.png" }, QDir::Files)
        .size();
    if (numFrames == 0) {
      error = "No animation frames were rendered.";
      return Result::Error;
    }

    return encodeMp4(tempDir.path(), numFrames, target, error);
  }

  Result encodeMp4(const QString& framesDir, int numFrames,
                   const QString& target, QString& error)
  {
    QStringList args;
    args << "-y" << "-framerate" << QString::number(frameRate->value())
         << "-start_number" << "0"
         << "-i" << framesDir + "/frame.%06d.png"
         << "-c:v" << "libx264"
         << "-preset" << "medium"
         << "-crf" << QString::number(crf())
         << "-pix_fmt" << "yuv420p"
         << "-movflags" << "+faststart"
         << "-nostats"
         << "-progress" << "pipe:1"
         << target;

    QProgressDialog progress("Encoding movie...", "Abort", 0, numFrames,
                             parent);
    progress.setWindowTitle("Export Movie");
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    QProcess process;
    connect(&process, &QProcess::readyReadStandardOutput, this,
            [&process, &progress, numFrames]() {
              while (process.canReadLine()) {
                QByteArray line = process.readLine().trimmed();
                if (line.startsWith("frame=")) {
                  bool ok = false;
                  int frame = line.mid(6).toInt(&ok);
                  if (ok) {
                    progress.setValue(qMin(frame, numFrames));
                  }
                }
              }
            });

    bool canceled = false;
    connect(&progress, &QProgressDialog::canceled, this,
            [&process, &canceled]() {
              canceled = true;
              process.kill();
            });

    QEventLoop loop;
    connect(&process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            &loop, &QEventLoop::quit);

    process.start(ffmpegPath, args);
    if (!process.waitForStarted(5000)) {
      error = QString("Failed to start ffmpeg at \"%1\".").arg(ffmpegPath);
      return Result::Error;
    }
    // QProcess::finished is only delivered from the event loop, so it
    // cannot fire between start() and exec() above; no missed-quit race.
    loop.exec();
    progress.reset();

    if (canceled) {
      QFile::remove(target);
      return Result::Canceled;
    }
    if (process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
      QString tail =
        QString::fromLocal8Bit(process.readAllStandardError()).right(600);
      error = "ffmpeg failed to encode the movie:\n" + tail;
      QFile::remove(target);
      return Result::Error;
    }
    return Result::Success;
  }
};

MovieExportDialog::MovieExportDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
}

MovieExportDialog::~MovieExportDialog() = default;

void MovieExportDialog::showEvent(QShowEvent* e)
{
  QDialog::showEvent(e);
  m_internal->refreshInfo();
}

void MovieExportDialog::exportMovie(QWidget* parent)
{
  auto* scene =
    pqPVApplicationCore::instance()->animationManager()->getActiveScene();
  auto* proxy = scene ? scene->getProxy() : nullptr;
  auto* object =
    proxy ? vtkSMAnimationScene::SafeDownCast(proxy->GetClientSideObject())
          : nullptr;
  if (object && object->GetInPlay()) {
    interruptAnimationPlayback(/*rewind=*/false);
    QObject* context = parent ? static_cast<QObject*>(parent) : qApp;
    QTimer::singleShot(0, context, [parent]() { exportMovie(parent); });
    return;
  }
  MovieExportDialog dialog(parent);
  dialog.exec();
}

void MovieExportDialog::accept()
{
  QString target = m_internal->fileName->text().trimmed();
  if (target.isEmpty()) {
    QMessageBox::warning(this, "Export Movie", "Choose an output file.");
    return;
  }
  QFileInfo fi(target);
  if (!fi.dir().exists()) {
    QMessageBox::warning(this, "Export Movie",
                         QString("The directory \"%1\" does not exist.")
                           .arg(fi.dir().absolutePath()));
    return;
  }
  if (fi.exists()) {
    auto answer = QMessageBox::question(
      this, "Export Movie",
      QString("\"%1\" already exists. Overwrite it?").arg(fi.fileName()));
    if (answer != QMessageBox::Yes) {
      return;
    }
  }

  QString error;
  auto result = m_internal->doExport(error);
  switch (result) {
    case Internal::Result::Success:
      QDialog::accept();
      break;
    case Internal::Result::Canceled:
      // The user aborted; leave the dialog open with their settings.
      break;
    case Internal::Result::Error:
      QMessageBox::critical(this, "Export Movie", error);
      break;
  }
}

} // namespace tomviz
