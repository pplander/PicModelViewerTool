#pragma once

#include <QDialog>
#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QTabWidget>
#include <QPair>

class ModelConverter;
class ImageConverter;
class VectorConverter;
class RasterConverter;

// One reusable "source files + format + output directory" panel.
class ConvertPanel : public QWidget
{
    Q_OBJECT
public:
    // formats: list of (display-label, file-extension) pairs for output combo
    // openFilters: filters for the "Add Files..." QFileDialog
    // dirNameFilters: extension filters for the "Add Directory..." recursive scan
    ConvertPanel(const QList<QPair<QString, QString>>& formats,
                 const QStringList& openFilters,
                 const QStringList& dirNameFilters,
                 QWidget* parent = nullptr);

    QStringList files() const;
    QString currentFormatExt() const;
    QString outputDir() const;
    bool isReady() const;

    void setBusy(bool busy);
    void retranslateUI();
    void resetFormats(const QList<QPair<QString, QString>>& formats);

signals:
    void inputsChanged();

private slots:
    void onAddFiles();
    void onAddDirectory();
    void onRemoveSelected();
    void onClear();
    void onBrowseOutputDir();

private:
    QListWidget* m_fileList = nullptr;
    QPushButton* m_addFilesBtn = nullptr;
    QPushButton* m_addDirBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    QLabel* m_formatLabelText = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QLabel* m_outputDirLabelText = nullptr;
    QLineEdit* m_outputDirEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;

    QStringList m_openFilters;
    QStringList m_dirNameFilters;
};

// Multi-tab format conversion dialog: Model / Image / Vector / Raster.
class BatchConvertDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BatchConvertDialog(ModelConverter* modelConverter, QWidget* parent = nullptr);

    void retranslateUI();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onConversionProgress(const QString& message, int current, int total);
    void onConversionFinished(bool success, const QString& message);
    void updateConvertButton();
    void startConversion();

private:
    void setupUI();

    enum TabIndex { TabModel = 0, TabImage = 1, TabVector = 2, TabRaster = 3 };

    QTabWidget* m_tabs = nullptr;
    ConvertPanel* m_modelPanel = nullptr;
    ConvertPanel* m_imagePanel = nullptr;
    ConvertPanel* m_vectorPanel = nullptr;
    ConvertPanel* m_rasterPanel = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_convertBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    ModelConverter*  m_modelConverter  = nullptr;
    ImageConverter*  m_imageConverter  = nullptr;
    VectorConverter* m_vectorConverter = nullptr;
    RasterConverter* m_rasterConverter = nullptr;

    bool m_converting = false;
};
