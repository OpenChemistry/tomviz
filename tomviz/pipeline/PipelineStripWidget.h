/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineStripWidget_h
#define tomvizPipelineStripWidget_h

#include "PortType.h"

#include <QHash>
#include <QIcon>
#include <QList>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QWidget>

#include <functional>

class QMenu;

namespace tomviz {
namespace pipeline {

enum class SortOrder;
class InputPort;
class Link;
class Node;
class OutputPort;
class Pipeline;
class SinkGroupNode;

struct LayoutItem
{
  enum Type
  {
    NodeCard,
    PortCard,
    GroupMemberCard // sink displayed inside a SinkGroupNode
  };

  Type type = NodeCard;
  Node* node = nullptr;
  OutputPort* port = nullptr; // set only for PortCard
  QRect rect;
};

struct LinkGeometry
{
  Link* link = nullptr;
  QPainterPath path;
  QColor color;
  bool valid = true;
};

class PipelineStripWidget : public QWidget
{
  Q_OBJECT

public:
  explicit PipelineStripWidget(QWidget* parent = nullptr);
  ~PipelineStripWidget() override = default;

  void setPipeline(Pipeline* pipeline);
  Pipeline* pipeline() const;

  void setSortOrder(SortOrder order);
  SortOrder sortOrder() const;

  Node* selectedNode() const;
  OutputPort* selectedPort() const;
  Link* selectedLink() const;

  /// Programmatic selection setters — update the visual selection state
  /// without emitting signals. Use these to sync the widget when the active
  /// object is changed externally (e.g. via ActiveObjects).
  void setSelectedNode(Node* node);
  void setSelectedPort(OutputPort* port);
  void setSelectedLink(Link* link);

  void setTipOutputPort(OutputPort* port);
  OutputPort* tipOutputPort() const;

  bool isExpanded(Node* node) const;
  void setExpanded(Node* node, bool expanded);

  /// Animate a "marching ants" flow along links. The flow can be enabled
  /// independently for the hovered link and the selected link.
  void setLinkAnimationOnHover(bool enabled);
  void setLinkAnimationOnSelection(bool enabled);

  /// Context menu providers. The callback populates a QMenu for the given
  /// element. If the callback leaves the menu empty, no context menu is shown.
  /// Passing a null (empty) std::function disables the context menu for that
  /// element type.
  using NodeMenuProvider = std::function<void(Node*, QMenu&)>;
  using PortMenuProvider = std::function<void(OutputPort*, QMenu&)>;
  using LinkMenuProvider = std::function<void(Link*, QMenu&)>;

  void setNodeMenuProvider(NodeMenuProvider provider);
  void setPortMenuProvider(PortMenuProvider provider);
  void setLinkMenuProvider(LinkMenuProvider provider);

  /// Validator called during interactive link creation to determine whether
  /// the pending link can connect to a given input port. If not set, all
  /// connections are considered valid.
  using LinkValidator = std::function<bool(OutputPort*, InputPort*)>;
  void setLinkValidator(LinkValidator validator);

  /// Dimming: blend element colors toward the background.
  /// dimLevel 0 = normal, 1 = fully faded to background.
  /// When true, mutation interactions (link dragging, double-click-to-edit,
  /// context-menu actions, delete key) are suppressed.
  void setInteractionLocked(bool locked);
  bool isInteractionLocked() const;

  void setDimmingEnabled(bool enabled);
  bool isDimmingEnabled() const;

