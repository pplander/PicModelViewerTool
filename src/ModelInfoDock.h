#pragma once

#include <QDockWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>

#include "ModelLoader.h"

#include <osg/Node>
#include <osg/Geode>
#include <osg/Geometry>

class ModelInfoDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ModelInfoDock(QWidget* parent = nullptr);

    void updateInfo(const ModelInfo& info);
    void clearInfo();
    void updateNodeInfo(osg::Node* node);

private:
    void setupUI();

    QTreeWidget* m_treeWidget;

    // File info items
    QTreeWidgetItem* m_fileNameItem;
    QTreeWidgetItem* m_filePathItem;
    QTreeWidgetItem* m_fileSizeItem;
    QTreeWidgetItem* m_formatItem;
    QTreeWidgetItem* m_loaderItem;

    // Model geometry info items
    QTreeWidgetItem* m_vertexCountItem;
    QTreeWidgetItem* m_faceCountItem;
    QTreeWidgetItem* m_meshCountItem;
    QTreeWidgetItem* m_materialCountItem;
    QTreeWidgetItem* m_textureCountItem;

    // Model bounding box items
    QTreeWidgetItem* m_bbMinItem;
    QTreeWidgetItem* m_bbMaxItem;
    QTreeWidgetItem* m_bbSizeItem;

    // Selected node info group
    QTreeWidgetItem* m_nodeInfoGroup;
    QTreeWidgetItem* m_nodeNameItem;
    QTreeWidgetItem* m_nodeTypeItem;
    QTreeWidgetItem* m_nodeChildrenItem;
    QTreeWidgetItem* m_nodeDescItem;

    // Selected node geometry info group (top-level)
    QTreeWidgetItem* m_nodeGeoGroup;
    QTreeWidgetItem* m_nodeDrawableItem;
    QTreeWidgetItem* m_nodeVertexItem;
    QTreeWidgetItem* m_nodeFaceItem;

    // Selected node bounding box group (top-level)
    QTreeWidgetItem* m_nodeBbGroup;
    QTreeWidgetItem* m_nodeBbMinItem;
    QTreeWidgetItem* m_nodeBbMaxItem;
    QTreeWidgetItem* m_nodeBbSizeItem;

    void collectNodeStats(const osg::Node* node, unsigned int& vertexCount, unsigned int& faceCount, unsigned int& drawableCount);

    bool m_hasNodeSelected = false;
};
