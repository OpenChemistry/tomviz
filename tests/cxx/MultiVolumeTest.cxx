/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QComboBox>
#include <QScopedPointer>
#include <QTest>

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqServerResource.h>

#include <vtkCommand.h>
#include <vtkFloatArray.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageData.h>
#include <vtkMultiVolume.h>
#include <vtkNew.h>
#include <vtkPVRenderView.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>
#include <vtkPointData.h>
#include <vtkProp.h>
#include <vtkPropCollection.h>
#include <vtkRenderer.h>
#include <vtkSMViewProxy.h>
#include <vtkSmartPointer.h>
#include <vtkVolume.h>

#include "pipeline/InputPort.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/SourceNode.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/LabelMapSink.h"
#include "pipeline/sinks/MultiVolumeCoordinator.h"
#include "pipeline/sinks/VolumeSink.h"

using namespace tomviz;
using pipeline::MultiVolumeCoordinator;

namespace {

pqRenderView* createRenderView()
{
  auto* server = pqActiveObjects::instance().activeServer();
  auto* builder = pqApplicationCore::instance()->getObjectBuilder();
  return qobject_cast<pqRenderView*>(
    builder->createView(pqRenderView::renderViewType(), server));
}

/// A float volume carrying one named array per entry of @a arrays; the
/// first one is the active scalars.
vtkSmartPointer<vtkImageData> floatImage(
  const QStringList& arrays = { "scalars" })
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(8, 8, 8);
  for (const auto& name : arrays) {
    vtkNew<vtkFloatArray> array;
    array->SetName(name.toUtf8().constData());
    array->SetNumberOfTuples(image->GetNumberOfPoints());
    array->Fill(1.0f);
    image->GetPointData()->AddArray(array);
  }
  image->GetPointData()->SetActiveScalars(arrays.first().toUtf8().constData());
  return vtkSmartPointer<vtkImageData>(image.GetPointer());
}

/// A volume whose values read as labels 0, 1 and 2.
vtkSmartPointer<vtkImageData> labelImage()
{
  vtkNew<vtkImageData> image;
  image->SetDimensions(8, 8, 8);
  image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  auto* p = static_cast<unsigned char*>(image->GetScalarPointer());
  for (int i = 0; i < 8 * 8 * 8; ++i) {
    p[i] = static_cast<unsigned char>(i % 3);
  }
  return vtkSmartPointer<vtkImageData>(image.GetPointer());
}

pipeline::SourceNode* addSource(pipeline::Pipeline* pip,
                                vtkSmartPointer<vtkImageData> image)
{
  auto vol = std::make_shared<pipeline::VolumeData>(image);
  auto* source = new pipeline::SourceNode();
  source->addOutput("volume", pipeline::PortType::Volume);
  pip->addNode(source);
  source->setOutputData(
    "volume", pipeline::PortData(vol, pipeline::PortType::Volume));
  return source;
}

/// A sink on @a view fed by @a source, with the data consumed, as it
/// stands after the pipeline has run.
template <typename Sink>
Sink* addSink(pipeline::Pipeline* pip, pipeline::SourceNode* source,
              pqRenderView* view)
{
  auto* sink = new Sink();
  sink->setLabel(QString("Sink %1").arg(pip->nodes().size()));
  sink->initialize(view->getViewProxy());
  pip->addNode(sink);
  pip->createLink(source->outputPorts()[0], sink->inputPorts()[0]);
  sink->execute();
  return sink;
}

vtkRenderer* renderer(pqRenderView* view)
{
  auto* rv = vtkPVRenderView::SafeDownCast(
    view->getViewProxy()->GetClientSideView());
  return rv ? rv->GetRenderer() : nullptr;
}

bool inRenderer(pqRenderView* view, vtkProp* prop)
{
  auto* ren = renderer(view);
  return ren && ren->HasViewProp(prop);
}

/// The multi-volume prop the view draws, or null while it draws none.
vtkMultiVolume* multiVolumeIn(pqRenderView* view)
{
  auto* ren = renderer(view);
  if (!ren) {
    return nullptr;
  }
  auto* props = ren->GetViewProps();
  vtkCollectionSimpleIterator it;
  props->InitTraversal(it);
  while (auto* prop = props->GetNextProp(it)) {
    if (auto* multi = vtkMultiVolume::SafeDownCast(prop)) {
      return multi;
    }
  }
  return nullptr;
}

