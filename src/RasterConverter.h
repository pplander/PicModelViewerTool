#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPair>

class RasterConverter : public QObject
{
    Q_OBJECT
public:
    explicit RasterConverter(QObject* parent = nullptr);

    // Output formats (label, fileExt, gdalDriverName)
    // Example: ("GeoTIFF", "tif", "GTiff")
    struct FormatInfo {
        QString label;
        QString extension;
        QString driver;
    };
    static QList<FormatInfo> getExportFormats();

    // For "Add Files..." filter
    static QStringList getImportFormatsFilter();

    bool convertFile(const QString& inputPath,
                     const QString& outputFormat,   // extension, e.g. "tif"
                     const QString& outputPath);

    bool convertBatch(const QStringList& inputFiles,
                      const QString& outputFormat,
                      const QString& outputDir);

signals:
    void conversionProgress(const QString& message, int current, int total);
    void conversionFinished(bool success, const QString& message);

private:
    QString generateOutputPath(const QString& inputPath,
                               const QString& outputDir,
                               const QString& outputFormat) const;
    static QString driverFromExt(const QString& ext);
};
