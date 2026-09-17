/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QTest>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqServer.h>
#include <pqServerResource.h>

#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPVRenderView.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>

#include "ActiveObjects.h"
#include "ImageViewerModeBehavior.h"
#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SinkGroupNode.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/OutlineSink.h"
#include "pipeline/sinks/SliceSink.h"
#include "pipeline/sinks/VolumeSink.h"

#include <cmath>

using namespace tomviz;

namespace {

const int MODE_2D = vtkPVRenderView::INTERACTION_MODE_2D;
const int MODE_3D = vtkPVRenderView::INTERACTION_MODE_3D;
const int MODE_SELECTION = vtkPVRenderView::INTERACTION_MODE_SELECTION;

pqRenderView* createRenderView()
{
  auto* server = pqActiveObjects::instance().activeServer();
  auto* builder = pqApplicationCore::instance()->getObjectBuilder();
  return qobject_cast<pqRenderView*>(
    builder->createView(pqRenderView::renderViewType(), server));
}

/// What the toolbar's 2D/3D button does.
void setInteractionMode(pqRenderView* view, int mode)
{
  vtkSMPropertyHelper(view->getProxy(), "InteractionMode").Set(mode);
  view->getProxy()->UpdateProperty("InteractionMode", 1);
}

vtkCamera* camera(pqRenderView* view)
{
  return view->getRenderViewProxy()->GetActiveCamera();
}

void placeCamera(pqRenderView* view, double px, double py, double pz,
                 double fx, double fy, double fz)
{
  auto* cam = camera(view);
  cam->SetPosition(px, py, pz);
  cam->SetFocalPoint(fx, fy, fz);
  cam->SetViewUp(0, 0, 1);
}

bool near(const double a[3], double x, double y, double z, double tol = 1e-6)
{
  return std::abs(a[0] - x) < tol && std::abs(a[1] - y) < tol &&
         std::abs(a[2] - z) < tol;
}

/// A source carrying a volume, with the SinkGroupNode sinks hang off.
struct Dataset
{
  pipeline::SourceNode* source = nullptr;
  pipeline::SinkGroupNode* group = nullptr;
  pipeline::OutputPort* port() const { return source->outputPorts()[0]; }
  pipeline::OutputPort* sinkPort() const { return group->outputPorts()[0]; }
};

Dataset addDataset(pipeline::Pipeline* pip, int nx, int ny, int nz)
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(nx, ny, nz);
  image->AllocateScalars(VTK_FLOAT, 1);
  auto vol = std::make_shared<pipeline::VolumeData>(
    vtkSmartPointer<vtkImageData>(image));

  Dataset ds;
  ds.source = new pipeline::SourceNode();
  ds.source->addOutput("volume", pipeline::PortType::Volume);
  pip->addNode(ds.source);
  ds.source->setOutputData(
    "volume", pipeline::PortData(vol, pipeline::PortType::Volume));

  ds.group = new pipeline::SinkGroupNode();
  ds.group->addPassthrough("volume", pipeline::PortType::ImageData);
  pip->addNode(ds.group);
  pip->createLink(ds.port(), ds.group->inputPorts()[0]);
  return ds;
}

template <typename Sink>
Sink* addSink(pipeline::Pipeline* pip, const Dataset& ds, pqRenderView* view)
{
  auto* sink = new Sink();
  sink->initialize(view->getViewProxy());
  pip->addNode(sink);
  pip->createLink(ds.sinkPort(), sink->inputPorts()[0]);
  return sink;
}

QList<pipeline::SliceSink*> slicesIn(pipeline::Pipeline* pip,
                                     pqRenderView* view)
{
  QList<pipeline::SliceSink*> slices;
  for (auto* node : pip->nodes()) {
    auto* slice = qobject_cast<pipeline::SliceSink*>(node);
    if (slice && slice->view() == view->getViewProxy()) {
      slices.append(slice);
    }
  }
  return slices;
}

} // namespace

class ImageViewerModeTest : public QObject
{
  Q_OBJECT

public:
  ImageViewerModeTest(ImageViewerModeBehavior* behavior,
                      pipeline::Pipeline* pipeline)
    : m_behavior(behavior), m_pip(pipeline)
  {}

private slots:
  void init()
  {
    m_view = createRenderView();
    QVERIFY(m_view);
  }

  void cleanup()
  {
    ActiveObjects::instance().clearActiveSelection();
    m_pip->clear();
    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    for (auto* view : m_extraViews) {
      builder->destroy(view);
    }
    m_extraViews.clear();
    builder->destroy(m_view);
    m_view = nullptr;
  }

