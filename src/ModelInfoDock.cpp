#include "ModelInfoDock.h"

#include <QHeaderView>
#include <QFileInfo>
#include <QFont>

#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/BoundingBox>

ModelInfoDock::ModelInfoDock(QWidget* parent)
    : QDockWidget(tr("Info"), parent)
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

    setWidget(m_treeWidget);
}

// ---------- helpers ----------

QTreeWidgetItem* ModelInfoDock::addGroup(const QString& title)
{
    QTreeWidgetItem* g = new QTreeWidgetItem(m_treeWidget, QStringList() << title);
    QFont f;
    f.setBold(true);
    g->setFont(0, f);
    g->setExpanded(true);
    return g;
}

void ModelInfoDock::addKV(QTreeWidgetItem* group, const QString& key, const QString& value)
{
    QTreeWidgetItem* it = new QTreeWidgetItem(group, QStringList() << key << value);
    it->setToolTip(1, value);
}

QString ModelInfoDock::formatBytes(qint64 bytes)
{
    if (bytes <= 0) return "0 B";
    if (bytes > 1024 * 1024 * 1024)
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    if (bytes > 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    if (bytes > 1024)
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    return QString("%1 B").arg(bytes);
}

QString ModelInfoDock::pixelFormatName(unsigned int glFormat)
{
    switch (glFormat)
    {
    case GL_RGB:             return "RGB";
    case GL_RGBA:            return "RGBA";
    case GL_BGR:             return "BGR";
    case GL_BGRA:            return "BGRA";
    case GL_LUMINANCE:       return "LUMINANCE";
    case GL_LUMINANCE_ALPHA: return "LUMINANCE_ALPHA";
    case GL_ALPHA:           return "ALPHA";
    case GL_RED:             return "RED";
    default:                 return QString("0x%1").arg(glFormat, 4, 16, QChar('0'));
    }
}

void ModelInfoDock::clearSelectionGroup()
{
    if (m_selectionGroup)
    {
        const int idx = m_treeWidget->indexOfTopLevelItem(m_selectionGroup);
        if (idx >= 0)
        {
            delete m_treeWidget->takeTopLevelItem(idx);
        }
        m_selectionGroup = nullptr;
    }
}

// ---------- common ----------

void ModelInfoDock::clearInfo()
{
    if (m_treeWidget) m_treeWidget->clear();
    m_selectionGroup = nullptr;
}

// ---------- 3D model ----------

void ModelInfoDock::updateInfo(const ModelInfo& info)
{
    clearInfo();

    QTreeWidgetItem* fileGroup = addGroup(tr("File Info"));
    addKV(fileGroup, tr("File Name"), info.fileName);
    addKV(fileGroup, tr("File Path"), info.filePath);
    addKV(fileGroup, tr("File Size"), formatBytes(info.fileSize));
    addKV(fileGroup, tr("Format"),    info.format);
    addKV(fileGroup, tr("Loader"),    info.loaderUsed);

    QTreeWidgetItem* geoGroup = addGroup(tr("Geometry"));
    addKV(geoGroup, tr("Vertices"),  QString::number(info.vertexCount));
    addKV(geoGroup, tr("Faces"),     QString::number(info.faceCount));
    addKV(geoGroup, tr("Meshes"),    QString::number(info.meshCount));
    addKV(geoGroup, tr("Materials"), QString::number(info.materialCount));
    addKV(geoGroup, tr("Textures"),  QString::number(info.textureCount));

    QTreeWidgetItem* bbGroup = addGroup(tr("Bounding Box"));
    addKV(bbGroup, tr("Min"),  QString("(%1, %2, %3)")
        .arg(info.boundingBoxMinX, 0, 'f', 4)
        .arg(info.boundingBoxMinY, 0, 'f', 4)
        .arg(info.boundingBoxMinZ, 0, 'f', 4));
    addKV(bbGroup, tr("Max"),  QString("(%1, %2, %3)")
        .arg(info.boundingBoxMaxX, 0, 'f', 4)
        .arg(info.boundingBoxMaxY, 0, 'f', 4)
        .arg(info.boundingBoxMaxZ, 0, 'f', 4));
    addKV(bbGroup, tr("Size"), QString("(%1, %2, %3)")
        .arg(info.boundingBoxSizeX, 0, 'f', 4)
        .arg(info.boundingBoxSizeY, 0, 'f', 4)
        .arg(info.boundingBoxSizeZ, 0, 'f', 4));
}

void ModelInfoDock::updateNodeInfo(osg::Node* node)
{
    clearSelectionGroup();
    if (!node) return;

    m_selectionGroup = addGroup(tr("Selected Node"));

    const std::string nameStr = node->getName();
    addKV(m_selectionGroup, tr("Name"),
          nameStr.empty() ? tr("(unnamed)") : QString::fromStdString(nameStr));

    const char* typeName = node->className();
    addKV(m_selectionGroup, tr("Type"), typeName ? QString(typeName) : "Node");

    const osg::Group* group = node->asGroup();
    addKV(m_selectionGroup, tr("Children"),
          group ? QString::number(group->getNumChildren()) : "0");
    addKV(m_selectionGroup, tr("Node Mask"),
          QString("0x%1").arg(node->getNodeMask(), 4, 16, QChar('0')));

    unsigned int vertexCount = 0, faceCount = 0, drawableCount = 0;
    collectNodeStats(node, vertexCount, faceCount, drawableCount);
    addKV(m_selectionGroup, tr("Drawables"), QString::number(drawableCount));
    addKV(m_selectionGroup, tr("Vertices"),  QString::number(vertexCount));
    addKV(m_selectionGroup, tr("Faces"),     QString::number(faceCount));

    osg::BoundingBox bb;
    bb.expandBy(node->computeBound());
    if (bb.valid())
    {
        addKV(m_selectionGroup, tr("BBox Min"),  QString("(%1, %2, %3)")
            .arg(bb.xMin(), 0, 'f', 4).arg(bb.yMin(), 0, 'f', 4).arg(bb.zMin(), 0, 'f', 4));
        addKV(m_selectionGroup, tr("BBox Max"),  QString("(%1, %2, %3)")
            .arg(bb.xMax(), 0, 'f', 4).arg(bb.yMax(), 0, 'f', 4).arg(bb.zMax(), 0, 'f', 4));
        addKV(m_selectionGroup, tr("BBox Size"), QString("(%1, %2, %3)")
            .arg(bb.xMax() - bb.xMin(), 0, 'f', 4)
            .arg(bb.yMax() - bb.yMin(), 0, 'f', 4)
            .arg(bb.zMax() - bb.zMin(), 0, 'f', 4));
    }
}

// ---------- image / raster ----------

void ModelInfoDock::showImageInfo(const QString& filePath, const osg::Image* image)
{
    clearInfo();

    const QFileInfo fi(filePath);
    QTreeWidgetItem* fileGroup = addGroup(tr("File Info"));
    addKV(fileGroup, tr("File Name"), fi.fileName());
    addKV(fileGroup, tr("File Path"), fi.absoluteFilePath());
    addKV(fileGroup, tr("File Size"), formatBytes(fi.size()));
    addKV(fileGroup, tr("Format"),    fi.suffix().toUpper());

    QTreeWidgetItem* imgGroup = addGroup(tr("Image"));
    if (image)
    {
        addKV(imgGroup, tr("Width"),         QString::number(image->s()));
        addKV(imgGroup, tr("Height"),        QString::number(image->t()));
        addKV(imgGroup, tr("Pixel Format"),  pixelFormatName(image->getPixelFormat()));
        addKV(imgGroup, tr("Pixel Size"),    QString("%1 bit").arg(image->getPixelSizeInBits()));
        addKV(imgGroup, tr("Image Size"),    formatBytes(image->getTotalSizeInBytes()));
    }
    else
    {
        addKV(imgGroup, tr("Status"), tr("(no image)"));
    }
}

void ModelInfoDock::showRasterInfo(const QString& filePath, const GdalRasterMeta& meta, const osg::Image* image)
{
    clearInfo();

    const QFileInfo fi(filePath);
    QTreeWidgetItem* fileGroup = addGroup(tr("File Info"));
    addKV(fileGroup, tr("File Name"), fi.fileName());
    addKV(fileGroup, tr("File Path"), fi.absoluteFilePath());
    addKV(fileGroup, tr("File Size"), formatBytes(fi.size()));
    addKV(fileGroup, tr("Format"),    fi.suffix().toUpper());
    addKV(fileGroup, tr("Loader"),    "GDAL");

    QTreeWidgetItem* rasGroup = addGroup(tr("Raster"));
    addKV(rasGroup, tr("Width"),       QString::number(meta.width));
    addKV(rasGroup, tr("Height"),      QString::number(meta.height));
    addKV(rasGroup, tr("Bands"),       QString::number(meta.bandCount));
    addKV(rasGroup, tr("Data Type"),   meta.dataType);
    addKV(rasGroup, tr("Pixel Size X"), QString::number(meta.pixelSizeX, 'g', 8));
    addKV(rasGroup, tr("Pixel Size Y"), QString::number(meta.pixelSizeY, 'g', 8));
    addKV(rasGroup, tr("CRS"),         meta.crs.isEmpty() ? tr("(none)") : meta.crs);

    if (image)
    {
        QTreeWidgetItem* dispGroup = addGroup(tr("Display"));
        addKV(dispGroup, tr("Width"),        QString::number(image->s()));
        addKV(dispGroup, tr("Height"),       QString::number(image->t()));
        addKV(dispGroup, tr("Pixel Format"), pixelFormatName(image->getPixelFormat()));
        addKV(dispGroup, tr("Image Size"),   formatBytes(image->getTotalSizeInBytes()));
    }
}

// ---------- vector ----------

void ModelInfoDock::showVectorInfo(const GdalVectorData& data)
{
    clearInfo();

    const QFileInfo fi(data.filePath);
    QTreeWidgetItem* fileGroup = addGroup(tr("File Info"));
    addKV(fileGroup, tr("File Name"), fi.fileName());
    addKV(fileGroup, tr("File Path"), fi.absoluteFilePath());
    addKV(fileGroup, tr("File Size"), formatBytes(fi.size()));
    addKV(fileGroup, tr("Format"),    data.format.isEmpty() ? fi.suffix().toUpper() : data.format);
    addKV(fileGroup, tr("Loader"),    "GDAL/OGR");

    QTreeWidgetItem* vecGroup = addGroup(tr("Vector"));
    addKV(vecGroup, tr("Geometry Type"),   data.geomType);
    addKV(vecGroup, tr("Layer Count"),     QString::number(data.layerCount));
    addKV(vecGroup, tr("Feature Count"),   QString::number(data.featureCount));
    addKV(vecGroup, tr("Displayed Count"),
          data.truncated
              ? tr("%1 (truncated)").arg(data.displayedCount)
              : QString::number(data.displayedCount));
    addKV(vecGroup, tr("CRS"), data.crs.isEmpty() ? tr("(none)") : data.crs);

    QTreeWidgetItem* extGroup = addGroup(tr("Extent"));
    addKV(extGroup, tr("Min X"), QString::number(data.minX, 'f', 6));
    addKV(extGroup, tr("Min Y"), QString::number(data.minY, 'f', 6));
    addKV(extGroup, tr("Max X"), QString::number(data.maxX, 'f', 6));
    addKV(extGroup, tr("Max Y"), QString::number(data.maxY, 'f', 6));
    addKV(extGroup, tr("Width"),  QString::number(data.maxX - data.minX, 'f', 6));
    addKV(extGroup, tr("Height"), QString::number(data.maxY - data.minY, 'f', 6));
}

void ModelInfoDock::showVectorFeatureInfo(const GdalFeature& feat)
{
    clearSelectionGroup();
    m_selectionGroup = addGroup(tr("Selected Feature"));

    addKV(m_selectionGroup, tr("FID"),
          feat.fid < 0 ? tr("(unknown)") : QString::number(feat.fid));
    if (!feat.layerName.isEmpty())
        addKV(m_selectionGroup, tr("Layer"), feat.layerName);
    if (!feat.geometryType.isEmpty())
        addKV(m_selectionGroup, tr("Geometry"), feat.geometryType);

    if (feat.isPoint)
    {
        addKV(m_selectionGroup, tr("Coord"),
              QString("(%1, %2)").arg(feat.point.x(), 0, 'f', 6).arg(feat.point.y(), 0, 'f', 6));
    }
    else
    {
        int totalPts = 0;
        for (const QPolygonF& r : feat.rings) totalPts += r.size();
        addKV(m_selectionGroup, tr("Rings"),  QString::number(feat.rings.size()));
        addKV(m_selectionGroup, tr("Points"), QString::number(totalPts));
    }

    if (!feat.attributes.isEmpty())
    {
        QTreeWidgetItem* attrGroup = new QTreeWidgetItem(m_selectionGroup, QStringList() << tr("Attributes"));
        attrGroup->setExpanded(true);
        QFont f; f.setBold(true);
        attrGroup->setFont(0, f);
        for (const auto& kv : feat.attributes)
        {
            QTreeWidgetItem* it = new QTreeWidgetItem(attrGroup, QStringList() << kv.first << kv.second);
            it->setToolTip(1, kv.second);
        }
    }
}

// ---------- node stats ----------

void ModelInfoDock::collectNodeStats(const osg::Node* node, unsigned int& vertexCount, unsigned int& faceCount, unsigned int& drawableCount)
{
    if (!node) return;

    const osg::Geode* geode = node->asGeode();
    if (geode)
    {
        for (unsigned int i = 0; i < geode->getNumDrawables(); ++i)
        {
            drawableCount++;
            const osg::Drawable* drawable = geode->getDrawable(i);
            if (!drawable) continue;
            const osg::Geometry* geom = drawable->asGeometry();
            if (!geom) continue;

            const osg::Array* verts = geom->getVertexArray();
            if (verts) vertexCount += verts->getNumElements();

            for (unsigned int p = 0; p < geom->getNumPrimitiveSets(); ++p)
            {
                const osg::PrimitiveSet* ps = geom->getPrimitiveSet(p);
                if (!ps) continue;
                switch (ps->getMode())
                {
                case GL_TRIANGLES:      faceCount += ps->getNumIndices() / 3; break;
                case GL_TRIANGLE_STRIP:
                case GL_TRIANGLE_FAN:   faceCount += ps->getNumIndices() > 2 ? (ps->getNumIndices() - 2) : 0; break;
                case GL_QUADS:          faceCount += ps->getNumIndices() / 4 * 2; break;
                case GL_POLYGON:        faceCount += 1; break;
                default: break;
                }
            }
        }
    }

    const osg::Group* group = node->asGroup();
    if (group)
    {
        for (unsigned int i = 0; i < group->getNumChildren(); ++i)
        {
            collectNodeStats(group->getChild(i), vertexCount, faceCount, drawableCount);
        }
    }
}