MultiVolumeCoordinator* coordinator(pqRenderView* view)
{
  return MultiVolumeCoordinator::find(view->getViewProxy());
}

bool rendersTogether(pqRenderView* view)
{
  auto* c = coordinator(view);
  return c && c->active() && multiVolumeIn(view) != nullptr;
}

} // namespace

class MultiVolumeTest : public QObject
{
  Q_OBJECT

public:
  explicit MultiVolumeTest(pipeline::Pipeline* pipeline) : m_pip(pipeline) {}

private slots:
  void init()
  {
    m_view = createRenderView();
    QVERIFY(m_view);
  }

  void cleanup()
  {
    m_pip->clear();
    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    for (auto* view : m_extraViews) {
      builder->destroy(view);
    }
    m_extraViews.clear();
    builder->destroy(m_view);
    m_view = nullptr;
  }

  // A lone volume renders exactly as it always has: its own prop in the
  // renderer, nothing shared.
  void oneVolumeRendersOnItsOwn()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    QVERIFY(a->visibility());
    QVERIFY(inRenderer(m_view, a->volumeProp()));
    QVERIFY(!multiVolumeIn(m_view));
    QVERIFY(!a->multiVolumeActive());
    auto* c = coordinator(m_view);
    QVERIFY(c);
    QVERIFY(!c->active());
    QCOMPARE(c->members().size(), 1);
  }

  // The second volume on the view switches to the shared multi-volume:
  // both sinks' own props leave the renderer and the mapper gets a port
  // per volume, the first sink on port 0.
  void aSecondVolumeSwitchesTheViewToTheMultiVolume()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    QVERIFY(rendersTogether(m_view));
    QVERIFY(!inRenderer(m_view, a->volumeProp()));
    QVERIFY(!inRenderer(m_view, b->volumeProp()));
    QVERIFY(a->multiVolumeActive());
    QVERIFY(b->multiVolumeActive());
    QVERIFY(a->multiVolumeLead());
    QVERIFY(!b->multiVolumeLead());

    auto* c = coordinator(m_view);
    QCOMPARE(c->lead(), a);
    QCOMPARE(c->mapper()->GetInputCount(), 2);
    QCOMPARE(c->multiVolume()->GetVolume(0), a->volumeProp());
    QCOMPARE(c->multiVolume()->GetVolume(1), b->volumeProp());
    // The multi-volume renders through the shared mapper, not either
    // sink's own.
    QCOMPARE(c->multiVolume()->GetMapper(), c->mapper());
  }

  // Hiding one of two volumes puts the other back on its own prop, and
  // showing it again rejoins them. The hidden sink gets its (invisible)
  // prop back too, so it renders as before once shown on its own.
  void hidingAVolumeHandsTheOtherItsPropBack()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    QVERIFY(rendersTogether(m_view));

    b->setVisibility(false);
    QVERIFY(!rendersTogether(m_view));
    QVERIFY(!multiVolumeIn(m_view));
    QVERIFY(inRenderer(m_view, a->volumeProp()));
    QVERIFY(inRenderer(m_view, b->volumeProp()));
    QCOMPARE(b->volumeProp()->GetVisibility(), 0);
    QVERIFY(!a->multiVolumeActive());
    QVERIFY(!b->multiVolumeActive());
    QCOMPARE(coordinator(m_view)->members().size(), 1);

    b->setVisibility(true);
    QVERIFY(rendersTogether(m_view));
    QVERIFY(!inRenderer(m_view, a->volumeProp()));
    QVERIFY(!inRenderer(m_view, b->volumeProp()));
  }

  // Port 0 decides the shading of the whole set, so it must never go
  // empty: when the lead leaves, the earliest remaining member takes it.
  void theEarliestRemainingVolumeBecomesTheLead()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* cSink = addSink<pipeline::VolumeSink>(
      m_pip, addSource(m_pip, floatImage()), m_view);
    auto* c = coordinator(m_view);
    QCOMPARE(c->lead(), a);
    QCOMPARE(c->mapper()->GetInputCount(), 3);

    a->setVisibility(false);
    QVERIFY(rendersTogether(m_view));
    QCOMPARE(c->lead(), b);
    QVERIFY(b->multiVolumeLead());
    QVERIFY(!cSink->multiVolumeLead());
    QCOMPARE(c->multiVolume()->GetVolume(0), b->volumeProp());
    QCOMPARE(c->mapper()->GetInputCount(), 2);
    QCOMPARE(c->multiVolume()->GetVolume(2), cSink->volumeProp());
  }

  // Leaving the multi-volume releases the shared mapper's GPU resources,
  // and VTK never marks a multi-input mapper initialized again after
  // that: it would re-upload every volume on every frame. So the mapper
  // is retired and the next activation gets a fresh one, fully wired.
  void aReleasedMapperIsReplacedOnReactivation()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* c = coordinator(m_view);
    QVERIFY(rendersTogether(m_view));
    auto* first = c->mapper();
    vtkNew<vtkPlane> plane;
    a->addClippingPlane(plane);

    b->setVisibility(false);
    b->setVisibility(true);
    QVERIFY(rendersTogether(m_view));
    QVERIFY(c->mapper() != first);
    QCOMPARE(c->multiVolume()->GetMapper(), c->mapper());
    QCOMPARE(c->mapper()->GetInputCount(), 2);
    QVERIFY(c->mapper()->GetInputDataObject(0, 0));
    QVERIFY(c->mapper()->GetInputDataObject(1, 0));
    QCOMPARE(c->mapper()->GetClippingPlanes()->GetNumberOfItems(), 1);
    // A release while active (a recreated GL context) is caught at the
    // start of the next frame.
    auto* second = c->mapper();
    second->ReleaseGraphicsResources(nullptr);
    renderer(m_view)->InvokeEvent(vtkCommand::StartEvent);
    QVERIFY(c->mapper() != second);
    QCOMPARE(c->mapper()->GetInputCount(), 2);
  }

  // A label map drawn as surfaces has no volume on screen, so it does not
  // count; switched to its volume representation it does.
  void labelMapsCountOnlyWhenDrawnAsVolumes()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* labels = addSink<pipeline::LabelMapSink>(
      m_pip, addSource(m_pip, labelImage()), m_view);
    QCOMPARE(labels->representation(),
             pipeline::LabelMapSink::Representation::Surface);
    QVERIFY(!rendersTogether(m_view));
    QVERIFY(inRenderer(m_view, a->volumeProp()));

    labels->setRepresentation(pipeline::LabelMapSink::Representation::Volume);
    QVERIFY(rendersTogether(m_view));
    QVERIFY(labels->multiVolumeActive());
    // The label map asked for fine sampling, which the shared mapper has
    // to honor for the set.
    QCOMPARE(coordinator(m_view)->mapper()->GetAutoAdjustSampleDistances(), 0);

    labels->setRepresentation(
      pipeline::LabelMapSink::Representation::Surface);
    QVERIFY(!rendersTogether(m_view));
    QVERIFY(inRenderer(m_view, a->volumeProp()));
  }

  // Two sinks on the same data can show different arrays: each port's
  // input carries the array its sink selected, and the shared data is
  // left alone.
  void eachVolumeKeepsItsOwnScalars()
  {
    auto image = floatImage({ "first", "second" });
    auto* source = addSource(m_pip, image);
    auto* a = addSink<pipeline::VolumeSink>(m_pip, source, m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, source, m_view);
    b->setActiveScalars(1);
    QVERIFY(rendersTogether(m_view));

    auto* c = coordinator(m_view);
    auto* inputA = vtkImageData::SafeDownCast(c->mapper()->GetInputDataObject(0, 0));
    auto* inputB = vtkImageData::SafeDownCast(c->mapper()->GetInputDataObject(1, 0));
    QVERIFY(inputA && inputB);
    QCOMPARE(QString(inputA->GetPointData()->GetScalars()->GetName()),
             QString("first"));
    QCOMPARE(QString(inputB->GetPointData()->GetScalars()->GetName()),
             QString("second"));
    QCOMPARE(QString(image->GetPointData()->GetScalars()->GetName()),
             QString("first"));
    // The copies share the voxels rather than duplicating them.
    QCOMPARE(inputB->GetPointData()->GetArray("second"),
             image->GetPointData()->GetArray("second"));

    // Re-selecting the same array hands the mapper nothing new.
    auto* before = c->mapper()->GetInputDataObject(1, 0);
    b->setActiveScalars(1);
    QCOMPARE(c->mapper()->GetInputDataObject(1, 0), before);
    QVERIFY(a->multiVolumeActive());
  }

  // Clipping planes are a mapper setting, so every member's planes gather
  // on the shared mapper, each once.
  void clippingPlanesGatherOnTheSharedMapper()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    vtkNew<vtkPlane> shared;
    vtkNew<vtkPlane> onlyA;
    a->addClippingPlane(shared);
    a->addClippingPlane(onlyA);
    b->addClippingPlane(shared);
    auto* planes = coordinator(m_view)->mapper()->GetClippingPlanes();
    QVERIFY(planes);
    QCOMPARE(planes->GetNumberOfItems(), 2);

    a->removeClippingPlane(onlyA);
    QCOMPARE(planes->GetNumberOfItems(), 1);
    b->setVisibility(false);
    // Back on its own, the sink's own mapper still carries its plane.
    QCOMPARE(a->clippingPlanes()->GetNumberOfItems(), 1);
  }

  // Views are independent: a volume in another view neither joins nor
  // counts.
  void viewsDoNotShareAMultiVolume()
  {
    auto* other = createRenderView();
    QVERIFY(other);
    m_extraViews.append(other);
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            other);
    QVERIFY(!rendersTogether(m_view));
    QVERIFY(!rendersTogether(other));
    QVERIFY(inRenderer(m_view, a->volumeProp()));
    QVERIFY(inRenderer(other, b->volumeProp()));
  }

  // Deleting a member leaves the set cleanly, and the coordinator goes
  // away with its last member.
  void removingSinksTearsDownCleanly()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    QVERIFY(rendersTogether(m_view));

    m_pip->removeNode(b); // deletes it
    QVERIFY(!rendersTogether(m_view));
    QVERIFY(!multiVolumeIn(m_view));
    QVERIFY(inRenderer(m_view, a->volumeProp()));
    QCOMPARE(coordinator(m_view)->members().size(), 1);

    m_pip->removeNode(a);
    QVERIFY(!coordinator(m_view));
    QVERIFY(!multiVolumeIn(m_view));
  }

  // The panel greys out what the shared path takes away and shows what
  // actually renders there, and hands the sink's own settings back
  // afterwards.
  void thePanelFollowsTheSharedPath()
  {
    auto* a = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    a->setBlendingMode(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND);
    QScopedPointer<QWidget> panel(a->createSinkPropertiesWidget(nullptr));
    auto* blending = panel->findChild<QComboBox*>("cbBlending");
    QVERIFY(blending);
    QVERIFY(blending->isEnabled());
    QCOMPARE(blending->currentIndex(),
             static_cast<int>(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND));

    auto* b = addSink<pipeline::VolumeSink>(m_pip, addSource(m_pip, floatImage()),
                                            m_view);
    QVERIFY(rendersTogether(m_view));
    QVERIFY(!blending->isEnabled());
    QCOMPARE(blending->currentIndex(),
             static_cast<int>(vtkVolumeMapper::COMPOSITE_BLEND));
    // The sink's own choice is untouched underneath.
    QCOMPARE(a->blendingMode(),
             static_cast<int>(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND));

    b->setVisibility(false);
    QVERIFY(blending->isEnabled());
    QCOMPARE(blending->currentIndex(),
             static_cast<int>(vtkVolumeMapper::MAXIMUM_INTENSITY_BLEND));
  }

private:
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
  MultiVolumeTest tc(&pipeline);
  return QTest::qExec(&tc, argc, argv);
}

#include "MultiVolumeTest.moc"
