#include "VectorConverter.h"
#include "GdalInit.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cpl_conv.h>
#include <cpl_string.h>

VectorConverter::VectorConverter(QObject* parent)
    : QObject(parent)
{
    ensureGdalRegistered();
}

QList<VectorConverter::FormatInfo> VectorConverter::getExportFormats()
{
    return {
        { tr("ESRI Shapefile"), "shp",     "ESRI Shapefile" },
        { tr("GeoJSON"),        "geojson", "GeoJSON" },
        { tr("KML"),            "kml",     "KML" },
        { tr("GeoPackage"),     "gpkg",    "GPKG" },
        { tr("GML"),            "gml",     "GML" },
        { tr("CSV"),            "csv",     "CSV" },
        { tr("MapInfo TAB"),    "tab",     "MapInfo File" },
        { tr("DXF"),            "dxf",     "DXF" },
    };
}

QString VectorConverter::driverFromExt(const QString& ext)
{
    const QString e = ext.toLower();
    for (const FormatInfo& f : getExportFormats())
        if (f.extension == e) return f.driver;
    return QString();
}

QStringList VectorConverter::getImportFormatsFilter()
{
    QStringList exts = {
        "*.shp", "*.geojson", "*.json", "*.kml", "*.kmz",
        "*.gpkg", "*.gml", "*.csv", "*.tab", "*.mif", "*.dxf", "*.dgn"
    };
    QStringList filters;
    filters << QObject::tr("All Supported Vectors (%1)").arg(exts.join(" "));
    filters << QObject::tr("ESRI Shapefile (*.shp)");
    filters << QObject::tr("GeoJSON (*.geojson *.json)");
    filters << QObject::tr("KML (*.kml *.kmz)");
    filters << QObject::tr("GeoPackage (*.gpkg)");
    filters << QObject::tr("GML (*.gml)");
    filters << QObject::tr("CSV (*.csv)");
    filters << QObject::tr("All Files (*)");
    return filters;
}

QString VectorConverter::generateOutputPath(const QString& inputPath,
                                            const QString& outputDir,
                                            const QString& outputFormat) const
{
    QFileInfo info(inputPath);
    return QDir(outputDir).filePath(info.completeBaseName() + "." + outputFormat.toLower());
}

// Copy all layers from src dataset to dst dataset using OGR CopyLayer.
static bool copyAllLayers(GDALDataset* src, GDALDataset* dst)
{
    int n = src->GetLayerCount();
    bool ok = (n > 0);
    for (int i = 0; i < n; ++i)
    {
        OGRLayer* srcLayer = src->GetLayer(i);
        if (!srcLayer) { ok = false; continue; }
        const char* name = srcLayer->GetName();
        OGRLayer* dstLayer = dst->CopyLayer(srcLayer, name, nullptr);
        if (!dstLayer) ok = false;
    }
    return ok;
}

bool VectorConverter::convertFile(const QString& inputPath,
                                  const QString& outputFormat,
                                  const QString& outputPath)
{
    const QString driverName = driverFromExt(outputFormat);
    if (driverName.isEmpty())
    {
        emit conversionFinished(false, tr("Unsupported vector output format: %1").arg(outputFormat));
        return false;
    }
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName.toUtf8().constData());
    if (!driver)
    {
        emit conversionFinished(false, tr("GDAL driver not available: %1").arg(driverName));
        return false;
    }

    GDALDataset* src = static_cast<GDALDataset*>(
        GDALOpenEx(inputPath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr));
    if (!src)
    {
        emit conversionFinished(false, tr("Failed to open vector: %1").arg(QFileInfo(inputPath).fileName()));
        return false;
    }

    QFileInfo info(outputPath);
    QDir d = info.absoluteDir();
    if (!d.exists()) d.mkpath(".");

    // Remove pre-existing output (some drivers refuse to overwrite)
    if (QFileInfo::exists(outputPath))
        driver->Delete(outputPath.toUtf8().constData());

    GDALDataset* dst = driver->Create(
        outputPath.toUtf8().constData(),
        0, 0, 0, GDT_Unknown, nullptr);
    bool ok = false;
    if (dst)
    {
        ok = copyAllLayers(src, dst);
        GDALClose(dst);
    }
    GDALClose(src);

    if (ok)
        emit conversionFinished(true, tr("Converted: %1").arg(QFileInfo(outputPath).fileName()));
    else
        emit conversionFinished(false, tr("Failed to write vector: %1").arg(QFileInfo(outputPath).fileName()));
    return ok;
}

bool VectorConverter::convertBatch(const QStringList& inputFiles,
                                   const QString& outputFormat,
                                   const QString& outputDir)
{
    if (inputFiles.isEmpty())
    {
        emit conversionFinished(false, tr("No input files specified"));
        return false;
    }

    const QString driverName = driverFromExt(outputFormat);
    if (driverName.isEmpty())
    {
        emit conversionFinished(false, tr("Unsupported vector output format: %1").arg(outputFormat));
        return false;
    }
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(driverName.toUtf8().constData());
    if (!driver)
    {
        emit conversionFinished(false, tr("GDAL driver not available: %1").arg(driverName));
        return false;
    }

    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath("."))
    {
        emit conversionFinished(false, tr("Cannot create output directory: %1").arg(outputDir));
        return false;
    }

    const int total = inputFiles.size();
    int successCount = 0, failCount = 0;
    QStringList failedFiles;

    for (int i = 0; i < total; ++i)
    {
        QCoreApplication::processEvents();
        const QString& inputPath = inputFiles[i];
        const QString outputPath = generateOutputPath(inputPath, outputDir, outputFormat);
        emit conversionProgress(tr("Converting %1/%2: %3")
            .arg(i + 1).arg(total).arg(QFileInfo(inputPath).fileName()), i + 1, total);
        QCoreApplication::processEvents();

        GDALDataset* src = static_cast<GDALDataset*>(
            GDALOpenEx(inputPath.toUtf8().constData(),
                       GDAL_OF_VECTOR | GDAL_OF_READONLY,
                       nullptr, nullptr, nullptr));
        bool ok = false;
        if (src)
        {
            if (QFileInfo::exists(outputPath))
                driver->Delete(outputPath.toUtf8().constData());
            GDALDataset* dst = driver->Create(
                outputPath.toUtf8().constData(),
                0, 0, 0, GDT_Unknown, nullptr);
            if (dst)
            {
                ok = copyAllLayers(src, dst);
                GDALClose(dst);
            }
            GDALClose(src);
        }

        if (ok) ++successCount;
        else { ++failCount; failedFiles << QFileInfo(inputPath).fileName(); }
    }

    QString msg = tr("Batch conversion complete: %1 succeeded, %2 failed out of %3 files")
        .arg(successCount).arg(failCount).arg(total);
    if (!failedFiles.isEmpty())
        msg += "\n" + tr("Failed files:") + "\n" + failedFiles.join("\n");
    emit conversionFinished(failCount == 0, msg);
    return failCount == 0;
}
