#include "BatchConvertDialog.h"
#include "ModelConverter.h"
#include "ImageConverter.h"
#include "VectorConverter.h"
#include "RasterConverter.h"
#include "ModelLoader.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>
#include <QEvent>

// ============================================================
//                   ConvertPanel
// ============================================================

ConvertPanel::ConvertPanel(const QList<QPair<QString, QString>>& formats,
                           const QStringList& openFilters,
                           const QStringList& dirNameFilters,
                           QWidget* parent)
    : QWidget(parent)
    , m_openFilters(openFilters)
    , m_dirNameFilters(dirNameFilters)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Source files group
    QGroupBox* sourceGroup = new QGroupBox(tr("Source Files"));
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceGroup);
    m_fileList = new QListWidget;
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setAlternatingRowColors(true);
    sourceLayout->addWidget(m_fileList);

    QHBoxLayout* btns = new QHBoxLayout;
    m_addFilesBtn = new QPushButton(tr("Add Files..."));
    m_addDirBtn   = new QPushButton(tr("Add Directory..."));
    m_removeBtn   = new QPushButton(tr("Remove"));
    m_clearBtn    = new QPushButton(tr("Clear"));
    btns->addWidget(m_addFilesBtn);
    btns->addWidget(m_addDirBtn);
    btns->addWidget(m_removeBtn);
    btns->addWidget(m_clearBtn);
    btns->addStretch();
    sourceLayout->addLayout(btns);
    layout->addWidget(sourceGroup);

    // Output settings group
    QGroupBox* outputGroup = new QGroupBox(tr("Output Settings"));
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);

    QHBoxLayout* formatRow = new QHBoxLayout;
    m_formatLabelText = new QLabel(tr("Output Format:"));
    formatRow->addWidget(m_formatLabelText);
    m_formatCombo = new QComboBox;
    formatRow->addWidget(m_formatCombo);
    formatRow->addStretch();
    outputLayout->addLayout(formatRow);

    QHBoxLayout* dirRow = new QHBoxLayout;
    m_outputDirLabelText = new QLabel(tr("Output Directory:"));
    dirRow->addWidget(m_outputDirLabelText);
    m_outputDirEdit = new QLineEdit;
    m_browseBtn = new QPushButton(tr("Browse..."));
    dirRow->addWidget(m_outputDirEdit);
    dirRow->addWidget(m_browseBtn);
    outputLayout->addLayout(dirRow);

    layout->addWidget(outputGroup);

    resetFormats(formats);

    connect(m_addFilesBtn, &QPushButton::clicked, this, &ConvertPanel::onAddFiles);
    connect(m_addDirBtn,   &QPushButton::clicked, this, &ConvertPanel::onAddDirectory);
    connect(m_removeBtn,   &QPushButton::clicked, this, &ConvertPanel::onRemoveSelected);
    connect(m_clearBtn,    &QPushButton::clicked, this, &ConvertPanel::onClear);
    connect(m_browseBtn,   &QPushButton::clicked, this, &ConvertPanel::onBrowseOutputDir);
    connect(m_outputDirEdit, &QLineEdit::textChanged, this, [this](const QString&){ emit inputsChanged(); });
    connect(m_formatCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ emit inputsChanged(); });
}

void ConvertPanel::resetFormats(const QList<QPair<QString, QString>>& formats)
{
    m_formatCombo->blockSignals(true);
    m_formatCombo->clear();
    for (const auto& kv : formats)
    {
        // user-data carries the file extension
        m_formatCombo->addItem(kv.first, kv.second);
    }
    if (m_formatCombo->count() > 0) m_formatCombo->setCurrentIndex(0);
    m_formatCombo->blockSignals(false);
    emit inputsChanged();
}

QStringList ConvertPanel::files() const
{
    QStringList list;
    for (int i = 0; i < m_fileList->count(); ++i)
        list << m_fileList->item(i)->text();
    return list;
}

QString ConvertPanel::currentFormatExt() const
{
    return m_formatCombo->currentData().toString();
}

QString ConvertPanel::outputDir() const
{
    return m_outputDirEdit->text();
}

bool ConvertPanel::isReady() const
{
    return m_fileList->count() > 0
        && !currentFormatExt().isEmpty()
        && !outputDir().isEmpty();
}

void ConvertPanel::setBusy(bool busy)
{
    m_addFilesBtn->setEnabled(!busy);
    m_addDirBtn->setEnabled(!busy);
    m_removeBtn->setEnabled(!busy);
    m_clearBtn->setEnabled(!busy);
    m_formatCombo->setEnabled(!busy);
    m_outputDirEdit->setEnabled(!busy);
    m_browseBtn->setEnabled(!busy);
}

void ConvertPanel::retranslateUI()
{
    // Group boxes & buttons are owned by Qt's translation auto-handling for tr()
    // strings we created locally; refresh the label texts explicitly.
    m_addFilesBtn->setText(tr("Add Files..."));
    m_addDirBtn->setText(tr("Add Directory..."));
    m_removeBtn->setText(tr("Remove"));
    m_clearBtn->setText(tr("Clear"));
    m_browseBtn->setText(tr("Browse..."));
    m_formatLabelText->setText(tr("Output Format:"));
    m_outputDirLabelText->setText(tr("Output Directory:"));
}

