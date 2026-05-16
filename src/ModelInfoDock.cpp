#include "ModelInfoDock.h"

#include <QHeaderView>
#include <QFileInfo>

#include <osg/Group>
#include <osg/Geode>
#include <osg/BoundingBox>

ModelInfoDock::ModelInfoDock(QWidget* parent)
    : QDockWidget(tr("Model Info"), parent)
{
    setupUI();
}

void ModelInfoDock::setupUI()
{
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels(QStringList() << tr("Property") << tr("Value"));
    m_treeWidget->setAlternatingRowColors(false);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setStretchLastSection(true);

    QFont boldFont;
    boldFont.setBold(true);

    // File info group
    QTreeWidgetItem* fileGroup = new QTreeWidgetItem(m_treeWidget, QStringList() << tr("File Info"));
    fileGroup->setExpanded(true);
    fileGroup->setFont(0, boldFont);

    m_fileNameItem = new QTreeWidgetItem(fileGroup, QStringList() << tr("File Name") << "");
    m_filePathItem = new QTreeWidgetItem(fileGroup, QStringList() << tr("File Path") << "");
    m_fileSizeItem = new QTreeWidgetItem(fileGroup, QStringList() << tr("File Size") << "");
    m_formatItem = new QTreeWidgetItem(fileGroup, QStringList() << tr("Format") << "");
    m_loaderItem = new QTreeWidgetItem(fileGroup, QStringList() << tr("Loader") << "");

    // Model geometry info group
    QTreeWidgetItem* geoGroup = new QTreeWidgetItem(m_treeWidget, QStringList() << tr("Geometry"));
    geoGroup->setExpanded(true);
    geoGroup->setFont(0, boldFont);

    m_vertexCountItem = new QTreeWidgetItem(geoGroup, QStringList() << tr("Vertices") << "");
    m_faceCountItem = new QTreeWidgetItem(geoGroup, QStringList() << tr("Faces") << "");
    m_meshCountItem = new QTreeWidgetItem(geoGroup, QStringList() << tr("Meshes") << "");
    m_materialCountItem = new QTreeWidgetItem(geoGroup, QStringList() << tr("Materials") << "");
    m_textureCountItem = new QTreeWidgetItem(geoGroup, QStringList() << tr("Textures") << "");

    // Model bounding box group
    QTreeWidgetItem* bbGroup = new QTreeWidgetItem(m_treeWidget, QStringList() << tr("Bounding Box"));
    bbGroup->setExpanded(true);
    bbGroup->setFont(0, boldFont);

    m_bbMinItem = new QTreeWidgetItem(bbGroup, QStringList() << tr("Min") << "");
    m_bbMaxItem = new QTreeWidgetItem(bbGroup, QStringList() << tr("Max") << "");
    m_bbSizeItem = new QTreeWidgetItem(bbGroup, QStringList() << tr("Size") << "");

    // Selected node info group (top-level, hidden by default)
    m_nodeInfoGroup = new QTreeWidgetItem(m_treeWidget, QStringList() << tr("Selected Node"));
    m_nodeInfoGroup->setExpanded(true);
    m_nodeInfoGroup->setFont(0, boldFont);
    m_nodeInfoGroup->setHidden(true);

    m_nodeNameItem = new QTreeWidgetItem(m_nodeInfoGroup, QStringList() << tr("Name") << "");
    m_nodeTypeItem = new QTreeWidgetItem(m_nodeInfoGroup, QStringList() << tr("Type") << "");
    m_nodeChildrenItem = new QTreeWidgetItem(m_nodeInfoGroup, QStringList() << tr("Children") << "");
    m_nodeDescItem = new QTreeWidgetItem(m_nodeInfoGroup, QStringList() << tr("Node Mask") << "");

    // Selected node geometry info group (top-level, hidden by default)
    m_nodeGeoGroup = new QTreeWidgetItem(m_treeWidget, QStringList() << tr("Node Geometry"));
    m_nodeGeoGroup->setExpanded(true);
    m_nodeGeoGroup->setFont(0, boldFont);
    m_nodeGeoGroup->setHidden(true);

    m_nodeDrawableItem = new QTreeWidgetItem(m_nodeGeoGroup, QStringList() << tr("Drawables") << "");
    m_nodeVertexItem = new QTreeWidgetItem(m_nodeGeoGroup, QStringList() << tr("Vertices") << "");
    m_nodeFaceItem = new QTreeWidgetItem(m_nodeGeoGroup, QStringList() << tr("Faces") << "");

    // Selected node bounding box group (top-level, hidden by default)
    m_nodeBbGroup = new QTreeWidgetItem(m_treeWidget, QStringList() << tr("Node Bounding Box"));
    m_nodeBbGroup->setExpanded(true);
    m_nodeBbGroup->setFont(0, boldFont);
    m_nodeBbGroup->setHidden(true);

    m_nodeBbMinItem = new QTreeWidgetItem(m_nodeBbGroup, QStringList() << tr("Min") << "");
    m_nodeBbMaxItem = new QTreeWidgetItem(m_nodeBbGroup, QStringList() << tr("Max") << "");
    m_nodeBbSizeItem = new QTreeWidgetItem(m_nodeBbGroup, QStringList() << tr("Size") << "");

    setWidget(m_treeWidget);
}

