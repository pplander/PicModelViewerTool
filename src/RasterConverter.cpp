#include "RasterConverter.h"
#include "GdalInit.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <gdal_priv.h>
#include <cpl_conv.h>
#include <cpl_string.h>

RasterConverter::RasterConverter(QObject* parent)
    : QObject(parent)
{
    ensureGdalRegistered();
}

QList<RasterConverter::FormatInfo> RasterConverter::getExportFormats()
{
    return {
        { tr("GeoTIFF"),     "tif",  "GTiff" },
        { tr("PNG"),         "png",  "PNG" },
        { tr("JPEG"),        "jpg",  "JPEG" },
        { tr("JPEG 2000"),   "jp2",  "JP2OpenJPEG" },
        { tr("Erdas IMG"),   "img",  "HFA" },
        { tr("BMP"),         "bmp",  "BMP" },
        { tr("GIF"),         "gif",  "GIF" },
        { tr("ENVI"),        "hdr",  "ENVI" },
    };
}

QString RasterConverter::driverFromExt(const QString& ext)
{
    const QString e = ext.toLower();
    for (const FormatInfo& f : getExportFormats())
    {
        if (f.extension == e) return f.driver;
    }
    return QString();
}

QStringList RasterConverter::getImportFormatsFilter()
{
    QStringList exts = {
        "*.tif", "*.tiff", "*.png", "*.jpg", "*.jpeg", "*.jp2",
        "*.img", "*.bmp", "*.gif", "*.hdr", "*.vrt", "*.dem", "*.asc"
    };
    QStringList filters;
    filters << QObject::tr("All Supported Rasters (%1)").arg(exts.join(" "));
    filters << QObject::tr("GeoTIFF (*.tif *.tiff)");
    filters << QObject::tr("PNG (*.png)");
    filters << QObject::tr("JPEG (*.jpg *.jpeg)");
    filters << QObject::tr("JPEG 2000 (*.jp2)");
    filters << QObject::tr("Erdas IMG (*.img)");
    filters << QObject::tr("All Files (*)");
    return filters;
}

QString RasterConverter::generateOutputPath(const QString& inputPath,
                                            const QString& outputDir,
                                            const QString& outputFormat) const
{
    QFileInfo info(inputPath);
    return QDir(outputDir).filePath(info.completeBaseName() + "." + outputFormat.toLower());
}

bool RasterConverter::convertFile(const QString& inputPath,
                                  const QString& outputFormat,
                                  const QString& outputPath)
{
    const QString driverName = driverFromExt(outputFormat);
    if (driverName.isEmpty())
    {
        emit conversionFinished(false, tr("Unsupported raster output format: %1").arg(outputFormat));
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
                   GDAL_OF_RASTER | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr));
    if (!src)
    {
        emit conversionFinished(false, tr("Failed to open raster: %1").arg(QFileInfo(inputPath).fileName()));
        return false;
    }

    QFileInfo info(outputPath);
    QDir d = info.absoluteDir();
    if (!d.exists()) d.mkpath(".");

    GDALDataset* dst = driver->CreateCopy(
        outputPath.toUtf8().constData(),
        src,
        FALSE, nullptr, nullptr, nullptr);

    bool ok = (dst != nullptr);
    if (dst) GDALClose(dst);
    GDALClose(src);

    if (ok)
        emit conversionFinished(true, tr("Converted: %1").arg(QFileInfo(outputPath).fileName()));
    else
        emit conversionFinished(false, tr("Failed to write raster: %1").arg(QFileInfo(outputPath).fileName()));
    return ok;
}

bool RasterConverter::convertBatch(const QStringList& inputFiles,
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
        emit conversionFinished(false, tr("Unsupported raster output format: %1").arg(outputFormat));
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
                       GDAL_OF_RASTER | GDAL_OF_READONLY,
                       nullptr, nullptr, nullptr));
        bool ok = false;
        if (src)
        {
            GDALDataset* dst = driver->CreateCopy(
                outputPath.toUtf8().constData(),
                src,
                FALSE, nullptr, nullptr, nullptr);
            ok = (dst != nullptr);
            if (dst) GDALClose(dst);
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
