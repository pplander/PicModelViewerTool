#include "WelcomeWidget.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QFont>
#include <QFile>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void WelcomeWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(16);

    // Add stretch at top
    mainLayout->addStretch(3);

    // Center container
    QVBoxLayout* centerLayout = new QVBoxLayout;
    centerLayout->setAlignment(Qt::AlignCenter);
    centerLayout->setSpacing(12);

    // Icon
    m_iconLabel = new QLabel;
    QPixmap iconPix(":/icons/welcome-model.svg");
    if (!iconPix.isNull())
    {
        m_iconLabel->setPixmap(iconPix.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setObjectName("welcomeIcon");
    centerLayout->addWidget(m_iconLabel);

    // Title
    m_titleLabel = new QLabel(tr("Welcome to PicModelViewer"));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("welcomeTitle");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    centerLayout->addWidget(m_titleLabel);

    // Subtitle
    m_subtitleLabel = new QLabel(tr("Universal 3D Model Viewer"));
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setObjectName("welcomeSubtitle");
    QFont subFont = m_subtitleLabel->font();
    subFont.setPointSize(12);
    m_subtitleLabel->setFont(subFont);
    centerLayout->addWidget(m_subtitleLabel);

    // Spacing
    centerLayout->addSpacing(16);

    // Open button
    m_openButton = new QPushButton(tr("Open Model"));
    m_openButton->setObjectName("welcomeOpenButton");
    m_openButton->setFixedSize(220, 44);
    m_openButton->setCursor(Qt::PointingHandCursor);
    connect(m_openButton, &QPushButton::clicked, this, &WelcomeWidget::openClicked);
    centerLayout->addWidget(m_openButton, 0, Qt::AlignCenter);

    // Recent files panel
    m_recentPanel = new QWidget;
    m_recentPanel->setObjectName("recentPanel");
    m_recentPanel->setMaximumWidth(400);
    m_recentLayout = new QVBoxLayout(m_recentPanel);
    m_recentLayout->setContentsMargins(16, 12, 16, 12);
    m_recentLayout->setSpacing(4);

    m_recentTitleLabel = new QLabel(tr("Recent Files"));
    m_recentTitleLabel->setObjectName("recentTitle");
    QFont recentFont = m_recentTitleLabel->font();
    recentFont.setBold(true);
    recentFont.setPointSize(10);
    m_recentTitleLabel->setFont(recentFont);
    m_recentLayout->addWidget(m_recentTitleLabel);

    centerLayout->addWidget(m_recentPanel, 0, Qt::AlignCenter);

    mainLayout->addLayout(centerLayout);

    // Add stretch at bottom
    mainLayout->addStretch(2);

    // Tip at bottom
    m_tipLabel = new QLabel(tr("Tip: Drag any model file into the window to open it"));
    m_tipLabel->setAlignment(Qt::AlignCenter);
    m_tipLabel->setObjectName("welcomeTip");
    QFont tipFont = m_tipLabel->font();
    tipFont.setPointSize(10);
    m_tipLabel->setFont(tipFont);
    mainLayout->addWidget(m_tipLabel);

    // Enable drag and drop visual hint
    setAcceptDrops(true);
}

void WelcomeWidget::setRecentFiles(const QStringList& files)
{
    // Remove old file buttons (keep title label at index 0)
    QLayoutItem* item;
    while (m_recentLayout->count() > 1)
    {
        item = m_recentLayout->takeAt(m_recentLayout->count() - 1);
        if (item->widget())
            delete item->widget();
        delete item;
    }

    if (files.isEmpty())
    {
        m_recentPanel->hide();
        return;
    }

    m_recentPanel->show();

    int count = qMin(files.size(), 5);
    for (int i = 0; i < count; ++i)
    {
        const QString& filePath = files[i];
        QPushButton* btn = new QPushButton(QFileInfo(filePath).fileName());
        btn->setObjectName("recentFileButton");
        btn->setToolTip(filePath);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, filePath]() {
            emit recentFileClicked(filePath);
        });
        m_recentLayout->addWidget(btn);
    }
}

void WelcomeWidget::retranslateUI()
{
    m_titleLabel->setText(tr("Welcome to PicModelViewer"));
    m_subtitleLabel->setText(tr("Universal 3D Model Viewer"));
    m_openButton->setText(tr("Open Model"));
    m_recentTitleLabel->setText(tr("Recent Files"));
    m_tipLabel->setText(tr("Tip: Drag any model file into the window to open it"));
}