void ModelInfoDock::updateInfo(const ModelInfo& info)
{
    m_hasNodeSelected = false;

    m_fileNameItem->setText(1, info.fileName);
    m_filePathItem->setText(1, info.filePath);
    m_filePathItem->setToolTip(1, info.filePath);

    // Format file size
    QString sizeStr;
    if (info.fileSize > 1024 * 1024)
    {
        sizeStr = QString("%1 MB").arg(info.fileSize / (1024.0 * 1024.0), 0, 'f', 2);
    }
    else if (info.fileSize > 1024)
    {
        sizeStr = QString("%1 KB").arg(info.fileSize / 1024.0, 0, 'f', 2);
    }
    else
    {
        sizeStr = QString("%1 B").arg(info.fileSize);
    }
    m_fileSizeItem->setText(1, sizeStr);

    m_formatItem->setText(1, info.format);
    m_loaderItem->setText(1, info.loaderUsed);

    m_vertexCountItem->setText(1, QString::number(info.vertexCount));
    m_faceCountItem->setText(1, QString::number(info.faceCount));
    m_meshCountItem->setText(1, QString::number(info.meshCount));
    m_materialCountItem->setText(1, QString::number(info.materialCount));
    m_textureCountItem->setText(1, QString::number(info.textureCount));

    m_bbMinItem->setText(1, QString("(%1, %2, %3)")
        .arg(info.boundingBoxMinX, 0, 'f', 4)
        .arg(info.boundingBoxMinY, 0, 'f', 4)
        .arg(info.boundingBoxMinZ, 0, 'f', 4));

    m_bbMaxItem->setText(1, QString("(%1, %2, %3)")
        .arg(info.boundingBoxMaxX, 0, 'f', 4)
        .arg(info.boundingBoxMaxY, 0, 'f', 4)
        .arg(info.boundingBoxMaxZ, 0, 'f', 4));

    m_bbSizeItem->setText(1, QString("(%1, %2, %3)")
        .arg(info.boundingBoxSizeX, 0, 'f', 4)
        .arg(info.boundingBoxSizeY, 0, 'f', 4)
        .arg(info.boundingBoxSizeZ, 0, 'f', 4));

    m_nodeInfoGroup->setHidden(true);
    m_nodeGeoGroup->setHidden(true);
    m_nodeBbGroup->setHidden(true);
}

void ModelInfoDock::clearInfo()
{
    m_hasNodeSelected = false;
    updateInfo(ModelInfo());
    m_nodeInfoGroup->setHidden(true);
    m_nodeGeoGroup->setHidden(true);
    m_nodeBbGroup->setHidden(true);
}

