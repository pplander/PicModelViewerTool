#pragma once

#include <QDockWidget>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>

class OSGWidget;

class LightControlDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit LightControlDock(OSGWidget* osgWidget, QWidget* parent = nullptr);

    void syncFromWidget();

private:
    void setupUI();

    OSGWidget* m_osgWidget;

    QCheckBox* m_lightEnabledCheck;
    QCheckBox* m_twoSidedCheck;

    QPushButton* m_ambientColorBtn;
    QPushButton* m_diffuseColorBtn;
    QPushButton* m_specularColorBtn;

    QDoubleSpinBox* m_lightPosX;
    QDoubleSpinBox* m_lightPosY;
    QDoubleSpinBox* m_lightPosZ;

    // Background color
    QPushButton* m_bgColorBtn;

private slots:
    void onLightEnabledChanged(int state);
    void onTwoSidedChanged(int state);
    void onAmbientColorClicked();
    void onDiffuseColorClicked();
    void onSpecularColorClicked();
    void onLightPositionChanged();
    void onBgColorClicked();
};
