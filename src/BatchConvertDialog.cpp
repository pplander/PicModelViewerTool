#include "BatchConvertDialog.h"
#include "ModelConverter.h"
#include "ModelLoader.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QEvent>
#include <QDirIterator>

BatchConvertDialog::BatchConvertDialog(ModelConverter* converter, QWidget* parent)
    : QDialog(parent)
    , m_converter(converter)
{
    setupUI();
    updateFormatCombo();
    updateConvertButton();

    connect(m_converter, &ModelConverter::conversionProgress,
            this, &BatchConvertDialog::onConversionProgress);
    connect(m_converter, &ModelConverter::conversionFinished,
            this, &BatchConvertDialog::onConversionFinished);
}

void BatchConvertDialog::setupUI()
{
    setWindowTitle(tr("Batch Model Conversion"));
    resize(640, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // --- Source files group ---
    QGroupBox* sourceGroup = new QGroupBox(tr("Source Files"));
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceGroup);

    m_fileList = new QListWidget;
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setAlternatingRowColors(true);
    sourceLayout->addWidget(m_fileList);

    QHBoxLayout* sourceBtnLayout = new QHBoxLayout;
    m_addFilesBtn = new QPushButton(tr("Add Files..."));
    m_addDirBtn = new QPushButton(tr("Add Directory..."));
    m_removeBtn = new QPushButton(tr("Remove"));
    m_clearBtn = new QPushButton(tr("Clear"));
    sourceBtnLayout->addWidget(m_addFilesBtn);
    sourceBtnLayout->addWidget(m_addDirBtn);
    sourceBtnLayout->addWidget(m_removeBtn);
    sourceBtnLayout->addWidget(m_clearBtn);
    sourceBtnLayout->addStretch();
    sourceLayout->addLayout(sourceBtnLayout);

    mainLayout->addWidget(sourceGroup);

    // --- Output settings group ---
    QGroupBox* outputGroup = new QGroupBox(tr("Output Settings"));
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);

    // Format row
    QHBoxLayout* formatLayout = new QHBoxLayout;
    formatLayout->addWidget(new QLabel(tr("Output Format:")));
    m_formatCombo = new QComboBox;
    formatLayout->addWidget(m_formatCombo);
    formatLayout->addStretch();
    outputLayout->addLayout(formatLayout);

    // Output dir row
    QHBoxLayout* dirLayout = new QHBoxLayout;
    dirLayout->addWidget(new QLabel(tr("Output Directory:")));
    m_outputDirEdit = new QLineEdit;
    m_browseBtn = new QPushButton(tr("Browse..."));
    dirLayout->addWidget(m_outputDirEdit);
    dirLayout->addWidget(m_browseBtn);
    outputLayout->addLayout(dirLayout);

    mainLayout->addWidget(outputGroup);

    // --- Progress ---
    QHBoxLayout* progressLayout = new QHBoxLayout;
    m_progressBar = new QProgressBar;
    m_progressBar->setValue(0);
    m_statusLabel = new QLabel(tr("Ready"));
    progressLayout->addWidget(m_progressBar, 1);
    progressLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(progressLayout);

    // --- Buttons ---
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    m_convertBtn = new QPushButton(tr("Start Conversion"));
    m_convertBtn->setEnabled(false);
    m_closeBtn = new QPushButton(tr("Close"));
    btnLayout->addWidget(m_convertBtn);
    btnLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_addFilesBtn, &QPushButton::clicked, this, &BatchConvertDialog::addFiles);
    connect(m_addDirBtn, &QPushButton::clicked, this, &BatchConvertDialog::addDirectory);
    connect(m_removeBtn, &QPushButton::clicked, this, &BatchConvertDialog::removeSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &BatchConvertDialog::clearFiles);
    connect(m_browseBtn, &QPushButton::clicked, this, &BatchConvertDialog::browseOutputDir);
    connect(m_convertBtn, &QPushButton::clicked, this, &BatchConvertDialog::startConversion);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(m_fileList, &QListWidget::itemChanged, this, &BatchConvertDialog::updateConvertButton);
}

