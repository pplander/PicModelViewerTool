#include "ModelLoader.h"
#include "AssimpToOSGConverter.h"

#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>

#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osg/ComputeBoundsVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/Texture>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// OSG-supported model extensions (only 3D model formats, exclude image formats)
static const QStringList OSG_FORMATS = {
    "osg", "osgb", "osgt", "osgx", "ive",
    "obj", "3ds", "dae", "stl", "ply",
    "lws", "lwo", "x", "dxf", "ac",
    "sha", "p3d", "txp", "bdb", "rot"
};

// Assimp-supported extensions (supplement OSG)
static const QStringList ASSIMP_FORMATS = {
    "fbx", "gltf", "glb", "amf", "3mf",
    "bvh", "csm", "hmp", "md2", "md3",
    "md5mesh", "md5anim", "md5camera",
    "mdc", "mdl", "ms3d", "nff", "off",
    "smd", "ter", "xgl", "zgl", "m3d",
    "m3da", "a3d", "a3da", "assbin",
    "assxml", "assjson", "pmx", "q3d",
    "q3s", "raw", "sib", "stp", "stla",
    "stlb", "3ds", "ac", "ase", "b3d",
    "blend", "dae", "ifc", "irr", "irrmesh",
    "lwo", "lws", "obj", "ply", "stl",
    "x", "x3d"
};

ModelLoader::ModelLoader(QObject* parent)
    : QObject(parent)
{
}

osg::Node* ModelLoader::loadFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists())
    {
        emit loadFinished(false, tr("File does not exist: %1").arg(filePath));
        return nullptr;
    }

    QString ext = fileInfo.suffix().toLower();

    osg::Node* node = nullptr;
    QString loaderUsed;

    // Try OSG first for all formats it supports
    if (isOSGFormat(ext))
    {
        node = loadWithOSG(filePath);
        if (node)
        {
            loaderUsed = "OSG";
        }
    }

    // If OSG failed, or it's an Assimp format, try Assimp
    if (!node && isAssimpFormat(ext))
    {
        node = loadWithAssimp(filePath);
        if (node)
        {
            loaderUsed = "Assimp";
        }
    }

    if (!node)
    {
        // Last resort: try OSG for any format
        node = loadWithOSG(filePath);
        if (node)
        {
            loaderUsed = "OSG";
        }
    }

    if (node)
    {
        calculateModelInfo(node, filePath, loaderUsed);
        emit loadFinished(true, tr("Model loaded successfully: %1").arg(fileInfo.fileName()));
    }
    else
    {
        emit loadFinished(false, tr("Failed to load model: %1").arg(fileInfo.fileName()));
    }

    return node;
}

osg::Node* ModelLoader::loadWithOSG(const QString& filePath)
{
    std::string stdPath = filePath.toLocal8Bit().toStdString();
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(stdPath);

    // Fallback for Unicode paths: copy to temp ASCII path
    if (!node)
    {
        bool hasUnicode = false;
        for (const QChar& c : filePath)
        {
            if (c.unicode() > 127) { hasUnicode = true; break; }
        }
        if (hasUnicode)
        {
            QTemporaryDir tempDir;
            if (tempDir.isValid())
            {
                QFileInfo fi(filePath);
                QString tempPath = tempDir.path() + "/input_temp." + fi.suffix();
                if (QFile::copy(filePath, tempPath))
                {
                    stdPath = tempPath.toLocal8Bit().toStdString();
                    node = osgDB::readNodeFile(stdPath);
                }
            }
        }
    }

    return node.release();
}

osg::Node* ModelLoader::loadWithAssimp(const QString& filePath)
{
    std::string stdPath = filePath.toUtf8().toStdString();

    Assimp::Importer importer;
    unsigned int flags = aiProcess_Triangulate |
                         aiProcess_JoinIdenticalVertices |
                         aiProcess_GenSmoothNormals |
                         aiProcess_CalcTangentSpace |
                         aiProcess_FlipUVs |
                         aiProcess_OptimizeMeshes |
                         aiProcess_OptimizeGraph;

    const aiScene* scene = importer.ReadFile(stdPath, flags);

    // Fallback for Unicode paths
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        bool hasUnicode = false;
        for (const QChar& c : filePath)
        {
            if (c.unicode() > 127) { hasUnicode = true; break; }
        }
        if (hasUnicode)
        {
            QTemporaryDir tempDir;
            if (tempDir.isValid())
            {
                QFileInfo fi(filePath);
                QString tempPath = tempDir.path() + "/input_temp." + fi.suffix();
                if (QFile::copy(filePath, tempPath))
                {
                    stdPath = tempPath.toUtf8().toStdString();
                    scene = importer.ReadFile(stdPath, flags);
                }
            }
        }
    }

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        return nullptr;
    }

    AssimpToOSGConverter converter;
    osg::Node* node = converter.convert(scene, stdPath);
    return node;
}