void ConvertPanel::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
        tr("Select Files"), QString(), m_openFilters.join(";;"));
    for (const QString& f : files)
    {
        if (m_fileList->findItems(f, Qt::MatchExactly).isEmpty())
            m_fileList->addItem(f);
    }
    emit inputsChanged();
}

void ConvertPanel::onAddDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (dir.isEmpty()) return;
    QDirIterator it(dir, m_dirNameFilters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString f = it.next();
        if (m_fileList->findItems(f, Qt::MatchExactly).isEmpty())
            m_fileList->addItem(f);
    }
    emit inputsChanged();
}

void ConvertPanel::onRemoveSelected()
{
    qDeleteAll(m_fileList->selectedItems());
    emit inputsChanged();
}

void ConvertPanel::onClear()
{
    m_fileList->clear();
    emit inputsChanged();
}

void ConvertPanel::onBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"),
        m_outputDirEdit->text());
    if (!dir.isEmpty())
        m_outputDirEdit->setText(dir);
}

// ============================================================
//                BatchConvertDialog (Format Conversion)
// ============================================================

static QList<QPair<QString, QString>> buildModelFormats()
{
    QList<QPair<QString, QString>> out;
    // OSG group first, prefer .osgb default
    for (const QString& fmt : ModelConverter::getOSGExportFormats())
        out << qMakePair(QString("%1 (.%2)").arg(fmt.toUpper()).arg(fmt), fmt);
    for (const QString& fmt : ModelConverter::getAssimpExportFormats())
    {
        if (!ModelConverter::isOSGExportFormat(fmt))
            out << qMakePair(QString("%1 (.%2)").arg(fmt.toUpper()).arg(fmt), fmt);
    }
    return out;
}

static QList<QPair<QString, QString>> buildImageFormats()
{
    QList<QPair<QString, QString>> out;
    for (const QString& fmt : ImageConverter::getExportFormats())
        out << qMakePair(QString("%1 (.%2)").arg(fmt.toUpper()).arg(fmt), fmt);
    return out;
}

static QList<QPair<QString, QString>> buildVectorFormats()
{
    QList<QPair<QString, QString>> out;
    for (const auto& f : VectorConverter::getExportFormats())
        out << qMakePair(QString("%1 (.%2)").arg(f.label).arg(f.extension), f.extension);
    return out;
}

static QList<QPair<QString, QString>> buildRasterFormats()
{
    QList<QPair<QString, QString>> out;
    for (const auto& f : RasterConverter::getExportFormats())
        out << qMakePair(QString("%1 (.%2)").arg(f.label).arg(f.extension), f.extension);
    return out;
}

BatchConvertDialog::BatchConvertDialog(ModelConverter* modelConverter, QWidget* parent)
    : QDialog(parent)
    , m_modelConverter(modelConverter)
{
    // Spin up sibling converters
    m_imageConverter  = new ImageConverter(this);
    m_vectorConverter = new VectorConverter(this);
    m_rasterConverter = new RasterConverter(this);

    setupUI();

    // Wire progress/finish signals from every converter to shared status UI
    auto wire = [this](auto* cv) {
        connect(cv, &std::remove_pointer_t<decltype(cv)>::conversionProgress,
                this, &BatchConvertDialog::onConversionProgress);
        connect(cv, &std::remove_pointer_t<decltype(cv)>::conversionFinished,
                this, &BatchConvertDialog::onConversionFinished);
    };
    if (m_modelConverter)  wire(m_modelConverter);
    wire(m_imageConverter);
    wire(m_vectorConverter);
    wire(m_rasterConverter);
}

