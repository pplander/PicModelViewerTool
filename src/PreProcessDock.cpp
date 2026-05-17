#include "PreProcessDock.h"
#include "OSGWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QColorDialog>
#include <QColor>
#include <QComboBox>
#include <QEvent>

using E = PreProcessManager::Effect;

PreProcessDock::PreProcessDock(PreProcessManager* mgr, OSGWidget* osgWidget, QWidget* parent)
    : QDockWidget(tr("Pre-Process Effects"), parent), m_mgr(mgr), m_osgWidget(osgWidget)
{
    // Initialize color arrays to defaults
    for (int i = 0; i < PreProcessManager::EffectCount; ++i) {
        m_colorR[i] = 1.0f; m_colorG[i] = 1.0f; m_colorB[i] = 1.0f;
    }
    // Set sensible default colors for effects that use them
    m_colorR[E::SubsurfaceScatter]=1.0f; m_colorG[E::SubsurfaceScatter]=0.6f; m_colorB[E::SubsurfaceScatter]=0.4f;
    m_colorR[E::RimLight]=1.0f; m_colorG[E::RimLight]=1.0f; m_colorB[E::RimLight]=1.0f;
    m_colorR[E::Metalness]=0.9f; m_colorG[E::Metalness]=0.8f; m_colorB[E::Metalness]=0.7f;
    m_colorR[E::OutlineStroke]=0.0f; m_colorG[E::OutlineStroke]=0.0f; m_colorB[E::OutlineStroke]=0.0f;
    m_colorR[E::WireframeOverlay]=0.0f; m_colorG[E::WireframeOverlay]=1.0f; m_colorB[E::WireframeOverlay]=0.5f;
    m_colorR[E::XRay]=0.0f; m_colorG[E::XRay]=1.0f; m_colorB[E::XRay]=1.0f;
    m_colorR[E::Hologram]=0.0f; m_colorG[E::Hologram]=0.9f; m_colorB[E::Hologram]=0.8f;
    m_colorR[E::EnergyShield]=0.2f; m_colorG[E::EnergyShield]=0.5f; m_colorB[E::EnergyShield]=1.0f;
    m_colorR[E::Dissolve]=1.0f; m_colorG[E::Dissolve]=0.4f; m_colorB[E::Dissolve]=0.0f;
    m_colorR[E::Frozen]=0.55f; m_colorG[E::Frozen]=0.8f; m_colorB[E::Frozen]=1.0f;
    m_colorR[E::Lava]=1.0f; m_colorG[E::Lava]=0.3f; m_colorB[E::Lava]=0.0f;
    m_colorR[E::Camouflage]=0.3f; m_colorG[E::Camouflage]=0.35f; m_colorB[E::Camouflage]=0.15f;
    m_colorR[E::CrystalGlass]=0.7f; m_colorG[E::CrystalGlass]=0.85f; m_colorB[E::CrystalGlass]=1.0f;
    m_colorR[E::DistanceFog]=0.7f; m_colorG[E::DistanceFog]=0.75f; m_colorB[E::DistanceFog]=0.8f;
    m_colorR[E::HeightFog]=0.6f; m_colorG[E::HeightFog]=0.7f; m_colorB[E::HeightFog]=0.9f;
    m_colorR[E::AtmosphericScatter]=0.4f; m_colorG[E::AtmosphericScatter]=0.6f; m_colorB[E::AtmosphericScatter]=1.0f;
    m_colorR[E::GroundFog]=0.9f; m_colorG[E::GroundFog]=0.92f; m_colorB[E::GroundFog]=0.95f;
    m_colorR[E::PulseBreathe]=1.0f; m_colorG[E::PulseBreathe]=0.8f; m_colorB[E::PulseBreathe]=0.4f;
    m_colorR[E::ScanLine]=0.0f; m_colorG[E::ScanLine]=1.0f; m_colorB[E::ScanLine]=0.5f;
    m_colorR[E::ElectricArc]=0.5f; m_colorG[E::ElectricArc]=0.3f; m_colorB[E::ElectricArc]=1.0f;
    m_colorR[E::BurnEffect]=1.0f; m_colorG[E::BurnEffect]=0.4f; m_colorB[E::BurnEffect]=0.0f;

    // Sync default colors to manager
    if (m_mgr) {
        for (int i = 0; i < PreProcessManager::EffectCount; ++i) {
            m_mgr->setEffectColor(static_cast<E>(i), m_colorR[i], m_colorG[i], m_colorB[i]);
        }
    }

    buildUi();
}