  // The toolbar's 2D toggle hides everything but the slice, aims the
  // camera down the slice normal, and the 3D toggle puts it all back:
  // the sinks' visibility and the camera's position, direction and up.
  void twoDShowsOnlyTheSliceAndThreeDRestores()
  {
    auto ds = addDataset(m_pip, 20, 30, 40);
    auto* outline = addSink<pipeline::OutlineSink>(m_pip, ds, m_view);
    auto* volume = addSink<pipeline::VolumeSink>(m_pip, ds, m_view);
    auto* slice = addSink<pipeline::SliceSink>(m_pip, ds, m_view);
    slice->setDirection(pipeline::SliceSink::XZ);
    ActiveObjects::instance().setActivePort(ds.port());

    placeCamera(m_view, 5, 7, 9, 1, 2, 3);

    setInteractionMode(m_view, MODE_2D);
    QVERIFY(m_behavior->isActive(m_view));
    QCOMPARE(m_behavior->slice(m_view), slice);
    QVERIFY(!outline->visibility());
    QVERIFY(!volume->visibility());
    QVERIFY(slice->visibility());
    QVERIFY(!slice->showArrow());
    // Looking along +y for an XZ slice, z up
    double dir[3], up[3];
    camera(m_view)->GetDirectionOfProjection(dir);
    camera(m_view)->GetViewUp(up);
    QVERIFY2(near(dir, 0, 1, 0), "camera should look along +y");
    QVERIFY2(near(up, 0, 0, 1), "z should be up");
    QCOMPARE(camera(m_view)->GetParallelProjection(), 1);

    setInteractionMode(m_view, MODE_3D);
    QVERIFY(!m_behavior->isActive(m_view));
    QVERIFY(outline->visibility());
    QVERIFY(volume->visibility());
    QVERIFY(slice->visibility());
    QVERIFY(slice->showArrow());
    double pos[3], focal[3];
    camera(m_view)->GetPosition(pos);
    camera(m_view)->GetFocalPoint(focal);
    camera(m_view)->GetViewUp(up);
    QVERIFY2(near(pos, 5, 7, 9), "camera position should be restored");
    QVERIFY2(near(focal, 1, 2, 3), "focal point should be restored");
    QVERIFY2(near(up, 0, 0, 1), "view up should be restored");
  }

  // A view with no slice gets one on the tip port. Leaving 2D hides it
  // rather than deleting it, so the next 2D toggle finds it again.
  void createsASliceOnTheTipPortWhenTheViewHasNone()
  {
    auto ds = addDataset(m_pip, 20, 20, 20);
    auto* outline = addSink<pipeline::OutlineSink>(m_pip, ds, m_view);
    ActiveObjects::instance().setActivePort(ds.port());

    setInteractionMode(m_view, MODE_2D);
    QVERIFY(m_behavior->isActive(m_view));
    auto slices = slicesIn(m_pip, m_view);
    QCOMPARE(slices.size(), 1);
    auto* slice = slices.first();
    QCOMPARE(m_behavior->slice(m_view), slice);
    QVERIFY(slice->visibility());
    QCOMPARE(slice->inputPorts()[0]->link()->from(), ds.sinkPort());
    QVERIFY(!outline->visibility());
    double dir[3];
    camera(m_view)->GetDirectionOfProjection(dir);
    QVERIFY2(near(dir, 0, 0, -1), "a new XY slice is viewed from +z");

    setInteractionMode(m_view, MODE_3D);
    QVERIFY(outline->visibility());
    QCOMPARE(slicesIn(m_pip, m_view).size(), 1);
    QVERIFY(!slice->visibility());

    // Back in 2D the same slice is reused, not a second one made
    setInteractionMode(m_view, MODE_2D);
    QCOMPARE(m_behavior->slice(m_view), slice);
    QCOMPARE(slicesIn(m_pip, m_view).size(), 1);
    QVERIFY(slice->visibility());
    setInteractionMode(m_view, MODE_3D);
  }

  // With several datasets, the slice fed by the tip port is the one the
  // camera aims at, and a view showing the tip port's data elsewhere is
  // not touched. A view with none of the tip port's slices gets its own
  // slice of the tip port instead of being emptied out.
  void picksTheTipPortsSliceAndLeavesOtherViewsAlone()
  {
    auto* other = createRenderView();
    QVERIFY(other);
    m_extraViews.append(other);

    auto a = addDataset(m_pip, 10, 10, 10);
    auto b = addDataset(m_pip, 10, 10, 10);
    auto* volumeA = addSink<pipeline::VolumeSink>(m_pip, a, m_view);
    auto* sliceA = addSink<pipeline::SliceSink>(m_pip, a, m_view);
    sliceA->setDirection(pipeline::SliceSink::XY);
    auto* volumeB = addSink<pipeline::VolumeSink>(m_pip, b, m_view);
    auto* sliceB = addSink<pipeline::SliceSink>(m_pip, b, m_view);
    sliceB->setDirection(pipeline::SliceSink::YZ);
    auto* outlineB = addSink<pipeline::OutlineSink>(m_pip, b, other);
    ActiveObjects::instance().setActivePort(b.port());

    setInteractionMode(m_view, MODE_2D);
    QCOMPARE(m_behavior->slice(m_view), sliceB);
    QVERIFY(!volumeA->visibility());
    QVERIFY(!volumeB->visibility());
    QVERIFY(sliceA->visibility());
    QVERIFY(sliceB->visibility());
    double dir[3];
    camera(m_view)->GetDirectionOfProjection(dir);
    QVERIFY2(near(dir, -1, 0, 0), "camera should look along the YZ normal");
    // The other view is not an image viewer and nothing in it changed
    QVERIFY(!m_behavior->isActive(other));
    QVERIFY(outlineB->visibility());

    // The tip port's slices live in m_view; the other view gets its own
    setInteractionMode(other, MODE_2D);
    QVERIFY(m_behavior->isActive(other));
    auto* created = m_behavior->slice(other);
    QVERIFY(created);
    QCOMPARE(created->view(), other->getViewProxy());
    QCOMPARE(created->inputPorts()[0]->link()->from(), b.sinkPort());
    QVERIFY(!outlineB->visibility());
    QCOMPARE(m_behavior->slice(m_view), sliceB);

    setInteractionMode(other, MODE_3D);
    QVERIFY(outlineB->visibility());
    QVERIFY(!created->visibility());
    QVERIFY(m_behavior->isActive(m_view));
    setInteractionMode(m_view, MODE_3D);
    QVERIFY(volumeA->visibility());
    QVERIFY(volumeB->visibility());
  }