void BatchConvertDialog::updateFormatCombo()
{
    m_formatCombo->clear();

    // OSG formats group
    m_formatCombo->addItem(tr("-- OSG Formats --"), "");
    for (const QString& fmt : ModelConverter::getOSGExportFormats())
    {
        QString label = QString("%1 (.%2)").arg(fmt.toUpper()).arg(fmt);
        m_formatCombo->addItem(label, fmt);
    }

    // Assimp formats group
    m_formatCombo->addItem(tr("-- Assimp Formats --"), "");
    for (const QString& fmt : ModelConverter::getAssimpExportFormats())
    {
        if (!ModelConverter::isOSGExportFormat(fmt))
        {
            QString label = QString("%1 (.%2)").arg(fmt.toUpper()).arg(fmt);
            m_formatCombo->addItem(label, fmt);
        }
    }

    // Default to OSGB
    for (int i = 0; i < m_formatCombo->count(); i++)
    {
        if (m_formatCombo->itemData(i).toString() == "osgb")
        {
            m_formatCombo->setCurrentIndex(i);
            break;
        }
    }
}

void BatchConvertDialog::updateConvertButton()
{
    bool hasFiles = m_fileList->count() > 0;
    bool hasFormat = !m_formatCombo->currentData().toString().isEmpty();
    bool hasOutputDir = !m_outputDirEdit->text().isEmpty();
    m_convertBtn->setEnabled(hasFiles && hasFormat && hasOutputDir && !m_converting);
}

void BatchConvertDialog::addFiles()
{
    QStringList filters = ModelLoader::getSupportedFormatsFilter();
    QStringList files = QFileDialog::getOpenFileNames(this,
        tr("Select Model Files"), QString(), filters.join(";;"));

    for (const QString& file : files)
    {
        if (m_fileList->findItems(file, Qt::MatchExactly).isEmpty())
        {
            m_fileList->addItem(file);
        }
    }
    updateConvertButton();
}

void BatchConvertDialog::addDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select Directory Containing Models"));
    if (dir.isEmpty()) return;

    QStringList allFormats = ModelLoader::getSupportedFormats();
    QStringList nameFilters;
    for (const QString& fmt : allFormats)
        nameFilters << QString("*.%1").arg(fmt);

    QDirIterator it(dir, nameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString file = it.next();
        if (m_fileList->findItems(file, Qt::MatchExactly).isEmpty())
        {
            m_fileList->addItem(file);
        }
    }
    updateConvertButton();
}

void BatchConvertDialog::removeSelected()
{
    QList<QListWidgetItem*> items = m_fileList->selectedItems();
    for (QListWidgetItem* item : items)
    {
        delete m_fileList->takeItem(m_fileList->row(item));
    }
    updateConvertButton();
}

void BatchConvertDialog::clearFiles()
{
    m_fileList->clear();
    updateConvertButton();
}

void BatchConvertDialog::browseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select Output Directory"));
    if (!dir.isEmpty())
    {
        m_outputDirEdit->setText(dir);
        updateConvertButton();
    }
}

void BatchConvertDialog::startConversion()
{
    if (m_converting) return;

    QString format = m_formatCombo->currentData().toString();
    if (format.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"), tr("Please select an output format"));
        return;
    }

    QString outputDir = m_outputDirEdit->text();
    if (outputDir.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"), tr("Please specify an output directory"));
        return;
    }

    QStringList files;
    for (int i = 0; i < m_fileList->count(); i++)
    {
        files << m_fileList->item(i)->text();
    }

    if (files.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"), tr("No files to convert"));
        return;
    }

    // Start conversion
    m_converting = true;
    m_convertBtn->setEnabled(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Converting..."));

    m_converter->convertBatch(files, format, outputDir);
}

void BatchConvertDialog::onConversionProgress(const QString& message, int current, int total)
{
    m_statusLabel->setText(message);
    if (total > 0)
    {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(current);
    }
}

void BatchConvertDialog::onConversionFinished(bool success, const QString& message)
{
    m_converting = false;
    m_statusLabel->setText(message);
    m_progressBar->setValue(m_progressBar->maximum());
    m_convertBtn->setEnabled(true);

    if (success)
    {
        QMessageBox::information(this, tr("Conversion Complete"), message);
    }
    else
    {
        QMessageBox::warning(this, tr("Conversion Finished"), message);
    }
}

void BatchConvertDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUI();
    }
    QDialog::changeEvent(event);
}

void BatchConvertDialog::retranslateUI()
{
    setWindowTitle(tr("Batch Model Conversion"));
    // Groups and buttons will be retranslated via event propagation
}
