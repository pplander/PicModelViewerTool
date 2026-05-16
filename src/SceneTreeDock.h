#pragma once

#include <QDockWidget>
#include <QTreeView>
#include <QStandardItemModel>

#include <osg/Node>

class SceneTreeDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit SceneTreeDock(QWidget* parent = nullptr);

    void updateTree(osg::Node* node);
    void clearTree();
    void selectNode(osg::Node* node);

signals:
    void nodeSelected(osg::Node* node);

private:
    void setupUI();
    void buildTree(const osg::Node* node, QStandardItem* parentItem);
    void onItemClicked(const QModelIndex& index);
    QModelIndex findIndexForNode(osg::Node* node, const QModelIndex& parent = QModelIndex()) const;

    QTreeView* m_treeView;
    QStandardItemModel* m_model;
};
