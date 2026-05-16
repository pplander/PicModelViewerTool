#include "ModelConverter.h"
#include "ModelLoader.h"

#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QCoreApplication>

#include <osgDB/WriteFile>
#include <osgDB/FileUtils>

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Helper: Convert QString to UTF-8 std::string for cross-library path handling
static std::string qToUtf8(const QString& str)
{
    return str.toUtf8().toStdString();
}

// Helper: Convert QString to local 8-bit encoding (for OSG which uses locale on Windows)
static std::string qToLocal8Bit(const QString& str)
{
    return str.toLocal8Bit().toStdString();
}

// OSG formats that support write/export
static const QStringList OSG_EXPORT_FORMATS = {
    "osg", "osgb", "ive", "obj", "3ds", "stl", "ply", "dxf"
};

// Assimp formats that support export
static const QStringList ASSIMP_EXPORT_FORMATS = {
    "fbx", "gltf", "glb", "obj", "stl", "dae", "3ds", "ply", "stp"
};

ModelConverter::ModelConverter(QObject* parent)
    : QObject(parent)
{
}

QStringList ModelConverter::getOSGExportFormats()
{
    return OSG_EXPORT_FORMATS;
}

QStringList ModelConverter::getAssimpExportFormats()
{
    return ASSIMP_EXPORT_FORMATS;
}

QStringList ModelConverter::getAllExportFormats()
{
    QStringList all = OSG_EXPORT_FORMATS;
    for (const QString& fmt : ASSIMP_EXPORT_FORMATS)
    {
        if (!all.contains(fmt))
            all << fmt;
    }
    return all;
}

QStringList ModelConverter::getExportFormatsFilter()
{
    QStringList all = getAllExportFormats();
    QStringList allExts;
    for (const QString& fmt : all)
    {
        allExts << QString("*.%1").arg(fmt);
    }

    QStringList filters;
    filters << QObject::tr("All Supported Formats (%1)").arg(allExts.join(" "));

    // OSG export formats
    filters << QObject::tr("OSG Native (*.osg *.osgb *.ive)");
    filters << QObject::tr("Wavefront OBJ (*.obj)");
    filters << QObject::tr("3D Studio (*.3ds)");
    filters << QObject::tr("STL (*.stl)");
    filters << QObject::tr("Stanford PLY (*.ply)");
    filters << QObject::tr("AutoCAD DXF (*.dxf)");

    // Assimp export formats
    filters << QObject::tr("Autodesk FBX (*.fbx)");
    filters << QObject::tr("glTF (*.gltf *.glb)");
    filters << QObject::tr("COLLADA (*.dae)");
    filters << QObject::tr("STEP (*.stp)");

    filters << QObject::tr("All Files (*)");
    return filters;
}

bool ModelConverter::isOSGExportFormat(const QString& format)
{
    return OSG_EXPORT_FORMATS.contains(format.toLower());
}

bool ModelConverter::isAssimpExportFormat(const QString& format)
{
    return ASSIMP_EXPORT_FORMATS.contains(format.toLower());
}

bool ModelConverter::convertFile(const QString& inputPath,
                                  const QString& outputFormat,
                                  const QString& outputPath)
{
    QFileInfo inputInfo(inputPath);
    if (!inputInfo.exists())
    {
        emit conversionFinished(false, tr("Input file does not exist: %1").arg(inputPath));
        return false;
    }

    QString fmt = outputFormat.toLower();

    // Try OSG export first (load with OSG -> write with osgDB)
    if (isOSGExportFormat(fmt))
    {
        // Load the model using ModelLoader
        ModelLoader loader;
        osg::Node* node = loader.loadFile(inputPath);
        if (!node)
        {
            emit conversionFinished(false, tr("Failed to load model: %1").arg(inputPath));
            return false;
        }

        if (exportWithOSG(node, outputPath))
        {
            emit conversionFinished(true, tr("Converted: %1").arg(QFileInfo(outputPath).fileName()));
            return true;
        }
        else
        {
            emit conversionFinished(false, tr("Failed to export: %1").arg(outputPath));
            return false;
        }
    }

    // Try Assimp export
    if (isAssimpExportFormat(fmt))
    {
        if (exportWithAssimp(inputPath, outputPath, fmt))
        {
            emit conversionFinished(true, tr("Converted: %1").arg(QFileInfo(outputPath).fileName()));
            return true;
        }
        else
        {
            emit conversionFinished(false, tr("Failed to export: %1").arg(outputPath));
            return false;
        }
    }

    emit conversionFinished(false, tr("Unsupported export format: %1").arg(fmt));
    return false;
}

