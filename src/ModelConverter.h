#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <osg/ref_ptr>
#include <osg/Node>

class ModelConverter : public QObject
{
    Q_OBJECT

public:
    explicit ModelConverter(QObject* parent = nullptr);

    // Export format queries
    static QStringList getOSGExportFormats();
    static QStringList getAssimpExportFormats();
    static QStringList getAllExportFormats();
    static QStringList getExportFormatsFilter();

    // Check if a format is supported for export
    static bool isOSGExportFormat(const QString& format);
    static bool isAssimpExportFormat(const QString& format);

    // Single file conversion
    bool convertFile(const QString& inputPath,
                     const QString& outputFormat,
                     const QString& outputPath);

    // Batch conversion
    bool convertBatch(const QStringList& inputFiles,
                      const QString& outputFormat,
                      const QString& outputDir);

signals:
    void conversionProgress(const QString& message, int current, int total);
    void conversionFinished(bool success, const QString& message);

private:
    bool exportWithOSG(osg::Node* node, const QString& outputPath);
    bool exportWithAssimp(const QString& inputPath, const QString& outputPath,
                          const QString& format);

    QString generateOutputPath(const QString& inputPath,
                               const QString& outputDir,
                               const QString& outputFormat) const;
};
