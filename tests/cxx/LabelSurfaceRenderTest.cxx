/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QTest>
#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqServerResource.h>
#include <vtkCellData.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPVRenderView.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSMViewProxy.h>
#include <vtkOutputWindow.h>
#include <vtkSmartPointer.h>
#include <vtkStringOutputWindow.h>
#include <vtkUnsignedCharArray.h>
#include <vtkWindowToImageFilter.h>
#include <vtkActor.h>
#include <vtkMapper.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkProperty.h>
#include <vtkPropCollection.h>

#include "pipeline/InputPort.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/LabelMapData.h"
#include "pipeline/sinks/LabelMapSink.h"
#include "pipeline/sinks/LabelMapSurface.h"

#include <map>
#include <string>
#include <set>
#include <tuple>

using namespace tomviz;

namespace {

// Three blocks side by side along x, labels 1, 2, 3, in a 24^3 volume.
vtkSmartPointer<vtkImageData> threeBlocks()
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(24, 24, 24);
  image->AllocateScalars(VTK_INT, 1);
  for (int z = 0; z < 24; ++z) {
    for (int y = 0; y < 24; ++y) {
      for (int x = 0; x < 24; ++x) {
        int v = 0;
        if (z >= 6 && z < 18 && y >= 6 && y < 18) {
          if (x >= 2 && x < 8) {
            v = 1;
          } else if (x >= 9 && x < 15) {
            v = 2;
          } else if (x >= 16 && x < 22) {
            v = 3;
          }
        }
        *static_cast<int*>(image->GetScalarPointer(x, y, z)) = v;
      }
    }
  }
  return image;
}

std::set<std::tuple<int, int, int>> distinctCellColors(vtkPolyData* surface)
{
  std::set<std::tuple<int, int, int>> colors;
  auto* arr = vtkUnsignedCharArray::SafeDownCast(
    surface->GetCellData()->GetArray(pipeline::kLabelColorsArrayName));
  if (!arr) {
    return colors;
  }
  for (vtkIdType c = 0; c < arr->GetNumberOfTuples(); ++c) {
    unsigned char rgb[3];
    arr->GetTypedTuple(c, rgb);
    colors.insert({ rgb[0], rgb[1], rgb[2] });
  }
  return colors;
}

} // namespace

class LabelSurfaceRenderTest : public QObject
{
  Q_OBJECT

public:
  explicit LabelSurfaceRenderTest(pipeline::Pipeline* pipeline)
    : m_pip(pipeline)
  {
  }

private slots:
  // A CI runner has no GPU and no software GL. There, a render window
  // that fails to load its OpenGL functions crashes when torn down, and
  // the view's window gets initialized as soon as the sink draws into
  // it, so the check has to come before any view exists. It is done
  // with a throwaway window of its own, watching what VTK says while
  // that window is created and initialized: vtkRenderWindow::New()
  // tries the backends in turn and each one that fails says so as it
  // goes (no X server, no EGL device, no OSMesa, no pixel format on
  // Windows), before any observer could be attached, and a healthy
  // window is created without a word. SupportsOpenGL() is not enough:
  // on the EGL runner it reports support despite all that.
  void initTestCase()
  {
    vtkNew<vtkStringOutputWindow> capture;
    auto* previous = vtkOutputWindow::GetInstance();
    previous->Register(nullptr);
    vtkOutputWindow::SetInstance(capture);
    {
      vtkNew<vtkRenderWindow> probe;
      probe->SetOffScreenRendering(1);
      probe->Initialize();
    }
    vtkOutputWindow::SetInstance(previous);
    previous->Delete();
    const std::string said = capture->GetOutput();
    for (const char* complaint : { "OpenGL", "X server", "EGL", "OSMesa",
                                   "pixel format", "PixelFormat" }) {
      if (said.find(complaint) != std::string::npos) {
        QSKIP(qPrintable(QString("no OpenGL available for an offscreen "
                                 "render: %1")
                           .arg(QString::fromStdString(said).trimmed()
                                  .left(160))));
      }
    }
  }

  void init()
  {
    auto* server = pqActiveObjects::instance().activeServer();
    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    m_view = qobject_cast<pqRenderView*>(
      builder->createView(pqRenderView::renderViewType(), server));
    QVERIFY(m_view);
  }