  void setDimLevel(qreal level);
  qreal dimLevel() const;
  void setNodeDimmed(Node* node, bool dimmed);
  void setPortDimmed(OutputPort* port, bool dimmed);
  void setLinkDimmed(Link* link, bool dimmed);
  bool isNodeDimmed(Node* node) const;
  bool isPortDimmed(OutputPort* port) const;
  bool isLinkDimmed(Link* link) const;
  void clearDimming();

  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;

signals:
  void nodeSelected(Node* node);
  void portSelected(OutputPort* port);
  void linkSelected(Link* link);
  void selectionCleared();
  void nodeDoubleClicked(Node* node);
  void linkRequested(OutputPort* from, InputPort* to);
  void leaveGroupRequested(Node* member, SinkGroupNode* group);
  void deleteNodeRequested(Node* node);
  void deleteLinkRequested(Link* link);

public slots:
  void rebuildLayout();

protected:
  bool event(QEvent* event) override; // hover tooltips
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

private:
  void connectPipeline();
  void disconnectPipeline();
  void selectItem(int index);
  void selectLink(Link* link);
  // Move selection up/down through a vertically-ordered list of layout
  // items and links (direction: -1 up, +1 down).
  void navigateVertical(int direction);
  // Start/stop the marching-ants timer based on the current hover/selection
  // state and the per-mode animation flags.
  void updateLinkAnimationTimer();
  int hitTest(const QPoint& pos) const;
  Link* linkHitTest(const QPoint& pos) const;
  // Tooltip text for whatever element is under @a pos (empty if none).
  QString tooltipAt(const QPoint& pos) const;
  int selectedIndex() const;
  void showContextMenu(const QPoint& globalPos);

  // Painting helpers
  void paintNodeCard(QPainter& painter, const LayoutItem& item,
                     bool selected, bool hovered);
  void paintPortCard(QPainter& painter, const LayoutItem& item,
                     bool selected, bool hovered);
  // The port card's icon square + persistence badges, painted on top of the
  // links (like collapsed output dots) so links/halo don't cover them.
  void paintPortCardIcon(QPainter& painter, const LayoutItem& item,
                         bool selected);
  void paintGroupMemberCard(QPainter& painter, const LayoutItem& item,
                            bool selected, bool hovered);
  void paintConnections(QPainter& painter);
  void paintSelectedLinkHalo(QPainter& painter);
  void computeLinkGeometries();
  void paintPendingLink(QPainter& painter);
  OutputPort* outputPortHitTest(const QPoint& pos) const;
  InputPort* inputPortHitTest(const QPoint& pos) const;
  void paintInputDots(QPainter& painter, const LayoutItem& item);
  void paintOutputDots(QPainter& painter, const LayoutItem& item);
  void paintPortCardDot(QPainter& painter, const LayoutItem& item);
  QPoint inputDotPos(Node* node, int portIndex, const QRect& nodeRect) const;
  QPoint outputDotPos(Node* node, int portIndex, const QRect& nodeRect) const;

  // Layout lookup helpers — resolve a port to its widget position
  QPoint outputPortPos(OutputPort* port) const;
  QPoint inputPortPos(InputPort* port) const;
  bool isPortCardVisible(OutputPort* port) const;

  QColor badgeColor(Node* node) const;
  QColor portTypeColor(OutputPort* port) const;
  QColor portTypeColor(PortType type) const;
  QColor dimmed(const QColor& color) const; // blend toward background
  void updateDimming(); // recompute dimming based on current selection
  QPainterPath buildLinkPath(OutputPort* fromPort, InputPort* toPort,
                             int gutterX) const;
  QIcon stateIcon(Node* node) const;
  QIcon portTypeIcon(OutputPort* port) const;
  QRect breakpointRect(const QRect& cardRect) const;
  /// One slot to the left of the breakpoint. Occupied — and clickable
  /// — only while the node has periodic execution enabled.
  QRect autoExecuteRect(const QRect& cardRect) const;
  QRect menuButtonRect(const QRect& cardRect) const;
  QRect actionButtonRect(const QRect& cardRect) const;

  bool m_interactionLocked = false;
  Pipeline* m_pipeline = nullptr;
  SortOrder m_sortOrder{}; // SortOrder::Default
  QList<LayoutItem> m_layout;
  QList<LinkGeometry> m_linkGeometries;
  int m_selectedIndex = -1;
  OutputPort* m_selectedPort = nullptr; // selected output dot (collapsed nodes)
  Node* m_selectedMember = nullptr;    // selected group member (collapsed)
  OutputPort* m_tipOutputPort = nullptr;
  Link* m_selectedLink = nullptr;
  Link* m_hoveredLink = nullptr;
  QSet<Node*> m_expandedNodes;
  int m_hoveredIndex = -1;
  QTimer m_spinnerTimer;
  int m_spinnerAngle = 0;

