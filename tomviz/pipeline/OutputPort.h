/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineOutputPort_h
#define tomvizPipelineOutputPort_h

#include "PersistenceMode.h"
#include "Port.h"
#include "PortData.h"
#include "PortType.h"

#include <QJsonObject>
#include <QList>

#include <atomic>
#include <memory>
#include <mutex>

class QTemporaryFile;

namespace tomviz {
namespace pipeline {

class InputPort;
class Link;

/// Where the port's currently-published payload lives at this moment.
/// Orthogonal to persistence mode: an OnDisk port reports `InMemory`
/// while a consumer is still holding the data, then flips to `OnDisk`
/// once the last consumer drops and the eviction completes.
enum class DataLocation
{
  None,
  InMemory,
  OnDisk
};

class OutputPort : public Port
{
  Q_OBJECT

public:
  OutputPort(const QString& name, PortType type,
             QObject* parent = nullptr);
  ~OutputPort() override;

  /// Effective type (may differ from declared type due to inference).
  /// Most code should use this rather than declaredType().
  PortType type() const;

  /// The type set at construction time, before any inference.
  PortType declaredType() const;

  /// Change the declared type (also resets the effective type to match).
  void setDeclaredType(PortType type);

  /// Set the effective type.  Called by the owning Node during type
  /// inference.  Only meaningful when declaredType() is ImageData.
  void setEffectiveType(PortType type);

  /// Virtual so subclasses whose persistence is intrinsic to the type
  /// (e.g. PassthroughOutputPort — a pure forwarder with no data of its
  /// own) can pin the answer regardless of the m_persistent flag, so an
  /// old state file's `persistent: true` can't reintroduce spurious
  /// data-serialization for them.
  virtual bool isPersistent() const;
  void setPersistent(bool persistent);

  /// The medium that backs the port when isPersistent() is true.
  /// Orthogonal to the persistent flag itself — setting the mode does
  /// not change whether the port is persistent; setPersistent(false)
  /// makes the mode irrelevant. Defaults to InMemory, so legacy
  /// setPersistent(true) callers get the established behavior.
  PersistenceMode persistenceMode() const;
  void setPersistenceMode(PersistenceMode mode);

  /// Peek at the currently-in-memory payload. Does NOT trigger a
  /// disk reload for OnDisk ports whose data has been evicted —
  /// callers that don't intend to keep the data alive (UI inspection
  /// widgets, histogram, properties panel) get an invalid PortData
  /// in that case and treat it as missing. Use materialize() when
  /// you genuinely need to read an evicted payload.
  virtual PortData data() const;
  void setData(const PortData& data);
  void clearData();
  virtual bool hasData() const;

  /// Materializing read: returns a strong handle to the payload,
  /// loading from disk if this is an OnDisk persistent port whose data
  /// is currently evicted. Returns nullptr when there is no payload to
  /// surface (transient port whose data is already gone). The handle
  /// keeps the port's m_weak alive — callers that need the data to
  /// stay materialized must hold the returned shared_ptr; dropping it
  /// re-evicts an OnDisk port via the universal deleter. For frequent
  /// UI reads, prefer data(), which treats on-disk payloads as
  /// effectively absent.
  virtual std::shared_ptr<PortData> materialize();

  /// Where the published payload currently lives. Tracks the actual
  /// material state, not the persistence policy — an OnDisk port can
  /// report `InMemory` between eviction cycles. Useful for UI cues
  /// that should reflect the real underlying state.
  virtual DataLocation dataLocation() const;

  /// Transient lifetime hook for the pipeline executor.
  ///
  /// Each setData() builds a shared_ptr<PortData> that the port self-pins
  /// (publish-handoff window: the data must outlive its own setData()
  /// emission so consumers about to read see something). The executor
  /// calls take() once it has consumers downstream that depend on this
  /// port; for a transient port that moves the strong ref out of the
  /// port into the executor's in-flight map, leaving the port with only
  /// a weak ref. When the in-flight map drops at end-of-plan, the data
  /// evicts unless some consumer (e.g. a sink) has stashed a copy of
  /// data() in a member.
  ///
  /// Persistent ports do NOT release their strong ref: take() returns a
  /// shared copy without clearing m_strong, so the port retains data
  /// indefinitely.
  ///
  /// A leaf output port (no outgoing links in the current plan) is never
  /// taken from — the port keeps m_strong and the data lives until the
  /// port gains a consumer, at which point the next take() unpins it.
  std::shared_ptr<PortData> take();

  /// Push intermediate data during execution. Thread-safe: can be called
  /// from a worker thread. The update is applied on the main thread
  /// (blocking the caller until complete). Emits intermediateDataApplied()
  /// after the data is applied. Subclasses override to handle specific
  /// data types; the default implementation is a no-op.
  virtual void setIntermediateData(const PortData& data);

  /// Announce that the current payload changed in place (e.g. a
  /// time-series step switch swapped the VolumeData's image) without
  /// republishing it. Runs the same refresh path as an
  /// intermediate-data push - linked sinks re-consume and views
  /// re-render - but skips the payload replacement and deep copy.
  /// Thread-safe: the emission is marshaled to the port's thread.
  void notifyDataMutated();

  virtual bool isStale() const;
  void setStale(bool stale);

  /// Whether this port accepts a link to the given input port.
  /// Default returns true.  Subclasses can restrict (e.g. sinks only).
  virtual bool canAcceptLink(InputPort* to) const;

  QList<Link*> links() const;

