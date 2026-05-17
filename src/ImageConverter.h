#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ImageConverter : public QObject
{
    Q_OBJECT
public:
    explicit ImageConverter(QObject* parent = nullptr);

    // Output formats supported by Qt's QImageWriter
    static QStringList getExportFormats();

    // File-open filters (for "Add Files..." dialog covering common input formats)
    static QStringList getImportFormatsFilter();

    // Convert one file
    bool convertFile(const QString& inputPath,
                     const QString& outputFormat,
                     const QString& outputPath);

    // Batch convert
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
};