void PreProcessDock::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange) {
        setWindowTitle(tr("Pre-Process Effects"));
        // Reset row pointers (will be recreated by buildUi)
        for (auto& r : m_rows) r = {};
        buildUi();
    }
    QDockWidget::changeEvent(event);
}

void PreProcessDock::buildUi()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget;
    auto* mainLayout = new QVBoxLayout(container);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    // ---- 渲染风格 Rendering Style ----
    auto* grpRender = new QGroupBox(tr("Rendering Style"));
    auto* layRender = new QVBoxLayout(grpRender);
    layRender->setSpacing(2); layRender->setContentsMargins(6,4,6,6);
    addEffectRow(layRender, E::PhongEnhanced,      tr("Enhanced Phong"),   tr("Specular"), 0.5f, tr("Roughness"),  0.5f);
    addEffectRow(layRender, E::CelShading,         tr("Cel Shading"),      tr("Steps"),    0.5f, tr("Outline"),    0.5f);
    addEffectRow(layRender, E::HalfLambert,        tr("Half Lambert"),     tr("Intensity"),0.5f);
    addEffectRow(layRender, E::AnisotropicSpecular, tr("Anisotropic"),     tr("Direction"),0.5f, tr("Strength"),   0.5f);
    addEffectRow(layRender, E::SubsurfaceScatter,  tr("Subsurface Scatter"),tr("Thickness"),0.5f, tr("Color Int."),0.5f, true);
    addEffectRow(layRender, E::RimLight,           tr("Rim Light"),        tr("Intensity"),0.5f, tr("Power"),      0.5f, true);
    addEffectRow(layRender, E::MatCap,             tr("MatCap"),           tr("Blend"),    0.5f);
    addEffectRow(layRender, E::Metalness,          tr("Metalness PBR"),    tr("Metallic"), 0.5f, tr("Roughness"),  0.5f, true);
    addEffectRow(layRender, E::OutlineStroke,      tr("Outline Stroke"),   tr("Width"),    0.5f, {}, 0.5f, true);
    addEffectRow(layRender, E::WireframeOverlay,   tr("Wireframe Overlay"),tr("Line Width"),0.5f, tr("Fill Alpha"),0.5f, true);
    mainLayout->addWidget(grpRender);

    // ---- 材质特效 Material Effects ----
    auto* grpMat = new QGroupBox(tr("Material Effects"));
    auto* layMat = new QVBoxLayout(grpMat);
    layMat->setSpacing(2); layMat->setContentsMargins(6,4,6,6);
    addEffectRow(layMat, E::XRay,         tr("X-Ray"),        tr("Intensity"),   0.5f, {}, 0.5f, true);
    addEffectRow(layMat, E::Hologram,     tr("Hologram"),     tr("Scan Speed"),  0.5f, tr("Density"),  0.5f, true);
    addEffectRow(layMat, E::EnergyShield, tr("Energy Shield"), tr("Wave Speed"), 0.5f, tr("Strength"),  0.5f, true);
    addEffectRow(layMat, E::Dissolve,     tr("Dissolve"),     tr("Progress"),    0.0f, tr("Edge Width"),0.3f, true);
    addEffectRow(layMat, E::Frozen,       tr("Frozen"),       tr("Strength"),    0.5f, {}, 0.5f, true);
    addEffectRow(layMat, E::Lava,         tr("Lava"),         tr("Flow Speed"),  0.5f, tr("Brightness"),0.5f, true);
    addEffectRow(layMat, E::Camouflage,   tr("Camouflage"),   tr("Scale"),       0.5f, tr("Blend"),     0.5f, true);
    addEffectRow(layMat, E::CrystalGlass, tr("Crystal Glass"),tr("Transparency"),0.5f, tr("Refraction"),0.5f, true);
    mainLayout->addWidget(grpMat);

    // ---- 环境氛围 Environment ----
    auto* grpEnv = new QGroupBox(tr("Environment"));
    auto* layEnv = new QVBoxLayout(grpEnv);
    layEnv->setSpacing(2); layEnv->setContentsMargins(6,4,6,6);
    addEffectRow(layEnv, E::DistanceFog,       tr("Distance Fog"),      tr("Start"),       0.3f, tr("End"),       0.7f, true);
    addEffectRow(layEnv, E::HeightFog,         tr("Height Fog"),        tr("Height"),      0.5f, tr("Density"),   0.3f, true);
    addEffectRow(layEnv, E::AtmosphericScatter,tr("Atmospheric Scatter"),tr("Intensity"),  0.5f, {}, 0.5f, true);
    addEffectRow(layEnv, E::GroundFog,         tr("Ground Fog"),        tr("Height"),      0.3f, tr("Density"),   0.3f, true);
    mainLayout->addWidget(grpEnv);

    // ---- 动画变形 Animation ----
    auto* grpAnim = new QGroupBox(tr("Animation"));
    auto* layAnim = new QVBoxLayout(grpAnim);
    layAnim->setSpacing(2); layAnim->setContentsMargins(6,4,6,6);
    addEffectRow(layAnim, E::VertexWave,   tr("Vertex Wave"),  tr("Frequency"), 0.3f, tr("Amplitude"),0.3f);
    addEffectRow(layAnim, E::FlagWave,     tr("Flag Wave"),    tr("Wind Speed"),0.4f, tr("Direction"),0.5f);
    addEffectRow(layAnim, E::WaterRipple,  tr("Water Ripple"), tr("Speed"),     0.4f, tr("Height"),   0.3f);
    addEffectRow(layAnim, E::PulseBreathe, tr("Pulse Breathe"),tr("Frequency"), 0.4f, {}, 0.5f, true);
    addEffectRow(layAnim, E::ScanLine,     tr("Scan Line"),    tr("Speed"),     0.4f, tr("Density"),  0.5f, true);
    addEffectRow(layAnim, E::ElectricArc,  tr("Electric Arc"), tr("Speed"),     0.5f, tr("Strength"), 0.5f, true);
    addEffectRow(layAnim, E::BurnEffect,   tr("Burn Effect"),  tr("Progress"),  0.0f, tr("Flame Height"),0.3f, true);
    addEffectRow(layAnim, E::GlitchEffect, tr("Glitch"),       tr("Intensity"), 0.3f, tr("Frequency"),0.5f);

    // ---- Explode (特殊行: 下拉选择中心炸开 / 随机方向) ----
    {
        auto* box = new QGroupBox(tr("Explode"));
        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setSpacing(3);
        boxLayout->setContentsMargins(8, 4, 8, 6);

        const int idx = static_cast<int>(E::Explode);
        auto& row = m_rows[idx];

        // First-build: push defaults to manager (Progress=0, Mode=Center)
        if (!m_initialized && m_mgr) {
            m_mgr->setEffectParam0(E::Explode, 0.0f);
            m_mgr->setEffectParam1(E::Explode, 0.0f);
        }

        // Row1: Enable + Mode combo
        auto* hbox1 = new QHBoxLayout;
        row.check = new QCheckBox(tr("Enable"));
        row.check->setChecked(m_mgr->isEffectEnabled(E::Explode));
        hbox1->addWidget(row.check);
        hbox1->addStretch();
        auto* modeLbl = new QLabel(tr("Mode") + ":");
        auto* modeCombo = new QComboBox;
        modeCombo->addItem(tr("From Center"));
        modeCombo->addItem(tr("Random"));
        int curMode = (m_mgr && m_mgr->effectParam1(E::Explode) >= 0.5f) ? 1 : 0;
        modeCombo->setCurrentIndex(curMode);
        hbox1->addWidget(modeLbl);
        hbox1->addWidget(modeCombo);
        boxLayout->addLayout(hbox1);

        // Row2: Progress slider
        const float v0 = m_mgr ? m_mgr->effectParam0(E::Explode) : 0.0f;
        auto* hbox2 = new QHBoxLayout;
        auto* lbl = new QLabel(tr("Progress") + ":");
        lbl->setFixedWidth(80);
        row.slider0 = new QSlider(Qt::Horizontal);
        row.slider0->setRange(0, 100);
        row.slider0->setValue(static_cast<int>(v0 * 100));
        row.slider0->setEnabled(row.check->isChecked());
        row.label0 = new QLabel(QString::number(v0, 'f', 2));
        row.label0->setFixedWidth(36);
        hbox2->addWidget(lbl);
        hbox2->addWidget(row.slider0);
        hbox2->addWidget(row.label0);
        boxLayout->addLayout(hbox2);

        layAnim->addWidget(box);

        // Connections (mirror addEffectRow but no slider1 / colorBtn)
        connect(row.check, &QCheckBox::toggled, this, [this, idx](bool on) {
            if (on) {
                for (int i = 0; i < PreProcessManager::EffectCount; ++i) {
                    if (i == idx) continue;
                    auto& other = m_rows[i];
                    if (other.check && other.check->isChecked()) {
                        QSignalBlocker blocker(other.check);
                        other.check->setChecked(false);
                    }
                    m_mgr->setEffectEnabled(static_cast<PreProcessManager::Effect>(i), false);
                    if (other.slider0) other.slider0->setEnabled(false);
                    if (other.slider1) other.slider1->setEnabled(false);
                }
            }
            m_mgr->setEffectEnabled(E::Explode, on);
            auto& r = m_rows[idx];
            if (r.slider0) r.slider0->setEnabled(on);
            if (m_osgWidget) {
                m_osgWidget->ensurePreProcessInitialized();
                m_osgWidget->update();
            }
        });
        connect(row.slider0, &QSlider::valueChanged, this, [this, idx](int v) {
            float f = v / 100.0f;
            m_mgr->setEffectParam0(E::Explode, f);
            if (m_rows[idx].label0) m_rows[idx].label0->setText(QString::number(f, 'f', 2));
            if (m_osgWidget) m_osgWidget->update();
        });
        connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
            m_mgr->setEffectParam1(E::Explode, i == 0 ? 0.0f : 1.0f);
            if (m_osgWidget) m_osgWidget->update();
        });
    }
    mainLayout->addWidget(grpAnim);

    // ---- 可视化诊断 Visualization ----
    auto* grpVis = new QGroupBox(tr("Visualization"));
    auto* layVis = new QVBoxLayout(grpVis);
    layVis->setSpacing(2); layVis->setContentsMargins(6,4,6,6);
    addEffectRow(layVis, E::NormalVis,       tr("Normal Vis"),    {}, 0.5f);
    addEffectRow(layVis, E::DepthVis,        tr("Depth Vis"),     tr("Near"), 0.3f, tr("Far"),  0.8f);
    addEffectRow(layVis, E::VertexColorVis,  tr("Vertex Color"),  {}, 0.5f);
    addEffectRow(layVis, E::UVVis,           tr("UV Vis"),        {}, 0.5f);
    addEffectRow(layVis, E::FaceOrientation, tr("Face Orientation"), {}, 0.5f);
    addEffectRow(layVis, E::HeightColor,     tr("Height Color"),  tr("Low"),  0.3f, tr("High"), 0.7f);
    mainLayout->addWidget(grpVis);

    mainLayout->addStretch();
    scroll->setWidget(container);
    setWidget(scroll); // takes ownership and deletes the previous widget
    m_initialized = true;
}

