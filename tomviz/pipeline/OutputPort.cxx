/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OutputPort.h"

#include "Link.h"
#include "PortDataDiskCache.h"
#include "ThreadUtils.h"
#include "data/LabelMapData.h"
#include "data/VolumeData.h"

#include <QDebug>
#include <QDir>
#include <QPointer>
#include <QTemporaryFile>

#include <cstdlib>

namespace tomviz {
namespace pipeline {

namespace {

QString portCacheDir()
{
  if (const char* env = std::getenv("TOMVIZ_PORT_CACHE_DIR");
      env && *env != '\0') {
    return QString::fromUtf8(env);
  }
  return QDir::tempPath();
}

} // namespace

OutputPort::OutputPort(const QString& name, PortType type, QObject* parent)
  : Port(name, parent), m_declaredType(type), m_effectiveType(type)
{}

OutputPort::~OutputPort()
{
  // Tell any in-flight OnDisk deleter (firing on another thread that
  // still holds a shared_ptr to our payload) to no-op instead of
  // touching half-destroyed state.
  m_destroying.store(true);
  // Release our own ref first so the deleter (if any) runs with the
  // destroying flag already set.
  {
    std::lock_guard<std::mutex> lock(m_diskMutex);
    m_strong.reset();
  }
  // m_diskFile destruction removes the temp file from disk.
}

void OutputPort::reconcilePersistence()
{
  const DataLocation before = dataLocation();
  // Hold any ref we're about to drop in a local that destructs *after*
  // we release the mutex — the universal deleter takes the same mutex
  // (via swapToDisk) and would otherwise deadlock.
  std::shared_ptr<PortData> dropped;
  std::unique_ptr<QTemporaryFile> droppedFile;
  {
    std::lock_guard<std::mutex> lock(m_diskMutex);

    if (!m_persistent) {
      // Transient: the port doesn't keep a ref. Any cache file is
      // meaningless (transient = no persistence at all) — drop it.
      dropped = std::move(m_strong);
      m_strong.reset();
      if (m_onDisk) {
        m_onDisk = false;
        droppedFile = std::move(m_diskFile);
      }
    } else if (m_mode == PersistenceMode::InMemory) {
      // InMemory persistent: port pins data in memory.
      if (!m_strong) {
        // Try to recover any live ref from m_weak; otherwise pull
        // from disk if we still have a cache.
        m_strong = m_weak.lock();
        if (!m_strong && m_onDisk) {
          m_strong = reloadFromDisk();
        }
      }
      // InMemory has no use for the disk cache.
      if (m_onDisk) {
        m_onDisk = false;
        droppedFile = std::move(m_diskFile);
      }
    } else {
      // OnDisk persistent: port doesn't pin in memory — the universal
      // deleter swaps to disk when the last consumer drops. If we
      // were the only holder, releasing now triggers the swap.
      dropped = std::move(m_strong);
      m_strong.reset();
    }
  }
  // Release outside the mutex (the deleter acquires m_diskMutex inside
  // swapToDisk) and before the `after` snapshot, so any eviction the
  // release triggers is reflected in it. Reconciling can move the
  // payload (evict, reload, drop) without a setData(); surface that to
  // keep the dataLocationChanged contract.
  dropped.reset();
  droppedFile.reset();
  const DataLocation after = dataLocation();
  if (after != before) {
    emit dataLocationChanged(after);
  }
}

PortType OutputPort::type() const
{
  return m_effectiveType;
}

PortType OutputPort::declaredType() const
{
  return m_declaredType;
}

void OutputPort::setDeclaredType(PortType type)
{
  m_declaredType = type;
  setEffectiveType(type);
}

void OutputPort::setEffectiveType(PortType type)
{
  if (m_effectiveType != type) {
    m_effectiveType = type;
    emit effectiveTypeChanged(type);
  }
}

bool OutputPort::isPersistent() const
{
  return m_persistent;
}

void OutputPort::setPersistent(bool persistent)
{
  if (m_persistent == persistent) {
    return;
  }
  m_persistent = persistent;
  reconcilePersistence();
  emit persistenceChanged();
}

PersistenceMode OutputPort::persistenceMode() const
{
  return m_mode;
}

void OutputPort::setPersistenceMode(PersistenceMode mode)
{
  if (m_mode == mode) {
    return;
  }
  m_mode = mode;
  reconcilePersistence();
  emit persistenceChanged();
}

PortData OutputPort::data() const
{
  if (auto sp = m_weak.lock()) {
    return *sp;
  }
  return PortData();
}

std::shared_ptr<PortData> OutputPort::materialize()
{
  if (auto sp = m_weak.lock()) {
    return sp;
  }
  if (m_persistent && m_mode == PersistenceMode::OnDisk) {
    std::shared_ptr<PortData> sp;
    bool reloaded = false;
    {
      // Acquire the disk mutex only for the reload, then release it
      // before sp's destruction could fire the OnDisk deleter — the
      // deleter takes the same mutex and would otherwise deadlock.
      std::lock_guard<std::mutex> lock(m_diskMutex);
      sp = m_weak.lock();
      if (!sp && m_onDisk) {
        sp = reloadFromDisk();
        reloaded = (sp != nullptr);
      }
    }
    if (reloaded) {
      emit dataLocationChanged(DataLocation::InMemory);
    }
    return sp;
  }
  return nullptr;
}

void OutputPort::setData(const PortData& data)
{
  carryOverLabelTable(data);
  {
    std::lock_guard<std::mutex> lock(m_diskMutex);
    // Bumping the generation invalidates the deleter of any earlier
    // shared_ptr still in flight — that deleter will no-op instead of
    // writing stale data over our new cache.
    m_generation.fetch_add(1);
    // Any prior on-disk content belongs to a previous generation; it
    // will be rewritten next time the new payload evicts.
    m_onDisk = false;

    // Always wrap with the universal deleter. It reads the port's
    // *current* mode at fire time, so a shared_ptr created while the
    // port was (say) InMemory still does the right thing if the port
    // is switched to OnDisk before the shared_ptr is dropped.
    m_strong = wrapPortData(new PortData(data));
    m_weak = m_strong;
    m_stale = false;
  }
  // If a metadata blob arrived via deserialize() before data was
  // populated, apply it now to the freshly-set payload — e.g. user
  // edits to a source's colormap / scalar renames that must survive
  // a state-file save+load+execute cycle.
  if (!m_pendingData.isEmpty()) {
    QJsonObject pending = m_pendingData;
    m_pendingData = {};
    deserialize(pending);
  }
  emit dataChanged();
  emit dataLocationChanged(DataLocation::InMemory);
}

void OutputPort::clearData()
{
  {
    std::lock_guard<std::mutex> lock(m_diskMutex);
    // Invalidate any in-flight deleter: this payload is discarded
    // outright, so a deleter firing on the reset below must not evict
    // it to disk (resurrecting cleared data) nor take m_diskMutex via
    // swapToDisk while we hold it.
    m_generation.fetch_add(1);
    m_strong.reset();
    m_weak.reset();
    m_onDisk = false;
  }
  emit dataChanged();
  emit dataLocationChanged(DataLocation::None);
}

bool OutputPort::hasData() const
{
  return dataLocation() != DataLocation::None;
}

DataLocation OutputPort::dataLocation() const
{
  if (!m_weak.expired()) {
    return DataLocation::InMemory;
  }
  std::lock_guard<std::mutex> lock(m_diskMutex);
  if (m_onDisk) {
    return DataLocation::OnDisk;
  }
  return DataLocation::None;
}

std::shared_ptr<PortData> OutputPort::take()
{
  if (m_persistent && m_mode == PersistenceMode::InMemory) {
    // InMemory: keep our own strong ref alive; hand the executor a
    // shared copy. The port retains the data indefinitely.
    if (m_strong) {
      return m_strong;
    }
    return m_weak.lock();
  }
  if (m_persistent && m_mode == PersistenceMode::OnDisk) {
    // OnDisk uses the same handoff semantics as transient — we move
    // our strong ref out so that when the last consumer drops it the
    // shared_ptr's deleter fires and evicts to disk. If everyone has
    // already dropped and the data is on disk, reload it.
    std::shared_ptr<PortData> result;
    bool reloaded = false;
    {
      std::lock_guard<std::mutex> lock(m_diskMutex);
      if (m_strong) {
        result = std::move(m_strong);
        m_strong.reset();
      } else if (auto sp = m_weak.lock()) {
        result = sp;
      } else if (m_onDisk) {
        result = reloadFromDisk();
        reloaded = (result != nullptr);
      }
    }
    if (reloaded) {
      emit dataLocationChanged(DataLocation::InMemory);
    }
    return result;
  }
  // Transient
  if (m_strong) {
    auto handle = std::move(m_strong);
    m_strong.reset();
    return handle;
  }
  return m_weak.lock();
}

std::shared_ptr<PortData> OutputPort::wrapPortData(PortData* raw)
{
  // Capture: a QPointer so a destroyed port is harmless, and the
  // current generation tag so a later setData() invalidates this
  // deleter cleanly. The mode itself is NOT captured — it's read at
  // fire time, so the deleter responds to the port's current state
  // even if persistence was switched between construction and drop.
  QPointer<OutputPort> guard(this);
  int gen = m_generation.load();
  return std::shared_ptr<PortData>(raw, [guard, gen](PortData* p) {
    OutputPort* port = guard.data();
    bool tracked = port && !port->m_destroying.load() &&
                   port->m_generation.load() == gen;
    bool swapped = false;
    if (tracked && port->m_persistent &&
        port->m_mode == PersistenceMode::OnDisk) {
      // Best-effort: if the swap fails (unsupported payload type,
      // disk full, ...) swapToDisk has already logged a warning.
      // The data is lost; the planner will see hasData()==false
      // and re-run the producer when something downstream needs
      // it.
      swapped = port->swapToDisk(p);
    }
    // InMemory persistent: m_strong was the sole holder and is being
    //   dropped (mode switch or port destruction); just delete.
    // Transient: same, no special action.
    delete p;
    if (tracked && !swapped) {
      // The payload is gone for good (transient/unpinned drop, or a
      // failed swap). Surface it, or UI residency cues keep showing
      // in-memory data that no longer exists. The successful-swap
      // transition is emitted by swapToDisk itself.
      emit port->dataLocationChanged(DataLocation::None);
    }
  });
}

bool OutputPort::swapToDisk(PortData* p)
{
  if (!p || !p->isValid()) {
    return false;
  }
  bool swapped = false;
  {
    std::lock_guard<std::mutex> lock(m_diskMutex);
    // Re-check under lock in case the port started destroying between
    // the deleter's outer check and acquiring the mutex.
    if (m_destroying.load()) {
      return false;
    }
    if (!m_diskFile) {
      // Lazily create the cache file under the configured directory.
      QString tmpl = QDir(portCacheDir())
                       .filePath(QStringLiteral("tomviz_port_XXXXXX.tvh5"));
      m_diskFile = std::make_unique<QTemporaryFile>(tmpl);
      m_diskFile->setAutoRemove(true);
      if (!m_diskFile->open()) {
        qWarning() << "OutputPort: failed to open cache file" << tmpl;
        m_diskFile.reset();
        return false;
      }
      // Close the QFile handle so Tvh5Format can open it for writing.
      m_diskFile->close();
    }
    QString path = m_diskFile->fileName();
    if (!writePortDataToFile(*p, path)) {
      qWarning() << "OutputPort: writePortDataToFile failed for" << path;
      return false;
    }
    m_onDisk = true;
    swapped = true;
  }
  if (swapped) {
    emit dataLocationChanged(DataLocation::OnDisk);
  }
  return swapped;
}

std::shared_ptr<PortData> OutputPort::reloadFromDisk()
{
  // m_diskMutex must already be held by caller.
  if (!m_onDisk || !m_diskFile) {
    return nullptr;
  }
  PortData payload = readPortDataFromFile(m_diskFile->fileName());
  if (!payload.isValid()) {
    qWarning() << "OutputPort: reload failed; clearing on-disk flag for"
               << m_diskFile->fileName();
    m_onDisk = false;
    return nullptr;
  }
  // No generation bump here: the data hasn't changed, just been
  // re-materialized. If another deleter were in flight it would have
  // kept m_weak alive (so we wouldn't be in this branch). The new
  // shared_ptr captures the current generation; nothing stale.
  auto sp = wrapPortData(new PortData(std::move(payload)));
  m_weak = sp;
  // Do not pin m_strong: the reload happened because someone read the
  // data; that someone (executor / consumer) will keep the strong ref
  // alive. When they drop it, the deleter writes back to disk.
  return sp;
}

void OutputPort::setIntermediateData(const PortData& data)
{
  // Default: replace the payload outright. Subclasses (e.g.
  // VolumeOutputPort) override when they can preserve the existing
  // object's identity to keep downstream references (color maps,
  // module pipelines) attached. Marshaled to the port's owning
  // thread so callers from worker threads are safe.
  auto apply = [this, data]() {
    setData(data);
    emit intermediateDataApplied();
  };
  runOnThread(this, apply);
}

void OutputPort::notifyDataMutated()
{
  runOnThread(this, [this]() { emit intermediateDataApplied(); });
}

bool OutputPort::isStale() const
{
  return m_stale;
}

void OutputPort::setStale(bool stale)
{
  if (m_stale != stale) {
    m_stale = stale;
    emit staleChanged(stale);
  }
}

QList<Link*> OutputPort::links() const
{
  return m_links;
}

void OutputPort::addLink(Link* link)
{
  m_links.append(link);
  emit connectionChanged();
}

void OutputPort::removeLink(Link* link)
{
  m_links.removeOne(link);
  emit connectionChanged();
}

bool OutputPort::canAcceptLink(InputPort* /*to*/) const
{
  return true;
}

void OutputPort::carryOverLabelTable(const PortData& incoming)
{
  auto previous = m_weak.lock();
  if (!previous) {
    return;
  }
  auto* oldVolume = std::any_cast<VolumeDataPtr>(&previous->data());
  auto* newVolume = std::any_cast<VolumeDataPtr>(&incoming.data());
  // A node that forwards its input untouched republishes the very same
  // payload; there is nothing to carry over, and adopting from itself
  // would force a needless rescan.
  if (!oldVolume || !newVolume || *oldVolume == *newVolume) {
    return;
  }
  auto oldLabels = labelMapData(*oldVolume);
  auto newLabels = labelMapData(*newVolume);
  if (oldLabels && newLabels) {
    newLabels->adoptLabelsFrom(*oldLabels);
  }
}

QJsonObject OutputPort::serialize() const
{
  auto sp = m_weak.lock();
  if (!sp) {
    return {};
  }
  // Known round-trippable payloads. Extend as new PortData types
  // (Molecule, Table, ...) acquire serialize()/deserialize() support.
  // Use std::any_cast's nothrow pointer form so ports carrying
  // non-matching payloads (e.g. Molecule, Table) don't throw here.
  if (auto* volume = std::any_cast<VolumeDataPtr>(&sp->data())) {
    return (*volume)->serialize();
  }
  return {};
}

bool OutputPort::deserialize(const QJsonObject& json)
{
  if (json.isEmpty()) {
    return true;
  }
  if (auto sp = m_weak.lock()) {
    if (auto* volume = std::any_cast<VolumeDataPtr>(&sp->data())) {
      return (*volume)->deserialize(json);
    }
  }
  // No payload yet — stash so the next setData() can apply this JSON
  // on top of the freshly-populated data (e.g. source node execute
  // that produces a fresh VolumeData, to which we then reattach the
  // user's colormap / scalar renames from the state file).
  m_pendingData = json;
  return true;
}

void OutputPort::clearPendingData()
{
  m_pendingData = QJsonObject();
}

} // namespace pipeline
} // namespace tomviz