void ModelInfoDock::updateNodeInfo(osg::Node* node)
{
    if (!node)
    {
        m_nodeInfoGroup->setHidden(true);
        m_nodeGeoGroup->setHidden(true);
        m_nodeBbGroup->setHidden(true);
        m_hasNodeSelected = false;
        return;
    }

    m_hasNodeSelected = true;
    m_nodeInfoGroup->setHidden(false);
    m_nodeGeoGroup->setHidden(false);
    m_nodeBbGroup->setHidden(false);

    // Name
    std::string nameStr = node->getName();
    m_nodeNameItem->setText(1, nameStr.empty() ? tr("(unnamed)") : QString::fromStdString(nameStr));

    // Type
    const char* typeName = node->className();
    m_nodeTypeItem->setText(1, typeName ? QString(typeName) : "Node");

    // Children count
    const osg::Group* group = node->asGroup();
    m_nodeChildrenItem->setText(1, group ? QString::number(group->getNumChildren()) : "0");

    // Node Mask
    m_nodeDescItem->setText(1, QString("0x%1").arg(node->getNodeMask(), 4, 16, QChar('0')));

    // Geometry stats of selected node
    unsigned int vertexCount = 0, faceCount = 0, drawableCount = 0;
    collectNodeStats(node, vertexCount, faceCount, drawableCount);
    m_nodeDrawableItem->setText(1, QString::number(drawableCount));
    m_nodeVertexItem->setText(1, QString::number(vertexCount));
    m_nodeFaceItem->setText(1, QString::number(faceCount));

    // Bounding box of selected node
    osg::BoundingBox bb;
    bb.expandBy(node->computeBound());
    if (bb.valid())
    {
        m_nodeBbMinItem->setText(1, QString("(%1, %2, %3)")
            .arg(bb.xMin(), 0, 'f', 4)
            .arg(bb.yMin(), 0, 'f', 4)
            .arg(bb.zMin(), 0, 'f', 4));
        m_nodeBbMaxItem->setText(1, QString("(%1, %2, %3)")
            .arg(bb.xMax(), 0, 'f', 4)
            .arg(bb.yMax(), 0, 'f', 4)
            .arg(bb.zMax(), 0, 'f', 4));
        m_nodeBbSizeItem->setText(1, QString("(%1, %2, %3)")
            .arg(bb.xMax() - bb.xMin(), 0, 'f', 4)
            .arg(bb.yMax() - bb.yMin(), 0, 'f', 4)
            .arg(bb.zMax() - bb.zMin(), 0, 'f', 4));
    }
    else
    {
        m_nodeBbMinItem->setText(1, tr("N/A"));
        m_nodeBbMaxItem->setText(1, tr("N/A"));
        m_nodeBbSizeItem->setText(1, tr("N/A"));
    }
}

void ModelInfoDock::collectNodeStats(const osg::Node* node, unsigned int& vertexCount, unsigned int& faceCount, unsigned int& drawableCount)
{
    if (!node) return;

    // If this node is a Geode, count its drawables
    const osg::Geode* geode = node->asGeode();
    if (geode)
    {
        for (unsigned int i = 0; i < geode->getNumDrawables(); ++i)
        {
            drawableCount++;
            const osg::Drawable* drawable = geode->getDrawable(i);
            if (drawable)
            {
                const osg::Geometry* geom = drawable->asGeometry();
                if (geom)
                {
                    const osg::Array* verts = geom->getVertexArray();
                    if (verts)
                        vertexCount += verts->getNumElements();
                    for (unsigned int p = 0; p < geom->getNumPrimitiveSets(); ++p)
                    {
                        const osg::PrimitiveSet* ps = geom->getPrimitiveSet(p);
                        if (ps)
                        {
                            switch (ps->getMode())
                            {
                            case GL_TRIANGLES:
                                faceCount += ps->getNumIndices() / 3;
                                break;
                            case GL_TRIANGLE_STRIP:
                                faceCount += ps->getNumIndices() > 2 ? (ps->getNumIndices() - 2) : 0;
                                break;
                            case GL_TRIANGLE_FAN:
                                faceCount += ps->getNumIndices() > 2 ? (ps->getNumIndices() - 2) : 0;
                                break;
                            case GL_QUADS:
                                faceCount += ps->getNumIndices() / 4 * 2;
                                break;
                            case GL_POLYGON:
                                faceCount += 1;
                                break;
                            default:
                                // Lines, points, etc. - not faces
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Recurse into children
    const osg::Group* group = node->asGroup();
    if (group)
    {
        for (unsigned int i = 0; i < group->getNumChildren(); ++i)
        {
            collectNodeStats(group->getChild(i), vertexCount, faceCount, drawableCount);
        }
    }
}
