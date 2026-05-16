#pragma once

#include <QDockWidget>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QScrollArea>
#include <QUndoStack>

#include <osg/Node>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/StateSet>
#include <osg/Material>
#include <osg/Texture2D>

class OSGWidget;

class NodeEditorDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit NodeEditorDock(OSGWidget* osgWidget, QUndoStack* undoStack, QWidget* parent = nullptr);

    void setNode(osg::Node* node);
    osg::Node* getCurrentNode() const { return m_currentNode.get(); }

    void syncFromNode();

signals:
    void nodeEdited(osg::Node* node);

private:
    void setupUI();
    QWidget* createTransformTab();
    QWidget* createMaterialTab();
    QWidget* createTextureTab();
    QWidget* createSceneTab();

    // Transform tab helpers
    osg::Node* getEffectiveTransformNode() const;
    void updateTransformUI();
    void applyTransform();

    // Material tab helpers
    void updateMaterialUI();
    void applyMaterial();
    void applyPreset(const QString& presetName);
    void onMaterialColorClicked(int component);
    void setButtonColor(QPushButton* btn, const osg::Vec4& color);

    // Texture tab helpers
    void updateTextureUI();
    void applyTextureWrapFilter();
    void browseTexture();
    void removeTexture();

    // Scene tab helpers
    void syncSceneUI();
    void setLightColorButton(QPushButton* btn, const QColor& color);

    // Node operations
    // (handled in MainWindow via lambdas)

    OSGWidget* m_osgWidget;
    QUndoStack* m_undoStack;
    osg::observer_ptr<osg::Node> m_currentNode;

    QTabWidget* m_tabWidget;

    // --- Transform Tab ---
    QScrollArea* m_transformScroll;
    QWidget* m_transformContent;
    QLabel* m_transformUnsupportedLabel;
    QDoubleSpinBox* m_posX, *m_posY, *m_posZ;
    QDoubleSpinBox* m_rotH, *m_rotP, *m_rotR;
    QDoubleSpinBox* m_scaleX, *m_scaleY, *m_scaleZ;
    QCheckBox* m_uniformScale;
    QPushButton* m_resetTransformBtn;
    bool m_updatingTransform = false;

    // --- Material Tab ---
    QScrollArea* m_materialScroll;

    // Presets group
    QComboBox* m_presetCombo;

    // Material Properties group
    QCheckBox* m_materialEnabled;
    QComboBox* m_colorModeCombo;
    QPushButton* m_ambientColorBtn;
    QPushButton* m_diffuseColorBtn;
    QPushButton* m_specularColorBtn;
    QPushButton* m_emissionColorBtn;
    QDoubleSpinBox* m_shininessSpin;
    QSlider* m_alphaSlider;
    QDoubleSpinBox* m_alphaSpin;

    // Transparency group
    QComboBox* m_blendModeCombo;
    QCheckBox* m_depthWriteCheck;

    // Rendering Options group
    QComboBox* m_cullFaceCombo;
    QComboBox* m_shadeModelCombo;
    QComboBox* m_polygonModeCombo;
    QDoubleSpinBox* m_lineWidthSpin;
    QDoubleSpinBox* m_pointSizeSpin;
    QDoubleSpinBox* m_polyOffsetFactor;
    QDoubleSpinBox* m_polyOffsetUnits;

    bool m_updatingMaterial = false;

    // --- Texture Tab ---
    QScrollArea* m_textureScroll;
    QCheckBox* m_textureEnabled;
    QLineEdit* m_texturePathEdit;
    QPushButton* m_textureBrowseBtn;
    QLabel* m_texturePreview;
    QComboBox* m_wrapSCombo;
    QComboBox* m_wrapTCombo;
    QComboBox* m_filterCombo;
    QPushButton* m_removeTextureBtn;
    bool m_updatingTexture = false;

    // --- Scene Tab ---
    QCheckBox* m_lightEnabledCheck;
    QCheckBox* m_sceneTwoSidedCheck;
    QPushButton* m_lightAmbientBtn;
    QPushButton* m_lightDiffuseBtn;
    QPushButton* m_lightSpecularBtn;
    QDoubleSpinBox* m_lightPosX;
    QDoubleSpinBox* m_lightPosY;
    QDoubleSpinBox* m_lightPosZ;
    QPushButton* m_bgColorBtn;
    bool m_updatingScene = false;
};
