#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <osg/ref_ptr>
#include <osg/Node>

struct ModelInfo
{
    QString fileName;
    QString filePath;
    QString format;
    qint64 fileSize = 0;

    int vertexCount = 0;
    int faceCount = 0;
    int meshCount = 0;
    int materialCount = 0;
    int textureCount = 0;

    double boundingBoxMinX = 0, boundingBoxMinY = 0, boundingBoxMinZ = 0;
    double boundingBoxMaxX = 0, boundingBoxMaxY = 0, boundingBoxMaxZ = 0;
    double boundingBoxSizeX = 0, boundingBoxSizeY = 0, boundingBoxSizeZ = 0;

    bool hasAnimation = false;
    int animationCount = 0;

    QString loaderUsed; // "OSG" or "Assimp"
};

class ModelLoader : public QObject
{
    Q_OBJECT

public:
    explicit ModelLoader(QObject* parent = nullptr);

    osg::Node* loadFile(const QString& filePath);
    ModelInfo getModelInfo() const { return m_modelInfo; }

    static QStringList getSupportedFormats();
    static QStringList getSupportedFormatsFilter();
    static bool isOSGFormat(const QString& extension);
    static bool isAssimpFormat(const QString& extension);

signals:
    void loadFinished(bool success, const QString& message);

private:
    osg::Node* loadWithOSG(const QString& filePath);
    osg::Node* loadWithAssimp(const QString& filePath);

    void calculateModelInfo(osg::Node* node, const QString& filePath, const QString& loaderUsed);

    static void countGeometry(osg::Node* node, int& vertexCount, int& faceCount,
        int& meshCount, int& materialCount, int& textureCount);

    ModelInfo m_modelInfo;
};