void ModelLoader::calculateModelInfo(osg::Node* node, const QString& filePath, const QString& loaderUsed)
{
    QFileInfo fileInfo(filePath);

    m_modelInfo = ModelInfo();
    m_modelInfo.fileName = fileInfo.fileName();
    m_modelInfo.filePath = filePath;
    m_modelInfo.format = fileInfo.suffix().toUpper();
    m_modelInfo.fileSize = fileInfo.size();
    m_modelInfo.loaderUsed = loaderUsed;

    // Count geometry
    countGeometry(node, m_modelInfo.vertexCount, m_modelInfo.faceCount,
        m_modelInfo.meshCount, m_modelInfo.materialCount, m_modelInfo.textureCount);

    // Compute bounding box
    osg::ComputeBoundsVisitor cbv;
    node->accept(cbv);
    osg::BoundingBox bb = cbv.getBoundingBox();
    m_modelInfo.boundingBoxMinX = bb.xMin();
    m_modelInfo.boundingBoxMinY = bb.yMin();
    m_modelInfo.boundingBoxMinZ = bb.zMin();
    m_modelInfo.boundingBoxMaxX = bb.xMax();
    m_modelInfo.boundingBoxMaxY = bb.yMax();
    m_modelInfo.boundingBoxMaxZ = bb.zMax();
    m_modelInfo.boundingBoxSizeX = bb.xMax() - bb.xMin();
    m_modelInfo.boundingBoxSizeY = bb.yMax() - bb.yMin();
    m_modelInfo.boundingBoxSizeZ = bb.zMax() - bb.zMin();
}

void ModelLoader::countGeometry(osg::Node* node, int& vertexCount, int& faceCount,
    int& meshCount, int& materialCount, int& textureCount)
{
    if (!node) return;

    // Use a visitor to count
    class CountVisitor : public osg::NodeVisitor
    {
    public:
        CountVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

        void apply(osg::Geode& geode) override
        {
            for (unsigned int i = 0; i < geode.getNumDrawables(); ++i)
            {
                osg::Geometry* geom = geode.getDrawable(i)->asGeometry();
                if (geom)
                {
                    meshCount++;
                    if (geom->getVertexArray())
                    {
                        vertexCount += geom->getVertexArray()->getNumElements();
                    }

                    for (unsigned int p = 0; p < geom->getNumPrimitiveSets(); ++p)
                    {
                        osg::PrimitiveSet* ps = geom->getPrimitiveSet(p);
                        if (ps->getMode() == GL_TRIANGLES)
                        {
                            faceCount += ps->getNumIndices() / 3;
                        }
                        else if (ps->getMode() == GL_TRIANGLE_STRIP)
                        {
                            faceCount += (ps->getNumIndices() - 2);
                        }
                        else if (ps->getMode() == GL_TRIANGLE_FAN)
                        {
                            faceCount += (ps->getNumIndices() - 2);
                        }
                        else if (ps->getMode() == GL_QUADS)
                        {
                            faceCount += ps->getNumIndices() / 4 * 2;
                        }
                        else if (ps->getMode() == GL_POLYGON)
                        {
                            faceCount++;
                        }
                        else
                        {
                            // Lines, points etc - just count as 1 face
                            faceCount++;
                        }
                    }

                    // Count materials
                    osg::StateSet* ss = geom->getStateSet();
                    if (ss)
                    {
                        if (ss->getAttribute(osg::StateAttribute::MATERIAL))
                            materialCount++;
                        if (ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE))
                            textureCount++;
                    }
                }
            }
            traverse(geode);
        }

        int vertexCount = 0;
        int faceCount = 0;
        int meshCount = 0;
        int materialCount = 0;
        int textureCount = 0;
    };

    CountVisitor cv;
    node->accept(cv);

    vertexCount = cv.vertexCount;
    faceCount = cv.faceCount;
    meshCount = cv.meshCount;
    materialCount = cv.materialCount;
    textureCount = cv.textureCount;
}

QStringList ModelLoader::getSupportedFormats()
{
    QStringList allFormats = OSG_FORMATS;
    for (const QString& fmt : ASSIMP_FORMATS)
    {
        if (!allFormats.contains(fmt))
        {
            allFormats << fmt;
        }
    }
    return allFormats;
}

QStringList ModelLoader::getSupportedFormatsFilter()
{
    QStringList formats = getSupportedFormats();

    // Build "All Supported Formats" filter with proper glob patterns
    QStringList allExts;
    for (const QString& fmt : formats)
    {
        allExts << QString("*.%1").arg(fmt);
    }

    QStringList filters;
    filters << QObject::tr("All Supported Formats (%1)").arg(allExts.join(" "));

    // Group by category
    filters << QObject::tr("OSG Native (*.osg *.osgb *.osgt *.osgx *.ive)");
    filters << QObject::tr("Wavefront OBJ (*.obj)");
    filters << QObject::tr("3D Studio (*.3ds)");
    filters << QObject::tr("COLLADA (*.dae)");
    filters << QObject::tr("Autodesk FBX (*.fbx)");
    filters << QObject::tr("glTF (*.gltf *.glb)");
    filters << QObject::tr("STL (*.stl)");
    filters << QObject::tr("Stanford PLY (*.ply)");
    filters << QObject::tr("All Files (*)");

    return filters;
}

bool ModelLoader::isOSGFormat(const QString& extension)
{
    return OSG_FORMATS.contains(extension.toLower());
}

bool ModelLoader::isAssimpFormat(const QString& extension)
{
    return ASSIMP_FORMATS.contains(extension.toLower());
}