  // Rubber-band selection borrows the interactor and hands back to 2D:
  // that round trip must not re-enter and must not restore anything.
  void transientModesKeepTheImageViewer()
  {
    auto ds = addDataset(m_pip, 10, 10, 10);
    auto* volume = addSink<pipeline::VolumeSink>(m_pip, ds, m_view);
    auto* slice = addSink<pipeline::SliceSink>(m_pip, ds, m_view);
    ActiveObjects::instance().setActivePort(ds.port());

    setInteractionMode(m_view, MODE_2D);
    setInteractionMode(m_view, MODE_SELECTION);
    QVERIFY(m_behavior->isActive(m_view));
    QVERIFY(!volume->visibility());
    setInteractionMode(m_view, MODE_2D);
    QVERIFY(m_behavior->isActive(m_view));
    QVERIFY(!volume->visibility());
    QCOMPARE(m_behavior->slice(m_view), slice);

    setInteractionMode(m_view, MODE_3D);
    QVERIFY(!m_behavior->isActive(m_view));
    QVERIFY(volume->visibility());
  }

  // Right after a load nothing is selected yet; the first branch's tip
  // stands in for the tip port, as it does elsewhere in tomviz.
  void fallsBackToTheFirstBranchTipWithoutASelection()
  {
    auto ds = addDataset(m_pip, 10, 10, 10);
    auto* volume = addSink<pipeline::VolumeSink>(m_pip, ds, m_view);
    ActiveObjects::instance().clearActiveSelection();
    QVERIFY(!ActiveObjects::instance().activeTipOutputPort());

    setInteractionMode(m_view, MODE_2D);
    QVERIFY(m_behavior->isActive(m_view));
    auto* slice = m_behavior->slice(m_view);
    QVERIFY(slice);
    QCOMPARE(slice->inputPorts()[0]->link()->from(), ds.sinkPort());
    QVERIFY(!volume->visibility());
    setInteractionMode(m_view, MODE_3D);
    QVERIFY(volume->visibility());
  }

  // No data: the toggle is just a 2D interactor, and the view is left
  // alone rather than emptied.
  void nothingToShowLeavesTheViewAlone()
  {
    setInteractionMode(m_view, MODE_2D);
    QVERIFY(!m_behavior->isActive(m_view));
    QVERIFY(slicesIn(m_pip, m_view).isEmpty());
    setInteractionMode(m_view, MODE_3D);
    QVERIFY(!m_behavior->isActive(m_view));
  }

  // A view that goes away while in 2D drops its record; the sinks it
  // showed are not touched afterwards.
  void removedViewIsForgotten()
  {
    auto* doomed = createRenderView();
    QVERIFY(doomed);
    auto ds = addDataset(m_pip, 10, 10, 10);
    addSink<pipeline::SliceSink>(m_pip, ds, doomed);
    ActiveObjects::instance().setActivePort(ds.port());
    setInteractionMode(doomed, MODE_2D);
    QVERIFY(m_behavior->isActive(doomed));
    pqApplicationCore::instance()->getObjectBuilder()->destroy(doomed);
    QVERIFY(!m_behavior->isActive(doomed));
  }

private:
  ImageViewerModeBehavior* m_behavior;
  pipeline::Pipeline* m_pip;
  pqRenderView* m_view = nullptr;
  QList<pqRenderView*> m_extraViews;
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  pipeline::Pipeline pipeline;
  ActiveObjects::instance().setPipeline(&pipeline);
  ImageViewerModeBehavior behavior;
  ImageViewerModeTest tc(&behavior, &pipeline);
  return QTest::qExec(&tc, argc, argv);
}

#include "ImageViewerModeTest.moc"