void BatchConvertDialog::setupUI()
{
    setWindowTitle(tr("Format Conversion"));
    resize(720, 560);

    QVBoxLayout* main = new QVBoxLayout(this);

    m_tabs = new QTabWidget;

    // Model panel - reuses ModelLoader's import filters
    {
        QStringList modelOpen = ModelLoader::getSupportedFormatsFilter();
        QStringList nameFilters;
        for (const QString& fmt : ModelLoader::getSupportedFormats())
            nameFilters << QString("*.%1").arg(fmt);
        m_modelPanel = new ConvertPanel(buildModelFormats(), modelOpen, nameFilters);
        m_tabs->addTab(m_modelPanel, tr("Model"));
    }
    // Image
    {
        QStringList nameFilters = { "*.png","*.jpg","*.jpeg","*.bmp","*.tif","*.tiff","*.webp","*.gif","*.ico" };
        m_imagePanel = new ConvertPanel(buildImageFormats(),
                                        ImageConverter::getImportFormatsFilter(),
                                        nameFilters);
        m_tabs->addTab(m_imagePanel, tr("Image"));
    }
    // Vector
    {
        QStringList nameFilters = { "*.shp","*.geojson","*.json","*.kml","*.kmz",
                                    "*.gpkg","*.gml","*.csv","*.tab","*.dxf","*.dgn" };
        m_vectorPanel = new ConvertPanel(buildVectorFormats(),
                                         VectorConverter::getImportFormatsFilter(),
                                         nameFilters);
        m_tabs->addTab(m_vectorPanel, tr("Vector"));
    }
    // Raster
    {
        QStringList nameFilters = { "*.tif","*.tiff","*.png","*.jpg","*.jpeg","*.jp2",
                                    "*.img","*.bmp","*.gif","*.hdr","*.vrt","*.dem","*.asc" };
        m_rasterPanel = new ConvertPanel(buildRasterFormats(),
                                         RasterConverter::getImportFormatsFilter(),
                                         nameFilters);
        m_tabs->addTab(m_rasterPanel, tr("Raster"));
    }

    main->addWidget(m_tabs);

    QHBoxLayout* prog = new QHBoxLayout;
    m_progressBar = new QProgressBar;
    m_progressBar->setValue(0);
    m_statusLabel = new QLabel(tr("Ready"));
    prog->addWidget(m_progressBar, 1);
    prog->addWidget(m_statusLabel);
    main->addLayout(prog);

    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_convertBtn = new QPushButton(tr("Start Conversion"));
    m_convertBtn->setEnabled(false);
    m_closeBtn = new QPushButton(tr("Close"));
    btnRow->addWidget(m_convertBtn);
    btnRow->addWidget(m_closeBtn);
    main->addLayout(btnRow);

    connect(m_modelPanel,  &ConvertPanel::inputsChanged, this, &BatchConvertDialog::updateConvertButton);
    connect(m_imagePanel,  &ConvertPanel::inputsChanged, this, &BatchConvertDialog::updateConvertButton);
    connect(m_vectorPanel, &ConvertPanel::inputsChanged, this, &BatchConvertDialog::updateConvertButton);
    connect(m_rasterPanel, &ConvertPanel::inputsChanged, this, &BatchConvertDialog::updateConvertButton);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int){ updateConvertButton(); });

    connect(m_convertBtn, &QPushButton::clicked, this, &BatchConvertDialog::startConversion);
    connect(m_closeBtn,   &QPushButton::clicked, this, &QDialog::close);
}

void BatchConvertDialog::updateConvertButton()
{
    if (m_converting) { m_convertBtn->setEnabled(false); return; }
    ConvertPanel* p = nullptr;
    switch (m_tabs->currentIndex())
    {
    case TabModel:  p = m_modelPanel;  break;
    case TabImage:  p = m_imagePanel;  break;
    case TabVector: p = m_vectorPanel; break;
    case TabRaster: p = m_rasterPanel; break;
    }
    m_convertBtn->setEnabled(p && p->isReady());
}

void BatchConvertDialog::startConversion()
{
    if (m_converting) return;

    const int tabIdx = m_tabs->currentIndex();
    ConvertPanel* panel = nullptr;
    switch (tabIdx)
    {
    case TabModel:  panel = m_modelPanel;  break;
    case TabImage:  panel = m_imagePanel;  break;
    case TabVector: panel = m_vectorPanel; break;
    case TabRaster: panel = m_rasterPanel; break;
    }
    if (!panel || !panel->isReady()) return;

    const QStringList files = panel->files();
    const QString fmt = panel->currentFormatExt();
    const QString outDir = panel->outputDir();

    m_converting = true;
    m_convertBtn->setEnabled(false);
    panel->setBusy(true);
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(files.size());

    switch (tabIdx)
    {
    case TabModel:
        if (m_modelConverter) m_modelConverter->convertBatch(files, fmt, outDir);
        else onConversionFinished(false, tr("Model converter unavailable"));
        break;
    case TabImage:  m_imageConverter->convertBatch(files, fmt, outDir);  break;
    case TabVector: m_vectorConverter->convertBatch(files, fmt, outDir); break;
    case TabRaster: m_rasterConverter->convertBatch(files, fmt, outDir); break;
    }
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

    // Re-enable all panels
    m_modelPanel->setBusy(false);
    m_imagePanel->setBusy(false);
    m_vectorPanel->setBusy(false);
    m_rasterPanel->setBusy(false);
    updateConvertButton();

    if (success)
        QMessageBox::information(this, tr("Conversion Complete"), message);
    else
        QMessageBox::warning(this, tr("Conversion Finished"), message);
}

void BatchConvertDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUI();
    QDialog::changeEvent(event);
}

void BatchConvertDialog::retranslateUI()
{
    setWindowTitle(tr("Format Conversion"));
    if (m_tabs)
    {
        m_tabs->setTabText(TabModel,  tr("Model"));
        m_tabs->setTabText(TabImage,  tr("Image"));
        m_tabs->setTabText(TabVector, tr("Vector"));
        m_tabs->setTabText(TabRaster, tr("Raster"));
    }
    m_convertBtn->setText(tr("Start Conversion"));
    m_closeBtn->setText(tr("Close"));
    if (m_modelPanel)  m_modelPanel->retranslateUI();
    if (m_imagePanel)  m_imagePanel->retranslateUI();
    if (m_vectorPanel) m_vectorPanel->retranslateUI();
    if (m_rasterPanel) m_rasterPanel->retranslateUI();
}