  // Marching-ants link animation.
  QTimer m_marchTimer;
  qreal m_marchPhase = 0.0;
  bool m_animateLinkOnHover = true;
  bool m_animateLinkOnSelection = true;

  NodeMenuProvider m_nodeMenuProvider;
  PortMenuProvider m_portMenuProvider;
  LinkMenuProvider m_linkMenuProvider;
  LinkValidator m_linkValidator;

  // Interactive link creation drag state
  OutputPort* m_dragFromPort = nullptr;
  InputPort* m_dragToPort = nullptr;
  QPoint m_dragStartPos;
  QPoint m_dragCurrentPos;
  bool m_draggingLink = false;
  int m_gutterLaneCount = 0;
  QHash<OutputPort*, int> m_outputGutterLanes;
  QHash<InputPort*, int> m_inputGutterLanes;

  // Dimming state — we store the bright set; everything else is dimmed
  // when m_hasBrightSelection is true.
  qreal m_dimLevel = 0.75;
  bool m_dimmingEnabled = false;
  bool m_hasBrightSelection = false;
  QSet<Node*> m_brightNodes;
  QSet<OutputPort*> m_brightPorts;
  QSet<Link*> m_brightLinks;

  // Layout constants
  static constexpr int GutterWidth = 24;
  static constexpr int NodeCardHeight = 32;
  static constexpr int CardSpacing = 4;
  static constexpr int DirectConnectionSpacing = 4; // per-side spacing for straight lines
  static constexpr int OutputSquareOverlap = 4; // pixels of output square inside node
  static constexpr int PortIndent = 16;
  // Left/right/bottom padding around an expanded node card's content
  // (the port / member sub-cards). Distinct from PortIndent, which also
  // positions the input/output dots and shouldn't move with it.
  static constexpr int PortContentPad = PortIndent / 2;
  static constexpr int CardRadius = 4;
  static constexpr int BadgeSize = 16;
  static constexpr int DotRadius = 5;
  static constexpr int DotSpacing = 12; // center-to-center between adjacent dots
  static constexpr int DotMargin = 6;      // left margin for dots on node cards
  static constexpr int OutputSquareEdge = 20;
  static constexpr int OutputSquareRadius = 4;
  static constexpr int OutputSquareSpacing = 29; // center-to-center
  static constexpr int OutputSquareIconSize = 16;
  static constexpr int PortCardHeight = OutputSquareEdge; // match collapsed port square
  static constexpr int PortCardSpacing = CardSpacing + 2; // vertical gap between port cards
  // Icon size in node/member card headers. Capped by the group-member
  // cards, which are PortCardHeight (20) tall and center these icons —
  // anything above 16 leaves them less than 2 px of breathing room.
  static constexpr int HeaderIconSize = 16;
  static constexpr int HeaderRightPad = 4;  // right padding in node card header
  static constexpr int HeaderExpandWidth = 16; // expand toggle width
  static constexpr int HeaderButtonGap = 8; // gap between button groups (with separator)
  static constexpr int HeaderButtonSpacing = 2; // gap between adjacent buttons
  static constexpr int LaneSpacing = 6;              // spacing between parallel lines
  // How far a link runs straight out of a port before its first 90° turn.
  // Feeds DotClearance (input side) and SquareClearance (output side).
  static constexpr int PortClearance = 7;
  static constexpr int DotClearance = DotRadius + PortClearance;
  static constexpr int SquareClearance = OutputSquareEdge / 2 + PortClearance;
  static constexpr int LinkCornerRadius = 4;
  static constexpr int IndentWidth = 8;
  static constexpr int Padding = 4;
};

} // namespace pipeline
} // namespace tomviz

#endif
