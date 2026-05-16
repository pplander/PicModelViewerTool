#include "LightControlDock.h"
#include "OSGWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QColorDialog>

LightControlDock::LightControlDock(OSGWidget* osgWidget, QWidget* parent)
    : QDockWidget(tr("Light Control"), parent)
    , m_osgWidget(osgWidget)
{
    setupUI();
    syncFromWidget();
}

void LightControlDock::setupUI()
{
    QWidget* content = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Light enable group
    QGroupBox* lightGroup = new QGroupBox(tr("Light Settings"));
    QVBoxLayout* lightLayout = new QVBoxLayout(lightGroup);

    m_lightEnabledCheck = new QCheckBox(tr("Enable Lighting"));
    m_lightEnabledCheck->setChecked(true);
    connect(m_lightEnabledCheck, &QCheckBox::stateChanged, this, &LightControlDock::onLightEnabledChanged);
    lightLayout->addWidget(m_lightEnabledCheck);

    m_twoSidedCheck = new QCheckBox(tr("Two-sided Lighting"));
    m_twoSidedCheck->setChecked(true);
    connect(m_twoSidedCheck, &QCheckBox::stateChanged, this, &LightControlDock::onTwoSidedChanged);
    lightLayout->addWidget(m_twoSidedCheck);

    // Light colors
    QFormLayout* colorLayout = new QFormLayout();

    m_ambientColorBtn = new QPushButton;
    m_ambientColorBtn->setFixedSize(60, 25);
    connect(m_ambientColorBtn, &QPushButton::clicked, this, &LightControlDock::onAmbientColorClicked);
    colorLayout->addRow(tr("Ambient:"), m_ambientColorBtn);

    m_diffuseColorBtn = new QPushButton;
    m_diffuseColorBtn->setFixedSize(60, 25);
    connect(m_diffuseColorBtn, &QPushButton::clicked, this, &LightControlDock::onDiffuseColorClicked);
    colorLayout->addRow(tr("Diffuse:"), m_diffuseColorBtn);

    m_specularColorBtn = new QPushButton;
    m_specularColorBtn->setFixedSize(60, 25);
    connect(m_specularColorBtn, &QPushButton::clicked, this, &LightControlDock::onSpecularColorClicked);
    colorLayout->addRow(tr("Specular:"), m_specularColorBtn);

    lightLayout->addLayout(colorLayout);

    // Light position
    QGroupBox* posGroup = new QGroupBox(tr("Light Position"));
    QFormLayout* posLayout = new QFormLayout(posGroup);

    m_lightPosX = new QDoubleSpinBox;
    m_lightPosX->setRange(-100, 100);
    m_lightPosX->setSingleStep(0.1);
    m_lightPosX->setValue(1.0);
    connect(m_lightPosX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LightControlDock::onLightPositionChanged);
    posLayout->addRow("X:", m_lightPosX);

    m_lightPosY = new QDoubleSpinBox;
    m_lightPosY->setRange(-100, 100);
    m_lightPosY->setSingleStep(0.1);
    m_lightPosY->setValue(1.0);
    connect(m_lightPosY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LightControlDock::onLightPositionChanged);
    posLayout->addRow("Y:", m_lightPosY);

    m_lightPosZ = new QDoubleSpinBox;
    m_lightPosZ->setRange(-100, 100);
    m_lightPosZ->setSingleStep(0.1);
    m_lightPosZ->setValue(1.0);
    connect(m_lightPosZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &LightControlDock::onLightPositionChanged);
    posLayout->addRow("Z:", m_lightPosZ);

    lightLayout->addWidget(posGroup);
    mainLayout->addWidget(lightGroup);

    // Background color
    QGroupBox* bgGroup = new QGroupBox(tr("Background"));
    QHBoxLayout* bgLayout = new QHBoxLayout(bgGroup);
    m_bgColorBtn = new QPushButton;
    m_bgColorBtn->setFixedSize(60, 25);
    connect(m_bgColorBtn, &QPushButton::clicked, this, &LightControlDock::onBgColorClicked);
    bgLayout->addWidget(new QLabel(tr("Color:")));
    bgLayout->addWidget(m_bgColorBtn);
    bgLayout->addStretch();
    mainLayout->addWidget(bgGroup);

    mainLayout->addStretch();
    setWidget(content);
}

