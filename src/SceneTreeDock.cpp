#include "SceneTreeDock.h"
#include "OSGWidget.h"

#include <osg/Node>
#include <osg/Group>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/LOD>
#include <osg/Switch>
#include <osg/ProxyNode>

#include <QHeaderView>
#include <QItemSelectionModel>

SceneTreeDock::SceneTreeDock(QWidget* parent)
    : QDockWidget(tr("Scene Tree"), parent)
{
    setupUI();
}

void SceneTreeDock::setupUI()
{
    m_treeView = new QTreeView(this);
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(QStringList() << tr("Node") << tr("Type"));

    m_treeView->setModel(m_model);
    m_treeView->setAlternatingRowColors(false);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(m_treeView, &QTreeView::clicked, this, &SceneTreeDock::onItemClicked);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        // Also trigger when keyboard navigation changes the current item
        if (current.isValid())
        {
            QModelIndex nameIndex = m_model->index(current.row(), 0, current.parent());
            QVariant data = m_model->data(nameIndex, Qt::UserRole);
            if (data.isValid())
            {
                osg::Node* node = reinterpret_cast<osg::Node*>(data.value<quintptr>());
                if (node)
                {
                    emit nodeSelected(node);
                }
            }
        }
    });

    setWidget(m_treeView);
}

void SceneTreeDock::updateTree(osg::Node* node)
{
    m_model->clear();
    m_model->setHorizontalHeaderLabels(QStringList() << tr("Node") << tr("Type"));

    if (node)
    {
        buildTree(node, m_model->invisibleRootItem());
        m_treeView->expandToDepth(2);
    }
}

void SceneTreeDock::clearTree()
{
    m_model->clear();
    m_model->setHorizontalHeaderLabels(QStringList() << tr("Node") << tr("Type"));
}

void SceneTreeDock::buildTree(const osg::Node* node, QStandardItem* parentItem)
{
    if (!node) return;

    // Skip highlight overlay nodes (mask is exactly HIGHLIGHT_NODE_MASK = 0x80000000).
    // Normal nodes have mask 0xFFFFFFFF which also has this bit set,
    // so we must use == not & to only filter overlay nodes.
    if (node->getNodeMask() == OSGWidget::HIGHLIGHT_NODE_MASK)
        return;

    QString name = node->getName().empty() ? tr("(unnamed)") : QString::fromStdString(node->getName());

    // Determine type string
    QString typeStr;
    const char* typeName = node->className();
    if (typeName)
    {
        typeStr = QString(typeName);
    }
    else
    {
        typeStr = "Node";
    }

    QStandardItem* nameItem = new QStandardItem(name);
    QStandardItem* typeItem = new QStandardItem(typeStr);

    // Store the node pointer for selection
    QVariant nodePtr = QVariant::fromValue(reinterpret_cast<quintptr>(node));
    nameItem->setData(nodePtr, Qt::UserRole);

    QList<QStandardItem*> rowItems;
    rowItems << nameItem << typeItem;
    parentItem->appendRow(rowItems);

    // If it's a group, recurse into children
    const osg::Group* group = node->asGroup();
    if (group)
    {
        for (unsigned int i = 0; i < group->getNumChildren(); ++i)
        {
            buildTree(group->getChild(i), nameItem);
        }
    }
}

void SceneTreeDock::onItemClicked(const QModelIndex& index)
{
    QModelIndex nameIndex = m_model->index(index.row(), 0, index.parent());
    QVariant data = m_model->data(nameIndex, Qt::UserRole);
    if (data.isValid())
    {
        osg::Node* node = reinterpret_cast<osg::Node*>(data.value<quintptr>());
        if (node)
        {
            emit nodeSelected(node);
        }
    }
}

void SceneTreeDock::selectNode(osg::Node* node)
{
    if (!node) {
        m_treeView->clearSelection();
        return;
    }
    QModelIndex idx = findIndexForNode(node);
    if (idx.isValid())
    {
        m_treeView->setCurrentIndex(idx);
        m_treeView->scrollTo(idx, QAbstractItemView::EnsureVisible);
    }
}

QModelIndex SceneTreeDock::findIndexForNode(osg::Node* node, const QModelIndex& parent) const
{
    quintptr targetPtr = reinterpret_cast<quintptr>(node);

    int rowCount = m_model->rowCount(parent);
    for (int r = 0; r < rowCount; ++r)
    {
        QModelIndex nameIdx = m_model->index(r, 0, parent);
        QVariant data = m_model->data(nameIdx, Qt::UserRole);
        if (data.isValid() && data.value<quintptr>() == targetPtr)
        {
            return nameIdx;
        }
        // Recurse into children
        QModelIndex childResult = findIndexForNode(node, nameIdx);
        if (childResult.isValid())
            return childResult;
    }
    return QModelIndex();
}
