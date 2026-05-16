#pragma once

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>

class ModelConverter;

class BatchConvertDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchConvertDialog(ModelConverter* converter, QWidget* parent = nullptr);

    void retranslateUI();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void addFiles();
    void addDirectory();
    void removeSelected();
    void clearFiles();
    void browseOutputDir();
    void startConversion();
    void onConversionProgress(const QString& message, int current, int total);
    void onConversionFinished(bool success, const QString& message);

private:
    void setupUI();
    void updateFormatCombo();
    void updateConvertButton();

    ModelConverter* m_converter;

    // UI elements
    QListWidget* m_fileList = nullptr;
    QPushButton* m_addFilesBtn = nullptr;
    QPushButton* m_addDirBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_outputDirEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;

    QPushButton* m_convertBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    bool m_converting = false;
};