void LightControlDock::syncFromWidget()
{
    if (!m_osgWidget) return;

    m_lightEnabledCheck->setChecked(m_osgWidget->isLightEnabled());
    m_twoSidedCheck->setChecked(m_osgWidget->isTwoSidedLighting());

    osg::Vec4 ambient = m_osgWidget->ambientColor();
    osg::Vec4 diffuse = m_osgWidget->diffuseColor();
    osg::Vec4 specular = m_osgWidget->specularColor();
    osg::Vec4 bg = m_osgWidget->backgroundColor();

    m_ambientColorBtn->setStyleSheet(QString("background-color: rgb(%1,%2,%3);")
        .arg(int(ambient.r() * 255)).arg(int(ambient.g() * 255)).arg(int(ambient.b() * 255)));
    m_diffuseColorBtn->setStyleSheet(QString("background-color: rgb(%1,%2,%3);")
        .arg(int(diffuse.r() * 255)).arg(int(diffuse.g() * 255)).arg(int(diffuse.b() * 255)));
    m_specularColorBtn->setStyleSheet(QString("background-color: rgb(%1,%2,%3);")
        .arg(int(specular.r() * 255)).arg(int(specular.g() * 255)).arg(int(specular.b() * 255)));
    m_bgColorBtn->setStyleSheet(QString("background-color: rgb(%1,%2,%3);")
        .arg(int(bg.r() * 255)).arg(int(bg.g() * 255)).arg(int(bg.b() * 255)));

    osg::Vec4 pos = m_osgWidget->lightPosition();
    m_lightPosX->setValue(pos.x());
    m_lightPosY->setValue(pos.y());
    m_lightPosZ->setValue(pos.z());
}

void LightControlDock::onLightEnabledChanged(int state)
{
    m_osgWidget->setLightEnabled(state == Qt::Checked);
}

void LightControlDock::onTwoSidedChanged(int state)
{
    m_osgWidget->setTwoSidedLighting(state == Qt::Checked);
}

void LightControlDock::onAmbientColorClicked()
{
    osg::Vec4 c = m_osgWidget->ambientColor();
    QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Ambient Color"));
    if (color.isValid())
    {
        m_osgWidget->setAmbientColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
        m_ambientColorBtn->setStyleSheet(QString("background-color: %1;").arg(color.name()));
    }
}

void LightControlDock::onDiffuseColorClicked()
{
    osg::Vec4 c = m_osgWidget->diffuseColor();
    QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Diffuse Color"));
    if (color.isValid())
    {
        m_osgWidget->setDiffuseColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
        m_diffuseColorBtn->setStyleSheet(QString("background-color: %1;").arg(color.name()));
    }
}

void LightControlDock::onSpecularColorClicked()
{
    osg::Vec4 c = m_osgWidget->specularColor();
    QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Specular Color"));
    if (color.isValid())
    {
        m_osgWidget->setSpecularColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
        m_specularColorBtn->setStyleSheet(QString("background-color: %1;").arg(color.name()));
    }
}

void LightControlDock::onLightPositionChanged()
{
    m_osgWidget->setLightPosition(osg::Vec4(
        m_lightPosX->value(),
        m_lightPosY->value(),
        m_lightPosZ->value(),
        0.0f));
}

void LightControlDock::onBgColorClicked()
{
    osg::Vec4 c = m_osgWidget->backgroundColor();
    QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Background Color"));
    if (color.isValid())
    {
        m_osgWidget->setBackgroundColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
        m_bgColorBtn->setStyleSheet(QString("background-color: %1;").arg(color.name()));
    }
}
