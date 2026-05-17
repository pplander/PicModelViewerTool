#pragma once

#include <QDockWidget>
#include <QTreeWidget>

#include "ModelLoader.h"
#include "GdalRasterLoader.h"
#include "GdalVectorData.h"

#include <osg/Node>
#include <osg/Image>

// General-purpose info dock.
// Default content is empty; populated based on the currently loaded asset type:
//   - 3D model        : updateInfo(ModelInfo) + updateNodeInfo(selected node)
//   - Image / Raster  : showImageInfo(...) / showRasterInfo(...)
//   - Vector dataset  : showVectorInfo(...) + showVectorFeatureInfo(selected feature)
class ModelInfoDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ModelInfoDock(QWidget* parent = nullptr);

    // Common
    void clearInfo();

    // 3D model
    void updateInfo(const ModelInfo& info);
    void updateNodeInfo(osg::Node* node);

    // 2D image / GDAL raster
    void showImageInfo(const QString& filePath, const osg::Image* image);
    void showRasterInfo(const QString& filePath, const GdalRasterMeta& meta, const osg::Image* image);

    // GDAL vector
    void showVectorInfo(const GdalVectorData& data);
    void showVectorFeatureInfo(const GdalFeature& feat);

private:
    void setupUI();

    // Helpers
    QTreeWidgetItem* addGroup(const QString& title);
    void addKV(QTreeWidgetItem* group, const QString& key, const QString& value);
    static QString formatBytes(qint64 bytes);
    static QString pixelFormatName(unsigned int glFormat);
    void collectNodeStats(const osg::Node* node, unsigned int& vertexCount, unsigned int& faceCount, unsigned int& drawableCount);

    // Replace the appended "selection" group (e.g. selected node / selected feature).
    void clearSelectionGroup();

    QTreeWidget* m_treeWidget = nullptr;

    // Tail group for selection (Selected Node / Selected Feature). Owned by m_treeWidget.
    QTreeWidgetItem* m_selectionGroup = nullptr;
};
