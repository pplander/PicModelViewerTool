#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class VectorConverter : public QObject
{
    Q_OBJECT
public:
    explicit VectorConverter(QObject* parent = nullptr);

    struct FormatInfo {
        QString label;
        QString extension;
        QString driver;   // GDAL/OGR driver short name, e.g. "ESRI Shapefile"
    };
    static QList<FormatInfo> getExportFormats();

    static QStringList getImportFormatsFilter();

    bool convertFile(const QString& inputPath,
                     const QString& outputFormat,
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