  void cleanup()
  {
    m_pip->clear();
    pqApplicationCore::instance()->getObjectBuilder()->destroy(m_view);
    m_view = nullptr;
  }

  void surfaceCarriesOneColorPerLabel()
  {
    // Payloads travel as VolumeDataPtr (exact-type any_cast).
    auto labelData = std::make_shared<pipeline::LabelMapData>(threeBlocks());
    labelData->refreshLabels();
    pipeline::VolumeDataPtr vol = labelData;
    auto* source = new pipeline::SourceNode();
    source->addOutput("volume", pipeline::PortType::Volume);
    m_pip->addNode(source);
    source->setOutputData(
      "volume", pipeline::PortData(vol, pipeline::PortType::Volume));

    auto* sink = new pipeline::LabelMapSink();
    sink->setLabel("labels");
    sink->initialize(m_view->getViewProxy());
    m_pip->addNode(sink);
    m_pip->createLink(source->outputPorts()[0], sink->inputPorts()[0]);
    sink->execute();
    QCOMPARE(sink->representation(),
             pipeline::LabelMapSink::Representation::Surface);

    auto* surface = sink->surface();
    QVERIFY(surface);
    QVERIFY(surface->GetNumberOfCells() > 0);
    const auto cellColors = distinctCellColors(surface);
    QCOMPARE(int(cellColors.size()), 3);

    // Now actually render and read back the pixels, with the sink's
    // own actor (the one it put in the view's renderer) in a plain
    // offscreen window, since the pq view does not draw under the
    // offscreen QPA.
    auto* rv = vtkPVRenderView::SafeDownCast(
      m_view->getViewProxy()->GetClientSideView());
    QVERIFY(rv);
    vtkActor* actor = nullptr;
    auto* props = rv->GetRenderer()->GetViewProps();
    vtkCollectionSimpleIterator it;
    props->InitTraversal(it);
    while (auto* prop = props->GetNextProp(it)) {
      auto* a = vtkActor::SafeDownCast(prop);
      if (a && a->GetMapper() && a->GetMapper()->GetInput() == surface) {
        actor = a;
      }
    }
    QVERIFY2(actor, "sink's surface actor not found in the renderer");

    vtkNew<vtkRenderer> renderer;
    vtkNew<vtkRenderWindow> window;
    window->SetOffScreenRendering(1);
    window->AddRenderer(renderer);
    renderer->AddActor(actor);
    renderer->SetBackground(0, 0, 0);
    window->SetSize(300, 300);
    renderer->ResetCamera();
    window->Render();

    vtkNew<vtkWindowToImageFilter> grab;
    grab->SetInput(window);
    grab->ReadFrontBufferOff();
    grab->Update();
    auto* img = grab->GetOutput();
    auto* px = vtkUnsignedCharArray::SafeDownCast(
      img->GetPointData()->GetScalars());
    QVERIFY(px);
    // Bucket pixels by dominant channel to count the hues on screen.
    std::map<int, int> dominant;
    for (vtkIdType i = 0; i < px->GetNumberOfTuples(); ++i) {
      unsigned char rgb[4] = { 0, 0, 0, 0 };
      px->GetTypedTuple(i, rgb);
      int mx = std::max({ rgb[0], rgb[1], rgb[2] });
      int mn = std::min({ rgb[0], rgb[1], rgb[2] });
      if (mx - mn < 40) {
        continue; // background / grey
      }
      int which = rgb[0] == mx ? 0 : (rgb[1] == mx ? 1 : 2);
      dominant[which]++;
    }
    // With three palette colors at least two hues must dominate
    // somewhere in the frame.
    int hues = 0;
    for (auto& kv : dominant) {
      if (kv.second > 50) {
        ++hues;
      }
    }
    QVERIFY2(hues >= 2, "surface rendered in a single hue");
  }

private:
  pipeline::Pipeline* m_pip;
  pqRenderView* m_view = nullptr;
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  pipeline::Pipeline pipeline;
  LabelSurfaceRenderTest tc(&pipeline);
  return QTest::qExec(&tc, argc, argv);
}

#include "LabelSurfaceRenderTest.moc"