  /// Serialize the metadata of the port's current payload (VolumeData,
  /// Molecule, Table, ...) as JSON. Does NOT include raw voxel / table
  /// byte payloads — those are embedded by container-level savers (e.g.
  /// Tvh5Format) under their own HDF5 groups. Returns an empty object
  /// if the port has no data or the payload type isn't round-trippable.
  /// Override in subclasses that carry specialized payloads.
  virtual QJsonObject serialize() const;

  /// Apply JSON produced by serialize() onto this port's current
  /// payload. No-op if the port has no payload yet (loaders that need
  /// to reconstruct the payload from scratch must do so before calling
  /// deserialize). Returns false on unrecoverable parse errors.
  virtual bool deserialize(const QJsonObject& json);

  /// Drop pending metadata stashed by deserialize() so the next
  /// setData() installs the payload as-is. Used by transient clones
  /// that need port structure but not replayed data-state metadata.
  void clearPendingData();

signals:
  void dataChanged();
  void staleChanged(bool stale);

  /// Emitted when the persistence policy changes: the persistent flag
  /// (setPersistent) or the backing medium (setPersistenceMode). UI
  /// showing policy cues (pin badge, menus) refreshes on this; actual
  /// payload residency changes are reported separately via
  /// dataLocationChanged.
  void persistenceChanged();
  void effectiveTypeChanged(PortType newType);
  void intermediateDataApplied();

  /// Emitted when the payload's residency changes (memory ↔ disk ↔
  /// none) without the payload itself necessarily being replaced.
  /// Distinct from dataChanged so subscribers that don't care about
  /// disk swaps (e.g. save dialogs that want a refresh only on new
  /// content) aren't woken on every cache cycle.
  void dataLocationChanged(DataLocation location);

  /// Emitted when lightweight metadata (units, label, spacing, origin)
  /// changes on the data held by this port, without the image data itself
  /// being replaced. Sinks can listen to this to update annotations/UI
  /// without a full pipeline re-execution.
  void metadataChanged();

private:
  friend class Link;
  void addLink(Link* link);
  void removeLink(Link* link);

  /// Hand the label table of the payload being replaced to @a incoming,
  /// when both are label maps. A label map's per-label colors and
  /// visibility are the user's choices, not the data's, and every
  /// execution publishes a fresh payload that would otherwise start
  /// from defaults. Data-only: no ParaView proxies are touched, so this
  /// is safe on whichever thread published.
  void carryOverLabelTable(const PortData& incoming);

  /// For OnDisk-mode persistence: when the last shared_ptr to this
  /// port's PortData is destroyed, the universal deleter calls this to
  /// serialize the payload into m_diskFile and flip m_onDisk to true.
  /// Returns true on success. On failure (write error, unserializable
  /// payload type, ...) the caller should treat the data as still
  /// in-memory and re-pin via the universal deleter rather than
  /// deleting it.
  /// Internal; not part of the public API.
  bool swapToDisk(PortData* data);
  /// Try to reload from the cache file, populating m_weak so subsequent
  /// data()/take() calls see the data again. Returns the freshly-built
  /// shared_ptr (with the universal deleter) or nullptr if the file is
  /// missing/corrupt. Called under m_diskMutex.
  std::shared_ptr<PortData> reloadFromDisk();
  /// Construct a shared_ptr<PortData> wrapping @a data with the
  /// universal deleter — captures the port (via QPointer) and the
  /// current generation tag. The deleter reads the port's CURRENT
  /// persistence mode at fire time and acts accordingly: OnDisk →
  /// swapToDisk; otherwise → just delete. This is what makes runtime
  /// mode switches behave correctly (a shared_ptr created when the
  /// port was InMemory still does the right thing if the port has
  /// since become OnDisk).
  std::shared_ptr<PortData> wrapPortData(PortData* data);
  /// Adjust m_strong / m_onDisk / m_diskFile to satisfy the invariants
  /// implied by the current (m_persistent, m_mode) combination. Called
  /// from setPersistent() and setPersistenceMode() when their value
  /// actually changes, so the port immediately reflects the new mode
  /// instead of waiting for the next setData() or consumer event.
  void reconcilePersistence();

  PortType m_declaredType;
  PortType m_effectiveType;
  /// Strong ref to the published PortData. Set by setData(); cleared by
  /// take() on transient ports; retained on persistent ports.
  std::shared_ptr<PortData> m_strong;
  /// Observation handle. Always set alongside m_strong in setData() and
  /// outlives the strong-ref handoff so that hasData()/data() can detect
  /// whether the data is still materialized anywhere.
  std::weak_ptr<PortData> m_weak;
  bool m_persistent = false;
  PersistenceMode m_mode = PersistenceMode::InMemory;
  bool m_stale = false;
  /// On-disk cache state. m_diskFile is created lazily on first eviction
  /// and lives for the port's lifetime (auto-removed on destruction).
  /// m_onDisk is true iff the cache file currently contains the live
  /// payload. m_generation is bumped on every setData() and captured by
  /// each fresh deleter so a stale generation's deleter no-ops instead
  /// of clobbering newer data. m_destroying short-circuits the deleter
  /// during the destructor so we don't try to write to a disappearing
  /// port. m_diskMutex guards the on-disk state transitions.
  std::unique_ptr<QTemporaryFile> m_diskFile;
  bool m_onDisk = false;
  std::atomic<int> m_generation{ 0 };
  std::atomic<bool> m_destroying{ false };
  mutable std::mutex m_diskMutex;
  QList<Link*> m_links;
  /// Metadata deserialized before setData ran (typical load path:
  /// source node populates data only when it executes). Applied in
  /// setData() and then cleared.
  QJsonObject m_pendingData;
};

} // namespace pipeline
} // namespace tomviz

#endif
