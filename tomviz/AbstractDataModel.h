/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __AbstractDataModel_h
#define __AbstractDataModel_h
#include <QAbstractItemModel>
#include <QTreeWidgetItem>

/**
 * @brief Abstract class implementing a model (table or tree) with basic
 * functionality.
 *
 * To use, derive and implement initializeRootItem and populate its nodes.
 */
class AbstractDataModel : public QAbstractItemModel
{
  Q_OBJECT
public:
  /**
   * A default model index might be useful to initialize selection in a view.
   */
  const QModelIndex getDefaultIndex();

protected:
  AbstractDataModel(QObject* parent_ = nullptr);
  virtual ~AbstractDataModel();

  /**
  * @{
  * QAbstractItemModel implementation
  */
  int rowCount(const QModelIndex& parent_ = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent_ = QModelIndex()) const override;

  QModelIndex index(int row, int column, const QModelIndex& parent_ = QModelIndex()) const override;

  QModelIndex parent(const QModelIndex& index_) const override;
  QVariant data(const QModelIndex& index_, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index_, const QVariant& value, int role) override;

  QVariant headerData(
    int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  Qt::ItemFlags flags(const QModelIndex& index_) const override;

  bool removeRows(int row, int count, const QModelIndex& parent) override;
  /**
  * @}
  */

  /**
   * Convenience function to query the internal item of an index. Checks the
   * validity of index and returns RootItem if invalid (QAbstractModelItem
   * expects invalid QModelIndex() instances to refer to the RootItem).
   */
  QTreeWidgetItem* getItem(const QModelIndex& index) const;

  /**
  * Construct the root element. This is the element holding the header tags
  * so these should be initialized here. Concrete classes should implement this
  * method as it is up to them to decide the concrete time of element
  * (QTreeWidgetItem subclasses) to use.
  */
  virtual void initializeRootItem() = 0;

  /**
  * More comprehensive validation. In addition to the standard QModelIndex::isValid
  * it checks the upper bounds. Because it internally calls QModelIndex::parent()
  * (and thus QAbstractItemMode::parent()) it should never be called from within
  * parent().
  */
  bool isIndexValidUpperBound(const QModelIndex& index_) const;

  QTreeWidgetItem* RootItem = nullptr;

private:
  void operator=(const AbstractDataModel&) = delete;
  AbstractDataModel(const AbstractDataModel&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
/**
 * \brief Qt Data model item holding a reference to custom model-data.
 *
 */
template <typename T>
class DataItem : public QTreeWidgetItem
{
public:
  DataItem(QTreeWidgetItem* parent = nullptr);
  ~DataItem() = default;

  void setReferencedData(const T& data);

  /**
   * Get the actual underlying data referenced by this element in the
   * data model.
   */
  const T& getReferencedDataConst() const;

private:
  DataItem(const DataItem&) = delete;
  void operator=(const DataItem&) = delete;

  /**
   * Copy of the underlying referenced data.
   */
  T m_data;
};

template <typename T>
DataItem<T>::DataItem(QTreeWidgetItem* parent)
  : QTreeWidgetItem(parent){}

template <typename T>
void DataItem<T>::setReferencedData(const T& data)
{
  m_data = data;
}

template <typename T>
const T& DataItem<T>::getReferencedDataConst() const
{
  return m_data;
}
#endif //__AbstractDataModel_h