void PreProcessDock::addEffectRow(QVBoxLayout* layout,
                                   PreProcessManager::Effect e,
                                   const QString& name,
                                   const QString& p0Label, float p0def,
                                   const QString& p1Label, float p1def,
                                   bool hasColor)
{
    const int idx = static_cast<int>(e);

    // On first build, push slider defaults into the manager so rebuild can read them back.
    if (!m_initialized && m_mgr) {
        m_mgr->setEffectParam0(e, p0def);
        m_mgr->setEffectParam1(e, p1def);
    }
    const float v0 = (m_initialized && m_mgr) ? m_mgr->effectParam0(e) : p0def;
    const float v1 = (m_initialized && m_mgr) ? m_mgr->effectParam1(e) : p1def;

    auto* box = new QGroupBox(name);
    auto* boxLayout = new QVBoxLayout(box);
    boxLayout->setSpacing(3);
    boxLayout->setContentsMargins(8, 4, 8, 6);

    auto& row = m_rows[idx];

    // Enable checkbox row
    {
        auto* hbox = new QHBoxLayout;
        row.check = new QCheckBox(tr("Enable"));
        row.check->setChecked(m_mgr->isEffectEnabled(e));
        hbox->addWidget(row.check);
        hbox->addStretch();

        if (hasColor) {
            row.colorBtn = new QPushButton;
            row.colorBtn->setFixedSize(42, 14);
            row.colorBtn->setFlat(true);
            row.colorBtn->setAutoFillBackground(false);
            QColor c(static_cast<int>(m_colorR[idx]*255), static_cast<int>(m_colorG[idx]*255), static_cast<int>(m_colorB[idx]*255));
            row.colorBtn->setStyleSheet(
                QString("QPushButton{background-color:%1; border:1px solid #555; border-radius:2px; padding:0px; margin:0px; min-width:0px; min-height:0px;} "
                        "QPushButton:hover{border:1px solid #888;} "
                        "QPushButton:pressed{border:1px solid #aaa;}").arg(c.name()));
            hbox->addWidget(row.colorBtn);
        }
        boxLayout->addLayout(hbox);
    }

    // Param 0
    if (!p0Label.isEmpty()) {
        auto* hbox = new QHBoxLayout;
        auto* lbl = new QLabel(p0Label + ":");
        lbl->setFixedWidth(80);
        row.slider0 = new QSlider(Qt::Horizontal);
        row.slider0->setRange(0, 100);
        row.slider0->setValue(static_cast<int>(v0 * 100));
        row.slider0->setEnabled(row.check->isChecked());
        row.label0 = new QLabel(QString::number(v0, 'f', 2));
        row.label0->setFixedWidth(36);
        hbox->addWidget(lbl);
        hbox->addWidget(row.slider0);
        hbox->addWidget(row.label0);
        boxLayout->addLayout(hbox);
    }

    // Param 1
    if (!p1Label.isEmpty()) {
        auto* hbox = new QHBoxLayout;
        auto* lbl = new QLabel(p1Label + ":");
        lbl->setFixedWidth(80);
        row.slider1 = new QSlider(Qt::Horizontal);
        row.slider1->setRange(0, 100);
        row.slider1->setValue(static_cast<int>(v1 * 100));
        row.slider1->setEnabled(row.check->isChecked());
        row.label1 = new QLabel(QString::number(v1, 'f', 2));
        row.label1->setFixedWidth(36);
        hbox->addWidget(lbl);
        hbox->addWidget(row.slider1);
        hbox->addWidget(row.label1);
        boxLayout->addLayout(hbox);
    }

    layout->addWidget(box);

    // --- Connections ---
    connect(row.check, &QCheckBox::toggled, this, [this, e, idx](bool on) {
        // Single-select preview mode: enable current -> disable all others
        if (on) {
            for (int i = 0; i < PreProcessManager::EffectCount; ++i) {
                if (i == idx) continue;
                auto& other = m_rows[i];
                if (other.check && other.check->isChecked()) {
                    QSignalBlocker blocker(other.check);
                    other.check->setChecked(false);
                }
                m_mgr->setEffectEnabled(static_cast<PreProcessManager::Effect>(i), false);
                if (other.slider0) other.slider0->setEnabled(false);
                if (other.slider1) other.slider1->setEnabled(false);
            }
        }

        m_mgr->setEffectEnabled(e, on);
        auto& r = m_rows[idx];
        if (r.slider0) r.slider0->setEnabled(on);
        if (r.slider1) r.slider1->setEnabled(on);
        if (m_osgWidget) {
            m_osgWidget->ensurePreProcessInitialized();
            m_osgWidget->update();
        }
    });

    if (row.slider0) {
        connect(row.slider0, &QSlider::valueChanged, this, [this, e, idx](int v) {
            float f = v / 100.0f;
            m_mgr->setEffectParam0(e, f);
            if (m_rows[idx].label0) m_rows[idx].label0->setText(QString::number(f, 'f', 2));
            if (m_osgWidget) m_osgWidget->update();
        });
    }

    if (row.slider1) {
        connect(row.slider1, &QSlider::valueChanged, this, [this, e, idx](int v) {
            float f = v / 100.0f;
            m_mgr->setEffectParam1(e, f);
            if (m_rows[idx].label1) m_rows[idx].label1->setText(QString::number(f, 'f', 2));
            if (m_osgWidget) m_osgWidget->update();
        });
    }

    if (row.colorBtn) {
        connect(row.colorBtn, &QPushButton::clicked, this, [this, e, idx]() {
            QColor init(static_cast<int>(m_colorR[idx]*255),
                        static_cast<int>(m_colorG[idx]*255),
                        static_cast<int>(m_colorB[idx]*255));
            QColor c = QColorDialog::getColor(init, this, tr("Select Color"));
            if (!c.isValid()) return;
            m_colorR[idx] = c.redF();
            m_colorG[idx] = c.greenF();
            m_colorB[idx] = c.blueF();
            m_rows[idx].colorBtn->setStyleSheet(
                QString("QPushButton{background-color:%1; border:1px solid #555; border-radius:2px; padding:0px; margin:0px; min-width:0px; min-height:0px;} "
                        "QPushButton:hover{border:1px solid #888;} "
                        "QPushButton:pressed{border:1px solid #aaa;}").arg(c.name()));
            m_mgr->setEffectColor(e, m_colorR[idx], m_colorG[idx], m_colorB[idx]);
            if (m_osgWidget) m_osgWidget->update();
        });
    }
}
