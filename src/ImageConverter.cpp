#include "ImageConverter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>

// Supported export formats (subset of QImageWriter::supportedImageFormats(),
// listed explicitly to expose a friendly UI)
static const QStringList IMAGE_EXPORT_FORMATS = {
    "png", "jpg", "jpeg", "bmp", "tiff", "tif", "webp", "ppm", "xbm", "xpm", "ico"
};

ImageConverter::ImageConverter(QObject* parent)
    : QObject(parent)
{
}

QStringList ImageConverter::getExportFormats()
{
    // Filter against runtime-supported set so we never offer an unavailable format.
    QStringList runtime;
    for (const QByteArray& f : QImageWriter::supportedImageFormats())
        runtime << QString::fromLatin1(f).toLower();

    QStringList out;
    for (const QString& fmt : IMAGE_EXPORT_FORMATS)
    {
        // QImageWriter usually advertises "jpeg" not "jpg"; treat them as equivalent.
        if (runtime.contains(fmt) ||
            (fmt == "jpg" && runtime.contains("jpeg")) ||
            (fmt == "tif" && runtime.contains("tiff")))
        {
            out << fmt;
        }
    }
    return out;
}

QStringList ImageConverter::getImportFormatsFilter()
{
    QStringList all;
    for (const QByteArray& f : QImageReader::supportedImageFormats())
        all << QString("*.%1").arg(QString::fromLatin1(f).toLower());
    all.removeDuplicates();
    std::sort(all.begin(), all.end());

    QStringList filters;
    filters << QObject::tr("All Supported Images (%1)").arg(all.join(" "));
    filters << QObject::tr("PNG (*.png)");
    filters << QObject::tr("JPEG (*.jpg *.jpeg)");
    filters << QObject::tr("BMP (*.bmp)");
    filters << QObject::tr("TIFF (*.tif *.tiff)");
    filters << QObject::tr("WebP (*.webp)");
    filters << QObject::tr("All Files (*)");
    return filters;
}

QString ImageConverter::generateOutputPath(const QString& inputPath,
                                           const QString& outputDir,
                                           const QString& outputFormat) const
{
    QFileInfo info(inputPath);
    QString baseName = info.completeBaseName();
    return QDir(outputDir).filePath(baseName + "." + outputFormat.toLower());
}

bool ImageConverter::convertFile(const QString& inputPath,
                                 const QString& outputFormat,
                                 const QString& outputPath)
{
    QImageReader reader(inputPath);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull())
    {
        emit conversionFinished(false, tr("Failed to read image: %1 (%2)")
            .arg(QFileInfo(inputPath).fileName()).arg(reader.errorString()));
        return false;
    }

    QFileInfo info(outputPath);
    QDir dir = info.absoluteDir();
    if (!dir.exists()) dir.mkpath(".");

    // QImageWriter uses "jpeg" canonical name even for *.jpg, just feed the suffix.
    QByteArray fmt = outputFormat.toLower().toLatin1();
    if (fmt == "jpg") fmt = "jpeg";
    if (fmt == "tif") fmt = "tiff";

    QImageWriter writer(outputPath, fmt);
    if (!writer.write(img))
    {
        emit conversionFinished(false, tr("Failed to write image: %1 (%2)")
            .arg(QFileInfo(outputPath).fileName()).arg(writer.errorString()));
        return false;
    }
    emit conversionFinished(true, tr("Converted: %1").arg(QFileInfo(outputPath).fileName()));
    return true;
}

bool ImageConverter::convertBatch(const QStringList& inputFiles,
                                  const QString& outputFormat,
                                  const QString& outputDir)
{
    if (inputFiles.isEmpty())
    {
        emit conversionFinished(false, tr("No input files specified"));
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

        QImageReader reader(inputPath);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        bool ok = false;
        if (!img.isNull())
        {
            QFileInfo info(outputPath);
            QDir d = info.absoluteDir();
            if (!d.exists()) d.mkpath(".");

            QByteArray fmt = outputFormat.toLower().toLatin1();
            if (fmt == "jpg") fmt = "jpeg";
            if (fmt == "tif") fmt = "tiff";
            QImageWriter writer(outputPath, fmt);
            ok = writer.write(img);
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