bool ModelConverter::convertBatch(const QStringList& inputFiles,
                                   const QString& outputFormat,
                                   const QString& outputDir)
{
    if (inputFiles.isEmpty())
    {
        emit conversionFinished(false, tr("No input files specified"));
        return false;
    }

    QDir dir(outputDir);
    if (!dir.exists())
    {
        if (!dir.mkpath("."))
        {
            emit conversionFinished(false, tr("Cannot create output directory: %1").arg(outputDir));
            return false;
        }
    }

    int total = inputFiles.size();
    int successCount = 0;
    int failCount = 0;
    QStringList failedFiles;

    for (int i = 0; i < total; ++i)
    {
        // Process events to keep UI responsive
        QCoreApplication::processEvents();

        QString inputPath = inputFiles[i];
        QString outputPath = generateOutputPath(inputPath, outputDir, outputFormat);

        emit conversionProgress(tr("Converting %1/%2: %3")
            .arg(i + 1).arg(total).arg(QFileInfo(inputPath).fileName()), i + 1, total);

        QCoreApplication::processEvents();

        QFileInfo inputInfo(inputPath);
        QString fmt = outputFormat.toLower();
        bool ok = false;

        if (isOSGExportFormat(fmt))
        {
            ModelLoader loader;
            osg::Node* node = loader.loadFile(inputPath);
            if (node)
            {
                ok = exportWithOSG(node, outputPath);
            }
        }
        else if (isAssimpExportFormat(fmt))
        {
            ok = exportWithAssimp(inputPath, outputPath, fmt);
        }

        if (ok)
            successCount++;
        else
        {
            failCount++;
            failedFiles << QFileInfo(inputPath).fileName();
        }
    }

    QString msg = tr("Batch conversion complete: %1 succeeded, %2 failed out of %3 files")
        .arg(successCount).arg(failCount).arg(total);
    if (!failedFiles.isEmpty())
    {
        msg += "\n" + tr("Failed files:") + "\n" + failedFiles.join("\n");
    }
    emit conversionFinished(failCount == 0, msg);
    return failCount == 0;
}

bool ModelConverter::exportWithOSG(osg::Node* node, const QString& outputPath)
{
    if (!node) return false;

    // Ensure output directory exists
    QFileInfo info(outputPath);
    QDir dir = info.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    // For Chinese path support on Windows:
    // osgDB::writeNodeFile uses std::string which may not handle Unicode paths.
    // Use a short ASCII temp path and then move the file.
    QString finalPath = QDir::toNativeSeparators(info.absoluteFilePath());

    // Try direct write first (works for ASCII paths)
    bool result = osgDB::writeNodeFile(*node, qToLocal8Bit(outputPath));

    if (!result)
    {
        // Fallback: write to a temp file with ASCII name, then move
        QTemporaryDir tempDir;
        if (tempDir.isValid())
        {
            QString tempPath = tempDir.path() + "/output_temp." + info.suffix();
            result = osgDB::writeNodeFile(*node, qToLocal8Bit(tempPath));
            if (result)
            {
                QFile tempFile(tempPath);
                result = tempFile.copy(finalPath);
            }
        }
    }

    return result;
}

bool ModelConverter::exportWithAssimp(const QString& inputPath,
                                       const QString& outputPath,
                                       const QString& format)
{
    // For Chinese path support, copy input to temp ASCII path if needed
    QString effectiveInputPath = inputPath;
    QTemporaryDir tempDir;
    bool hasTempInput = false;

    // Check if path contains non-ASCII characters
    bool inputHasUnicode = false;
    for (const QChar& c : inputPath)
    {
        if (c.unicode() > 127) { inputHasUnicode = true; break; }
    }

    if (inputHasUnicode && tempDir.isValid())
    {
        // Copy input file to temp with ASCII name
        QFileInfo inputInfo(inputPath);
        QString tempInputPath = tempDir.path() + "/input_temp." + inputInfo.suffix();
        if (QFile::copy(inputPath, tempInputPath))
        {
            effectiveInputPath = tempInputPath;
            hasTempInput = true;
        }
    }

    // Load the scene with Assimp
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(qToUtf8(effectiveInputPath),
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

    if (!scene)
    {
        return false;
    }

    // Export to target format
    Assimp::Exporter exporter;

    // Find the export format ID for the given extension
    QString formatExt = format.toLower();
    std::string exportFormatId;

    for (size_t i = 0; i < exporter.GetExportFormatCount(); i++)
    {
        const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
        if (desc && QString(desc->fileExtension).toLower() == formatExt)
        {
            exportFormatId = desc->id;
            break;
        }
    }

    if (exportFormatId.empty())
    {
        return false;
    }

    // Ensure output directory exists
    QFileInfo info(outputPath);
    QDir dir = info.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    // Handle output path with Chinese characters
    bool outputHasUnicode = false;
    for (const QChar& c : outputPath)
    {
        if (c.unicode() > 127) { outputHasUnicode = true; break; }
    }

    QString effectiveOutputPath = outputPath;
    bool usedTempOutput = false;

    if (outputHasUnicode && tempDir.isValid())
    {
        // Export to temp ASCII path first
        effectiveOutputPath = tempDir.path() + "/output_temp." + info.suffix();
        usedTempOutput = true;
    }

    aiReturn ret = exporter.Export(scene, exportFormatId, qToUtf8(effectiveOutputPath));

    if (ret == AI_SUCCESS && usedTempOutput)
    {
        // Move temp output to final destination
        QFile tempFile(effectiveOutputPath);
        if (!tempFile.copy(QDir::toNativeSeparators(info.absoluteFilePath())))
        {
            return false;
        }
    }

    return ret == AI_SUCCESS;
}

QString ModelConverter::generateOutputPath(const QString& inputPath,
                                            const QString& outputDir,
                                            const QString& outputFormat) const
{
    QFileInfo info(inputPath);
    QString baseName = info.completeBaseName();
    return QDir(outputDir).filePath(baseName + "." + outputFormat);
}
