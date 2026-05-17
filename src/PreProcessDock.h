#pragma once
#include <QDockWidget>
#include <array>
#include "PreProcessManager.h"

class QCheckBox;
class QSlider;
class QLabel;
class QPushButton;
class OSGWidget;
class QVBoxLayout;

class PreProcessDock : public QDockWidget {
    Q_OBJECT
public:
    explicit PreProcessDock(PreProcessManager* mgr, OSGWidget* osgWidget, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    struct EffectRow {
        QCheckBox*   check    = nullptr;
        QSlider*     slider0  = nullptr;
        QLabel*      label0   = nullptr;
        QSlider*     slider1  = nullptr;
        QLabel*      label1   = nullptr;
        QPushButton* colorBtn = nullptr;
    };

    void buildUi();
    void addEffectRow(QVBoxLayout* layout,
                      PreProcessManager::Effect e,
                      const QString& name,
                      const QString& p0Label = {},  float p0def = 0.5f,
                      const QString& p1Label = {},  float p1def = 0.5f,
                      bool hasColor = false);

    PreProcessManager* m_mgr       = nullptr;
    OSGWidget*         m_osgWidget = nullptr;
    std::array<EffectRow, PreProcessManager::EffectCount> m_rows{};
    float m_colorR[PreProcessManager::EffectCount];
    float m_colorG[PreProcessManager::EffectCount];
        float m_colorB[PreProcessManager::EffectCount];
    bool  m_initialized = false;
};
