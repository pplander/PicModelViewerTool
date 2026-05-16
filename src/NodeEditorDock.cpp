#include "NodeEditorDock.h"
#include "OSGWidget.h"
#include "EditCommand.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QPixmap>
#include <QImage>
#include <QGridLayout>
#include <QTemporaryDir>
#include <QScrollArea>
#include <QSpacerItem>
#include <QHash>

#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/TexEnv>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osg/PolygonMode>
#include <osg/PolygonOffset>
#include <osg/BlendFunc>
#include <osg/CullFace>
#include <osg/Point>
#include <osg/Depth>
#include <osg/ShadeModel>
#include <osg/LineWidth>
#include <osg/PolygonOffset>

// ============================================================================
// Construction
// ============================================================================

NodeEditorDock::NodeEditorDock(OSGWidget* osgWidget, QUndoStack* undoStack, QWidget* parent)
    : QDockWidget(tr("Node Editor"), parent)
    , m_osgWidget(osgWidget)
    , m_undoStack(undoStack)
{
    setupUI();
}

void NodeEditorDock::setupUI()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createTransformTab(), tr("Transform"));
    m_tabWidget->addTab(createMaterialTab(), tr("Material"));
    m_tabWidget->addTab(createTextureTab(), tr("Texture"));
    m_tabWidget->addTab(createSceneTab(), tr("Scene"));
    setWidget(m_tabWidget);

    setMinimumWidth(280);
}

// ============================================================================
// Transform Tab
// ============================================================================

QWidget* NodeEditorDock::createTransformTab()
{
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    // Unsupported label (hidden by default - only used for root node with no parent)
    m_transformUnsupportedLabel = new QLabel(tr("Cannot edit transform of the scene root node."));
    m_transformUnsupportedLabel->setWordWrap(true);
    m_transformUnsupportedLabel->setAlignment(Qt::AlignCenter);
    m_transformUnsupportedLabel->hide();
    layout->addWidget(m_transformUnsupportedLabel);

    // Scroll area for transform controls
    m_transformScroll = new QScrollArea;
    m_transformScroll->setWidgetResizable(true);
    m_transformScroll->setFrameShape(QFrame::NoFrame);
    m_transformContent = new QWidget;
    QFormLayout* form = new QFormLayout(m_transformContent);
    form->setContentsMargins(4, 4, 4, 4);

    // Position
    auto posLayout = new QHBoxLayout;
    m_posX = new QDoubleSpinBox; m_posX->setRange(-9999, 9999); m_posX->setDecimals(3); m_posX->setSingleStep(0.1);
    m_posY = new QDoubleSpinBox; m_posY->setRange(-9999, 9999); m_posY->setDecimals(3); m_posY->setSingleStep(0.1);
    m_posZ = new QDoubleSpinBox; m_posZ->setRange(-9999, 9999); m_posZ->setDecimals(3); m_posZ->setSingleStep(0.1);
    for (auto* sb : {m_posX, m_posY, m_posZ}) sb->setMinimumWidth(60);
    posLayout->addWidget(m_posX); posLayout->addWidget(m_posY); posLayout->addWidget(m_posZ);
    form->addRow(tr("Position"), posLayout);

    // Rotation (Euler angles in degrees)
    auto rotLayout = new QHBoxLayout;
    m_rotH = new QDoubleSpinBox; m_rotH->setRange(-360, 360); m_rotH->setDecimals(1); m_rotH->setSingleStep(1.0);
    m_rotP = new QDoubleSpinBox; m_rotP->setRange(-360, 360); m_rotP->setDecimals(1); m_rotP->setSingleStep(1.0);
    m_rotR = new QDoubleSpinBox; m_rotR->setRange(-360, 360); m_rotR->setDecimals(1); m_rotR->setSingleStep(1.0);
    for (auto* sb : {m_rotH, m_rotP, m_rotR}) sb->setMinimumWidth(60);
    rotLayout->addWidget(m_rotH); rotLayout->addWidget(m_rotP); rotLayout->addWidget(m_rotR);
    form->addRow(tr("Rotation (H/P/R)"), rotLayout);

    // Scale
    auto scaleLayout = new QHBoxLayout;
    m_scaleX = new QDoubleSpinBox; m_scaleX->setRange(0.01, 999); m_scaleX->setDecimals(3); m_scaleX->setSingleStep(0.1); m_scaleX->setValue(1.0);
    m_scaleY = new QDoubleSpinBox; m_scaleY->setRange(0.01, 999); m_scaleY->setDecimals(3); m_scaleY->setSingleStep(0.1); m_scaleY->setValue(1.0);
    m_scaleZ = new QDoubleSpinBox; m_scaleZ->setRange(0.01, 999); m_scaleZ->setDecimals(3); m_scaleZ->setSingleStep(0.1); m_scaleZ->setValue(1.0);
    for (auto* sb : {m_scaleX, m_scaleY, m_scaleZ}) sb->setMinimumWidth(60);
    scaleLayout->addWidget(m_scaleX); scaleLayout->addWidget(m_scaleY); scaleLayout->addWidget(m_scaleZ);
    form->addRow(tr("Scale"), scaleLayout);

    m_uniformScale = new QCheckBox(tr("Uniform Scale"));
    form->addRow(m_uniformScale);

    m_resetTransformBtn = new QPushButton(tr("Reset Transform"));
    form->addRow(m_resetTransformBtn);

    form->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_transformScroll->setWidget(m_transformContent);
    layout->addWidget(m_transformScroll);

    // Use editingFinished for SpinBoxes so undo only triggers on Enter/focus-out
    auto onSpinFinished = [this]() {
        if (m_updatingTransform || !m_currentNode.valid()) return;
        applyTransform();
    };
    connect(m_posX, &QDoubleSpinBox::editingFinished, this, onSpinFinished);
    connect(m_posY, &QDoubleSpinBox::editingFinished, this, onSpinFinished);
    connect(m_posZ, &QDoubleSpinBox::editingFinished, this, onSpinFinished);
    connect(m_rotH, &QDoubleSpinBox::editingFinished, this, onSpinFinished);
    connect(m_rotP, &QDoubleSpinBox::editingFinished, this, onSpinFinished);
    connect(m_rotR, &QDoubleSpinBox::editingFinished, this, onSpinFinished);
    connect(m_scaleX, &QDoubleSpinBox::editingFinished, this, [this, onSpinFinished]() {
        if (m_uniformScale->isChecked() && !m_updatingTransform) {
            m_updatingTransform = true;
            m_scaleY->setValue(m_scaleX->value());
            m_scaleZ->setValue(m_scaleX->value());
            m_updatingTransform = false;
        }
        onSpinFinished();
    });
    connect(m_scaleY, &QDoubleSpinBox::editingFinished, this, [this, onSpinFinished]() {
        if (m_uniformScale->isChecked() && !m_updatingTransform) {
            m_updatingTransform = true;
            m_scaleX->setValue(m_scaleY->value());
            m_scaleZ->setValue(m_scaleY->value());
            m_updatingTransform = false;
        }
        onSpinFinished();
    });
    connect(m_scaleZ, &QDoubleSpinBox::editingFinished, this, [this, onSpinFinished]() {
        if (m_uniformScale->isChecked() && !m_updatingTransform) {
            m_updatingTransform = true;
            m_scaleX->setValue(m_scaleZ->value());
            m_scaleY->setValue(m_scaleZ->value());
            m_updatingTransform = false;
        }
        onSpinFinished();
    });
    connect(m_resetTransformBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentNode.valid()) return;
        osg::Node* transformNode = getEffectiveTransformNode();
        if (!transformNode) return;
        osg::MatrixTransform* mt = dynamic_cast<osg::MatrixTransform*>(transformNode);
        osg::PositionAttitudeTransform* pat = dynamic_cast<osg::PositionAttitudeTransform*>(transformNode);
        if (mt)
            m_undoStack->push(new TransformCommand(mt, osg::Matrix::identity()));
        else if (pat)
            m_undoStack->push(new PATCommand(pat, osg::Vec3(0,0,0), osg::Quat(), osg::Vec3(1,1,1)));
        updateTransformUI();
    });

    // Initially hidden until a node is selected
    m_transformContent->hide();
    m_transformUnsupportedLabel->hide();

    return page;
}

osg::Node* NodeEditorDock::getEffectiveTransformNode() const
{
    osg::Node* node = m_currentNode.get();
    if (!node) return nullptr;

    // If the node itself is a transform type, edit it directly
    if (dynamic_cast<osg::MatrixTransform*>(node) ||
        dynamic_cast<osg::PositionAttitudeTransform*>(node))
    {
        return node;
    }

    // Check if parent is a transform with only this one real child
    // (exclude highlight overlay nodes from child count)
    if (node->getNumParents() > 0)
    {
        osg::Group* parent = node->getParent(0);
        bool parentIsTransform = (dynamic_cast<osg::MatrixTransform*>(parent) ||
                                  dynamic_cast<osg::PositionAttitudeTransform*>(parent));

        if (parentIsTransform)
        {
            int realChildCount = 0;
            for (unsigned int i = 0; i < parent->getNumChildren(); ++i)
            {
                if (parent->getChild(i)->getNodeMask() != OSGWidget::HIGHLIGHT_NODE_MASK)
                    realChildCount++;
            }

            if (realChildCount == 1)
            {
                return parent;
            }
        }
    }

    // No effective transform node - will need to auto-wrap
    return nullptr;
}

void NodeEditorDock::updateTransformUI()
{
    osg::Node* node = m_currentNode.get();
    if (!node)
    {
        m_transformContent->hide();
        m_transformUnsupportedLabel->hide();
        return;
    }

    // Get the effective transform node (may be the node itself or its parent)
    osg::Node* transformNode = getEffectiveTransformNode();

    if (!transformNode)
    {
        if (node->getNumParents() == 0)
        {
            // Root node without parent cannot be wrapped
            m_transformContent->hide();
            m_transformUnsupportedLabel->show();
            return;
        }
        // Non-transform node without qualifying transform parent: show identity values, will auto-wrap on apply
        m_transformContent->show();
        m_transformUnsupportedLabel->hide();

        m_updatingTransform = true;
        m_posX->setValue(0); m_posY->setValue(0); m_posZ->setValue(0);
        m_rotH->setValue(0); m_rotP->setValue(0); m_rotR->setValue(0);
        m_scaleX->setValue(1); m_scaleY->setValue(1); m_scaleZ->setValue(1);
        m_updatingTransform = false;
        return;
    }

    m_transformContent->show();
    m_transformUnsupportedLabel->hide();

    osg::MatrixTransform* mt = dynamic_cast<osg::MatrixTransform*>(transformNode);
    osg::PositionAttitudeTransform* pat = dynamic_cast<osg::PositionAttitudeTransform*>(transformNode);

    m_updatingTransform = true;

    if (mt)
    {
        osg::Matrix m = mt->getMatrix();
        osg::Vec3 t, s;
        osg::Quat r, so;
        m.decompose(t, r, s, so);

        m_posX->setValue(t.x()); m_posY->setValue(t.y()); m_posZ->setValue(t.z());
        m_scaleX->setValue(s.x()); m_scaleY->setValue(s.y()); m_scaleZ->setValue(s.z());

        double heading, pitch, roll;
        double sinp = 2.0 * (r.w() * r.y() - r.z() * r.x());
        if (fabs(sinp) >= 1.0)
            pitch = copysign(osg::PI / 2.0, sinp);
        else
            pitch = asin(sinp);
        heading = atan2(2.0 * (r.w() * r.z() + r.x() * r.y()), 1.0 - 2.0 * (r.y() * r.y() + r.z() * r.z()));
        roll = atan2(2.0 * (r.w() * r.x() + r.y() * r.z()), 1.0 - 2.0 * (r.x() * r.x() + r.y() * r.y()));

        m_rotH->setValue(osg::RadiansToDegrees(heading));
        m_rotP->setValue(osg::RadiansToDegrees(pitch));
        m_rotR->setValue(osg::RadiansToDegrees(roll));
    }
    else if (pat)
    {
        osg::Vec3 pos = pat->getPosition();
        osg::Vec3 scale = pat->getScale();
        osg::Quat rot = pat->getAttitude();

        m_posX->setValue(pos.x()); m_posY->setValue(pos.y()); m_posZ->setValue(pos.z());
        m_scaleX->setValue(scale.x()); m_scaleY->setValue(scale.y()); m_scaleZ->setValue(scale.z());

        double heading, pitch, roll;
        double sinp = 2.0 * (rot.w() * rot.y() - rot.z() * rot.x());
        if (fabs(sinp) >= 1.0)
            pitch = copysign(osg::PI / 2.0, sinp);
        else
            pitch = asin(sinp);
        heading = atan2(2.0 * (rot.w() * rot.z() + rot.x() * rot.y()), 1.0 - 2.0 * (rot.y() * rot.y() + rot.z() * rot.z()));
        roll = atan2(2.0 * (rot.w() * rot.x() + rot.y() * rot.z()), 1.0 - 2.0 * (rot.x() * rot.x() + rot.y() * rot.y()));

        m_rotH->setValue(osg::RadiansToDegrees(heading));
        m_rotP->setValue(osg::RadiansToDegrees(pitch));
        m_rotR->setValue(osg::RadiansToDegrees(roll));
    }

    m_updatingTransform = false;
}

void NodeEditorDock::applyTransform()
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    osg::Vec3 pos(m_posX->value(), m_posY->value(), m_posZ->value());
    double h = osg::DegreesToRadians(m_rotH->value());
    double p = osg::DegreesToRadians(m_rotP->value());
    double r = osg::DegreesToRadians(m_rotR->value());
    osg::Vec3 scale(m_scaleX->value(), m_scaleY->value(), m_scaleZ->value());

    osg::Quat rot;
    rot.makeRotate(h, osg::Vec3(0, 0, 1), p, osg::Vec3(0, 1, 0), r, osg::Vec3(1, 0, 0));

    osg::Node* transformNode = getEffectiveTransformNode();

    if (transformNode)
    {
        // Apply directly to the effective transform node (may be the node itself or its parent)
        osg::MatrixTransform* mt = dynamic_cast<osg::MatrixTransform*>(transformNode);
        osg::PositionAttitudeTransform* pat = dynamic_cast<osg::PositionAttitudeTransform*>(transformNode);

        if (mt)
        {
            osg::Matrix newMatrix = osg::Matrix::scale(scale) *
                                    osg::Matrix::rotate(rot) *
                                    osg::Matrix::translate(pos);
            m_undoStack->push(new TransformCommand(mt, newMatrix));
        }
        else if (pat)
        {
            m_undoStack->push(new PATCommand(pat, pos, rot, scale));
        }
    }
    else
    {
        // Non-transform node without qualifying transform parent: auto-wrap with a MatrixTransform
        if (node->getNumParents() == 0) return;

        osg::Group* parent = node->getParent(0);
        auto* wrapCmd = new WrapNodeCommand(parent, node);
        osg::MatrixTransform* wrapper = wrapCmd->getWrapper();

        // Push wrap command first (it replaces child with wrapper in the scene graph)
        m_undoStack->push(wrapCmd);

        // Then apply the transform to the wrapper
        osg::Matrix newMatrix = osg::Matrix::scale(scale) *
                                osg::Matrix::rotate(rot) *
                                osg::Matrix::translate(pos);
        m_undoStack->push(new TransformCommand(wrapper, newMatrix));

        // Update current node to the wrapper
        m_currentNode = wrapper;
        emit nodeEdited(wrapper);
    }
}

// ============================================================================
// Material Tab
// ============================================================================

QWidget* NodeEditorDock::createMaterialTab()
{
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    m_materialScroll = new QScrollArea;
    m_materialScroll->setWidgetResizable(true);
    m_materialScroll->setFrameShape(QFrame::NoFrame);
    QWidget* content = new QWidget;
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(4, 4, 4, 4);
    contentLayout->setSpacing(6);

    // ============================================================
    // Group 1: Material Presets
    // ============================================================
    QGroupBox* presetGroup = new QGroupBox(tr("Material Presets"));
    QFormLayout* presetForm = new QFormLayout(presetGroup);
    presetForm->setContentsMargins(4, 4, 4, 4);

    m_presetCombo = new QComboBox;
    m_presetCombo->addItem(tr("None"), QString());
    m_presetCombo->addItem(tr("Gold"), "Gold");
    m_presetCombo->addItem(tr("Silver"), "Silver");
    m_presetCombo->addItem(tr("Copper"), "Copper");
    m_presetCombo->addItem(tr("Bronze"), "Bronze");
    m_presetCombo->addItem(tr("Chrome"), "Chrome");
    m_presetCombo->addItem(tr("Ruby"), "Ruby");
    m_presetCombo->addItem(tr("Emerald"), "Emerald");
    m_presetCombo->addItem(tr("Jade"), "Jade");
    m_presetCombo->addItem(tr("Glass"), "Glass");
    m_presetCombo->addItem(tr("Plastic Red"), "PlasticRed");
    m_presetCombo->addItem(tr("Plastic Green"), "PlasticGreen");
    m_presetCombo->addItem(tr("Plastic Blue"), "PlasticBlue");
    m_presetCombo->addItem(tr("Plastic White"), "PlasticWhite");
    m_presetCombo->addItem(tr("Rubber Black"), "RubberBlack");
    m_presetCombo->addItem(tr("Rubber Red"), "RubberRed");
    m_presetCombo->addItem(tr("Ceramic"), "Ceramic");
    presetForm->addRow(tr("Preset"), m_presetCombo);
    contentLayout->addWidget(presetGroup);

    // ============================================================
    // Group 2: Material Properties
    // ============================================================
    QGroupBox* propGroup = new QGroupBox(tr("Material Properties"));
    QFormLayout* propForm = new QFormLayout(propGroup);
    propForm->setContentsMargins(4, 4, 4, 4);

    m_materialEnabled = new QCheckBox(tr("Enable Material"));
    propForm->addRow(m_materialEnabled);

    m_colorModeCombo = new QComboBox;
    m_colorModeCombo->addItem(tr("Ambient and Diffuse"), static_cast<int>(osg::Material::AMBIENT_AND_DIFFUSE));
    m_colorModeCombo->addItem(tr("Off"), static_cast<int>(osg::Material::OFF));
    m_colorModeCombo->addItem(tr("Emission"), static_cast<int>(osg::Material::EMISSION));
    m_colorModeCombo->addItem(tr("Ambient"), static_cast<int>(osg::Material::AMBIENT));
    m_colorModeCombo->addItem(tr("Diffuse"), static_cast<int>(osg::Material::DIFFUSE));
    m_colorModeCombo->addItem(tr("Specular"), static_cast<int>(osg::Material::SPECULAR));
    propForm->addRow(tr("Color Mode"), m_colorModeCombo);

    m_ambientColorBtn = new QPushButton(tr("Click to Pick"));
    m_ambientColorBtn->setMinimumHeight(26);
    propForm->addRow(tr("Ambient"), m_ambientColorBtn);

    m_diffuseColorBtn = new QPushButton(tr("Click to Pick"));
    m_diffuseColorBtn->setMinimumHeight(26);
    propForm->addRow(tr("Diffuse"), m_diffuseColorBtn);

    m_specularColorBtn = new QPushButton(tr("Click to Pick"));
    m_specularColorBtn->setMinimumHeight(26);
    propForm->addRow(tr("Specular"), m_specularColorBtn);

    m_emissionColorBtn = new QPushButton(tr("Click to Pick"));
    m_emissionColorBtn->setMinimumHeight(26);
    propForm->addRow(tr("Emission"), m_emissionColorBtn);

    m_shininessSpin = new QDoubleSpinBox;
    m_shininessSpin->setRange(0, 128);
    m_shininessSpin->setDecimals(1);
    m_shininessSpin->setSingleStep(1);
    propForm->addRow(tr("Shininess"), m_shininessSpin);

    auto alphaLayout = new QHBoxLayout;
    m_alphaSlider = new QSlider(Qt::Horizontal);
    m_alphaSlider->setRange(0, 100);
    m_alphaSpin = new QDoubleSpinBox;
    m_alphaSpin->setRange(0, 1);
    m_alphaSpin->setDecimals(2);
    m_alphaSpin->setSingleStep(0.01);
    m_alphaSpin->setMinimumWidth(60);
    alphaLayout->addWidget(m_alphaSlider, 1);
    alphaLayout->addWidget(m_alphaSpin);
    propForm->addRow(tr("Alpha"), alphaLayout);

    contentLayout->addWidget(propGroup);

    // ============================================================
    // Group 3: Transparency
    // ============================================================
    QGroupBox* transGroup = new QGroupBox(tr("Transparency"));
    QFormLayout* transForm = new QFormLayout(transGroup);
    transForm->setContentsMargins(4, 4, 4, 4);

    m_blendModeCombo = new QComboBox;
    m_blendModeCombo->addItem(tr("Off"), 0);
    m_blendModeCombo->addItem(tr("Standard Alpha"), 1);
    m_blendModeCombo->addItem(tr("Additive"), 2);
    transForm->addRow(tr("Blend Mode"), m_blendModeCombo);

    m_depthWriteCheck = new QCheckBox(tr("Depth Write"));
    m_depthWriteCheck->setChecked(true);
    transForm->addRow(m_depthWriteCheck);

    contentLayout->addWidget(transGroup);

    // ============================================================
    // Group 4: Rendering Options
    // ============================================================
    QGroupBox* renderGroup = new QGroupBox(tr("Rendering Options"));
    QFormLayout* renderForm = new QFormLayout(renderGroup);
    renderForm->setContentsMargins(4, 4, 4, 4);

    m_cullFaceCombo = new QComboBox;
    m_cullFaceCombo->addItem(tr("Back"), static_cast<int>(osg::CullFace::BACK));
    m_cullFaceCombo->addItem(tr("Off (Two-Sided)"), -1);
    m_cullFaceCombo->addItem(tr("Front"), static_cast<int>(osg::CullFace::FRONT));
    m_cullFaceCombo->addItem(tr("Front and Back"), static_cast<int>(osg::CullFace::FRONT_AND_BACK));
    renderForm->addRow(tr("Cull Face"), m_cullFaceCombo);

    m_shadeModelCombo = new QComboBox;
    m_shadeModelCombo->addItem(tr("Smooth"), static_cast<int>(osg::ShadeModel::SMOOTH));
    m_shadeModelCombo->addItem(tr("Flat"), static_cast<int>(osg::ShadeModel::FLAT));
    renderForm->addRow(tr("Shade Model"), m_shadeModelCombo);

    m_polygonModeCombo = new QComboBox;
    m_polygonModeCombo->addItem(tr("Fill"), static_cast<int>(osg::PolygonMode::FILL));
    m_polygonModeCombo->addItem(tr("Wireframe"), static_cast<int>(osg::PolygonMode::LINE));
    m_polygonModeCombo->addItem(tr("Point"), static_cast<int>(osg::PolygonMode::POINT));
    renderForm->addRow(tr("Polygon Mode"), m_polygonModeCombo);

    m_lineWidthSpin = new QDoubleSpinBox;
    m_lineWidthSpin->setRange(1.0, 10.0);
    m_lineWidthSpin->setDecimals(1);
    m_lineWidthSpin->setSingleStep(0.5);
    m_lineWidthSpin->setValue(1.0);
    renderForm->addRow(tr("Line Width"), m_lineWidthSpin);

    m_pointSizeSpin = new QDoubleSpinBox;
    m_pointSizeSpin->setRange(1.0, 20.0);
    m_pointSizeSpin->setDecimals(1);
    m_pointSizeSpin->setSingleStep(0.5);
    m_pointSizeSpin->setValue(1.0);
    renderForm->addRow(tr("Point Size"), m_pointSizeSpin);

    auto polyOffLayout = new QHBoxLayout;
    m_polyOffsetFactor = new QDoubleSpinBox;
    m_polyOffsetFactor->setRange(-100, 100);
    m_polyOffsetFactor->setDecimals(1);
    m_polyOffsetFactor->setSingleStep(1.0);
    m_polyOffsetFactor->setValue(0.0);
    m_polyOffsetUnits = new QDoubleSpinBox;
    m_polyOffsetUnits->setRange(-100, 100);
    m_polyOffsetUnits->setDecimals(1);
    m_polyOffsetUnits->setSingleStep(1.0);
    m_polyOffsetUnits->setValue(0.0);
    polyOffLayout->addWidget(new QLabel(tr("F:")));
    polyOffLayout->addWidget(m_polyOffsetFactor);
    polyOffLayout->addWidget(new QLabel(tr("U:")));
    polyOffLayout->addWidget(m_polyOffsetUnits);
    renderForm->addRow(tr("Polygon Offset"), polyOffLayout);

    contentLayout->addWidget(renderGroup);

    contentLayout->addStretch();

    m_materialScroll->setWidget(content);
    layout->addWidget(m_materialScroll);

    // ============================================================
    // Connections
    // ============================================================

    // Preset
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (m_updatingMaterial || !m_currentNode.valid()) return;
        QString preset = m_presetCombo->currentData().toString();
        if (!preset.isEmpty())
            applyPreset(preset);
    });

    // Enable material
    connect(m_materialEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_updatingMaterial || !m_currentNode.valid()) return;
        if (!checked)
        {
            osg::StateSet* ss = m_currentNode->getOrCreateStateSet();
            ss->removeAttribute(osg::StateAttribute::MATERIAL);
        }
        else
        {
            applyMaterial();
        }
    });

    // Color mode
    connect(m_colorModeCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Shininess
    connect(m_shininessSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Alpha
    connect(m_alphaSlider, &QSlider::sliderReleased, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid())
        {
            m_alphaSpin->setValue(m_alphaSlider->value() / 100.0);
            applyMaterial();
        }
    });
    connect(m_alphaSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_updatingMaterial) return;
        m_updatingMaterial = true;
        m_alphaSpin->setValue(v / 100.0);
        m_updatingMaterial = false;
    });
    connect(m_alphaSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid())
        {
            m_alphaSlider->setValue(static_cast<int>(m_alphaSpin->value() * 100));
            applyMaterial();
        }
    });

    // Blend mode
    connect(m_blendModeCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Depth write
    connect(m_depthWriteCheck, &QCheckBox::toggled, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Cull face
    connect(m_cullFaceCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Shade model
    connect(m_shadeModelCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Polygon mode
    connect(m_polygonModeCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Line width
    connect(m_lineWidthSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Point size
    connect(m_pointSizeSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Polygon offset
    connect(m_polyOffsetFactor, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });
    connect(m_polyOffsetUnits, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (!m_updatingMaterial && m_currentNode.valid()) applyMaterial();
    });

    // Color buttons
    connect(m_ambientColorBtn, &QPushButton::clicked, this, [this]() { onMaterialColorClicked(0); });
    connect(m_diffuseColorBtn, &QPushButton::clicked, this, [this]() { onMaterialColorClicked(1); });
    connect(m_specularColorBtn, &QPushButton::clicked, this, [this]() { onMaterialColorClicked(2); });
    connect(m_emissionColorBtn, &QPushButton::clicked, this, [this]() { onMaterialColorClicked(3); });

    return page;
}

void NodeEditorDock::updateMaterialUI()
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    m_updatingMaterial = true;

    osg::StateSet* ss = node->getOrCreateStateSet();
    osg::Material* mat = dynamic_cast<osg::Material*>(ss->getAttribute(osg::StateAttribute::MATERIAL));

    m_materialEnabled->setChecked(mat != nullptr);
    m_presetCombo->setCurrentIndex(0); // Reset preset to None

    if (mat)
    {
        setButtonColor(m_ambientColorBtn, mat->getAmbient(osg::Material::FRONT));
        setButtonColor(m_diffuseColorBtn, mat->getDiffuse(osg::Material::FRONT));
        setButtonColor(m_specularColorBtn, mat->getSpecular(osg::Material::FRONT));
        setButtonColor(m_emissionColorBtn, mat->getEmission(osg::Material::FRONT));
        m_shininessSpin->setValue(mat->getShininess(osg::Material::FRONT));

        float alpha = mat->getDiffuse(osg::Material::FRONT).a();
        m_alphaSlider->setValue(static_cast<int>(alpha * 100));
        m_alphaSpin->setValue(alpha);

        // Color mode
        osg::Material::ColorMode colorMode = mat->getColorMode();
        for (int i = 0; i < m_colorModeCombo->count(); ++i)
        {
            if (m_colorModeCombo->itemData(i).toInt() == static_cast<int>(colorMode))
            {
                m_colorModeCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    else
    {
        setButtonColor(m_ambientColorBtn, osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));
        setButtonColor(m_diffuseColorBtn, osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
        setButtonColor(m_specularColorBtn, osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        setButtonColor(m_emissionColorBtn, osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        m_shininessSpin->setValue(0);
        m_alphaSlider->setValue(100);
        m_alphaSpin->setValue(1.0);
        m_colorModeCombo->setCurrentIndex(0); // AMBIENT_AND_DIFFUSE
    }

    // Blend mode
    osg::StateAttribute* blend = ss->getAttribute(osg::StateAttribute::BLENDFUNC);
    if (!blend)
    {
        m_blendModeCombo->setCurrentIndex(0); // Off
    }
    else
    {
        osg::BlendFunc* bf = dynamic_cast<osg::BlendFunc*>(blend);
        if (bf && bf->getDestination() == osg::BlendFunc::ONE)
            m_blendModeCombo->setCurrentIndex(2); // Additive
        else
            m_blendModeCombo->setCurrentIndex(1); // Standard Alpha
    }

    // Depth write
    m_depthWriteCheck->setChecked(true);
    osg::StateAttribute* depth = ss->getAttribute(osg::StateAttribute::DEPTH);
    if (depth)
    {
        osg::Depth* d = dynamic_cast<osg::Depth*>(depth);
        if (d) m_depthWriteCheck->setChecked(d->getWriteMask());
    }

    // Cull face
    m_cullFaceCombo->setCurrentIndex(0); // Back (default)
    osg::StateAttribute* cull = ss->getAttribute(osg::StateAttribute::CULLFACE);
    if (!cull)
    {
        // Cull face is OFF = two-sided
        m_cullFaceCombo->setCurrentIndex(1); // Off (Two-Sided)
    }
    else
    {
        osg::CullFace* cf = dynamic_cast<osg::CullFace*>(cull);
        if (cf)
        {
            osg::CullFace::Mode mode = cf->getMode();
            for (int i = 0; i < m_cullFaceCombo->count(); ++i)
            {
                if (m_cullFaceCombo->itemData(i).toInt() == static_cast<int>(mode))
                {
                    m_cullFaceCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    // Shade model
    m_shadeModelCombo->setCurrentIndex(0); // Smooth (default)
    osg::StateAttribute* shadeModel = ss->getAttribute(osg::StateAttribute::SHADEMODEL);
    if (shadeModel)
    {
        osg::ShadeModel* sm = dynamic_cast<osg::ShadeModel*>(shadeModel);
        if (sm)
        {
            if (sm->getMode() == GL_FLAT)
                m_shadeModelCombo->setCurrentIndex(1);
        }
    }

    // Polygon mode
    m_polygonModeCombo->setCurrentIndex(0); // Fill (default)
    osg::StateAttribute* polyMode = ss->getAttribute(osg::StateAttribute::POLYGONMODE);
    if (polyMode)
    {
        osg::PolygonMode* pm = dynamic_cast<osg::PolygonMode*>(polyMode);
        if (pm)
        {
            osg::PolygonMode::Mode mode = pm->getMode(osg::PolygonMode::FRONT_AND_BACK);
            for (int i = 0; i < m_polygonModeCombo->count(); ++i)
            {
                if (m_polygonModeCombo->itemData(i).toInt() == static_cast<int>(mode))
                {
                    m_polygonModeCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    // Line width
    m_lineWidthSpin->setValue(1.0);
    osg::StateAttribute* lineWidth = ss->getAttribute(osg::StateAttribute::LINEWIDTH);
    if (lineWidth)
    {
        osg::LineWidth* lw = dynamic_cast<osg::LineWidth*>(lineWidth);
        if (lw) m_lineWidthSpin->setValue(lw->getWidth());
    }

    // Point size
    m_pointSizeSpin->setValue(1.0);
    osg::StateAttribute* pointSize = ss->getAttribute(osg::StateAttribute::POINT);
    if (pointSize)
    {
        osg::Point* ps = dynamic_cast<osg::Point*>(pointSize);
        if (ps) m_pointSizeSpin->setValue(ps->getSize());
    }

    // Polygon offset
    m_polyOffsetFactor->setValue(0.0);
    m_polyOffsetUnits->setValue(0.0);
    osg::StateAttribute* polyOffset = ss->getAttribute(osg::StateAttribute::POLYGONOFFSET);
    if (polyOffset)
    {
        osg::PolygonOffset* po = dynamic_cast<osg::PolygonOffset*>(polyOffset);
        if (po)
        {
            m_polyOffsetFactor->setValue(po->getFactor());
            m_polyOffsetUnits->setValue(po->getUnits());
        }
    }

    m_updatingMaterial = false;
}

void NodeEditorDock::onMaterialColorClicked(int component)
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    osg::StateSet* ss = node->getOrCreateStateSet();
    osg::Material* mat = dynamic_cast<osg::Material*>(ss->getAttribute(osg::StateAttribute::MATERIAL));
    if (!mat)
    {
        // Auto-create material if enabled
        if (m_materialEnabled->isChecked())
        {
            mat = new osg::Material;
            ss->setAttributeAndModes(mat, osg::StateAttribute::ON);
        }
        else
        {
            return;
        }
    }

    osg::Vec4 currentColor;
    switch (component)
    {
    case 0: currentColor = mat->getAmbient(osg::Material::FRONT); break;
    case 1: currentColor = mat->getDiffuse(osg::Material::FRONT); break;
    case 2: currentColor = mat->getSpecular(osg::Material::FRONT); break;
    case 3: currentColor = mat->getEmission(osg::Material::FRONT); break;
    default: return;
    }

    QColor initial = QColor::fromRgbF(
        qBound(0.0, static_cast<double>(currentColor.r()), 1.0),
        qBound(0.0, static_cast<double>(currentColor.g()), 1.0),
        qBound(0.0, static_cast<double>(currentColor.b()), 1.0),
        qBound(0.0, static_cast<double>(currentColor.a()), 1.0));

    QColor selected = QColorDialog::getColor(initial, this, tr("Select Color"), QColorDialog::ShowAlphaChannel);
    if (!selected.isValid()) return;

    osg::Vec4 newColor(selected.redF(), selected.greenF(), selected.blueF(), selected.alphaF());
    osg::Vec4 oldColor = currentColor;

    m_undoStack->push(new MaterialColorCommand(node, component, oldColor, newColor));
    updateMaterialUI();
}

void NodeEditorDock::applyMaterial()
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    // Save old StateSet snapshot for undo
    osg::ref_ptr<osg::StateSet> oldSS = node->getStateSet();
    auto* cmd = new StateSetCommand(node, oldSS.get(), tr("Change Material"));

    // Apply material changes directly
    osg::StateSet* ss = node->getOrCreateStateSet();

    if (!m_materialEnabled->isChecked())
    {
        ss->removeAttribute(osg::StateAttribute::MATERIAL);
    }
    else
    {
        osg::Material* mat = dynamic_cast<osg::Material*>(ss->getAttribute(osg::StateAttribute::MATERIAL));
        if (!mat)
        {
            mat = new osg::Material;
            ss->setAttribute(mat);
        }

        // Color mode
        osg::Material::ColorMode colorMode = static_cast<osg::Material::ColorMode>(m_colorModeCombo->currentData().toInt());
        mat->setColorMode(colorMode);

        // Read current colors from UI buttons (they store osg_color property)
        auto colorFromBtn = [](QPushButton* btn) -> osg::Vec4 {
            QVariant v = btn->property("osg_color");
            if (!v.isValid()) return osg::Vec4(0,0,0,1);
            QColor c = v.value<QColor>();
            return osg::Vec4(c.redF(), c.greenF(), c.blueF(), c.alphaF());
        };

        osg::Vec4 ambient  = colorFromBtn(m_ambientColorBtn);
        osg::Vec4 diffuse  = colorFromBtn(m_diffuseColorBtn);
        osg::Vec4 specular = colorFromBtn(m_specularColorBtn);
        osg::Vec4 emission = colorFromBtn(m_emissionColorBtn);

        float alpha = static_cast<float>(m_alphaSpin->value());
        diffuse.a() = alpha;

        mat->setAmbient(osg::Material::FRONT_AND_BACK, ambient);
        mat->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);
        mat->setSpecular(osg::Material::FRONT_AND_BACK, specular);
        mat->setEmission(osg::Material::FRONT_AND_BACK, emission);
        mat->setShininess(osg::Material::FRONT_AND_BACK, static_cast<float>(m_shininessSpin->value()));

        ss->setAssociatedModes(mat, osg::StateAttribute::ON);
    }

    // Blend mode
    int blendMode = m_blendModeCombo->currentData().toInt();
    if (blendMode == 0)
    {
        ss->removeAttribute(osg::StateAttribute::BLENDFUNC);
        ss->setRenderingHint(osg::StateSet::OPAQUE_BIN);
    }
    else if (blendMode == 1)
    {
        osg::BlendFunc* bf = new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA);
        ss->setAttributeAndModes(bf, osg::StateAttribute::ON);
        ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    }
    else if (blendMode == 2)
    {
        osg::BlendFunc* bf = new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE);
        ss->setAttributeAndModes(bf, osg::StateAttribute::ON);
        ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    }

    // Depth write
    if (m_depthWriteCheck->isChecked())
    {
        ss->removeAttribute(osg::StateAttribute::DEPTH);
    }
    else
    {
        osg::Depth* depth = new osg::Depth;
        depth->setWriteMask(false);
        ss->setAttributeAndModes(depth, osg::StateAttribute::ON);
    }

    // Cull face
    int cullMode = m_cullFaceCombo->currentData().toInt();
    if (cullMode == -1)
    {
        // Off (two-sided)
        ss->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);
        ss->removeAttribute(osg::StateAttribute::CULLFACE);
    }
    else
    {
        osg::CullFace* cf = new osg::CullFace(static_cast<osg::CullFace::Mode>(cullMode));
        ss->setAttributeAndModes(cf, osg::StateAttribute::ON);
    }

    // Shade model
    osg::ShadeModel::Mode shadeMode = static_cast<osg::ShadeModel::Mode>(m_shadeModelCombo->currentData().toInt());
    osg::ShadeModel* sm = new osg::ShadeModel(shadeMode);
    ss->setAttribute(sm);

    // Polygon mode
    osg::PolygonMode::Mode polyMode = static_cast<osg::PolygonMode::Mode>(m_polygonModeCombo->currentData().toInt());
    osg::PolygonMode* pm = new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, polyMode);
    ss->setAttributeAndModes(pm, osg::StateAttribute::ON);

    // Line width
    if (m_lineWidthSpin->value() > 1.0)
    {
        osg::LineWidth* lw = new osg::LineWidth(static_cast<float>(m_lineWidthSpin->value()));
        ss->setAttribute(lw);
    }
    else
    {
        ss->removeAttribute(osg::StateAttribute::LINEWIDTH);
    }

    // Point size
    if (m_pointSizeSpin->value() > 1.0)
    {
        osg::Point* ps = new osg::Point(static_cast<float>(m_pointSizeSpin->value()));
        ss->setAttribute(ps);
    }
    else
    {
        ss->removeAttribute(osg::StateAttribute::POINT);
    }

    // Polygon offset
    float offsetF = static_cast<float>(m_polyOffsetFactor->value());
    float offsetU = static_cast<float>(m_polyOffsetUnits->value());
    if (offsetF != 0.0f || offsetU != 0.0f)
    {
        osg::PolygonOffset* po = new osg::PolygonOffset(offsetF, offsetU);
        ss->setAttributeAndModes(po, osg::StateAttribute::ON);
    }
    else
    {
        ss->removeAttribute(osg::StateAttribute::POLYGONOFFSET);
        ss->setMode(GL_POLYGON_OFFSET_FILL, osg::StateAttribute::OFF);
    }

    // Save new StateSet snapshot and push command
    cmd->setNewStateSet(node->getStateSet());
    m_undoStack->push(cmd);
}

void NodeEditorDock::applyPreset(const QString& presetName)
{
    // Classic material presets: Ambient, Diffuse, Specular, Emission, Shininess, Alpha, BlendMode(0=Off,1=Alpha,2=Additive)
    struct Preset {
        osg::Vec4 ambient, diffuse, specular, emission;
        float shininess;
        float alpha;
        int blendMode;
    };

    static const QHash<QString, Preset> presets = {
        {"Gold",       {{0.33,0.22,0.03,1}, {0.78,0.57,0.04,1}, {0.99,0.91,0.61,1}, {0,0,0,1}, 27.8f, 1.0f, 0}},
        {"Silver",     {{0.27,0.27,0.27,1}, {0.78,0.78,0.78,1}, {0.97,0.97,0.97,1}, {0,0,0,1}, 51.2f, 1.0f, 0}},
        {"Copper",     {{0.33,0.18,0.08,1}, {0.78,0.46,0.18,1}, {0.99,0.81,0.61,1}, {0,0,0,1}, 27.8f, 1.0f, 0}},
        {"Bronze",     {{0.33,0.22,0.07,1}, {0.71,0.46,0.16,1}, {0.39,0.27,0.17,1}, {0,0,0,1}, 12.8f, 1.0f, 0}},
        {"Chrome",     {{0.37,0.37,0.37,1}, {0.78,0.78,0.78,1}, {0.97,0.97,0.97,1}, {0,0,0,1}, 76.8f, 1.0f, 0}},
        {"Ruby",       {{0.35,0.05,0.05,1}, {0.75,0.12,0.12,1}, {0.95,0.55,0.55,1}, {0,0,0,1}, 38.4f, 0.85f, 1}},
        {"Emerald",    {{0.05,0.35,0.07,1}, {0.12,0.75,0.15,1}, {0.55,0.95,0.55,1}, {0,0,0,1}, 38.4f, 0.85f, 1}},
        {"Jade",       {{0.06,0.22,0.09,1}, {0.14,0.55,0.19,1}, {0.33,0.72,0.38,1}, {0.04,0.06,0.04,1}, 12.8f, 0.92f, 1}},
        {"Glass",      {{0.05,0.05,0.08,1}, {0.15,0.18,0.25,1}, {0.75,0.78,0.85,1}, {0.03,0.03,0.05,1}, 76.8f, 0.35f, 1}},
        {"PlasticRed", {{0.20,0.02,0.02,1}, {0.80,0.05,0.05,1}, {0.60,0.40,0.40,1}, {0,0,0,1}, 32.0f, 1.0f, 0}},
        {"PlasticGreen",{{0.02,0.20,0.02,1}, {0.05,0.80,0.05,1}, {0.40,0.60,0.40,1}, {0,0,0,1}, 32.0f, 1.0f, 0}},
        {"PlasticBlue", {{0.02,0.02,0.20,1}, {0.05,0.05,0.80,1}, {0.40,0.40,0.60,1}, {0,0,0,1}, 32.0f, 1.0f, 0}},
        {"PlasticWhite",{{0.40,0.40,0.40,1}, {0.85,0.85,0.85,1}, {0.90,0.90,0.90,1}, {0,0,0,1}, 32.0f, 1.0f, 0}},
        {"RubberBlack",{{0.02,0.02,0.02,1}, {0.14,0.14,0.14,1}, {0.10,0.10,0.10,1}, {0,0,0,1}, 6.0f, 1.0f, 0}},
        {"RubberRed",  {{0.05,0.01,0.01,1}, {0.50,0.04,0.04,1}, {0.20,0.06,0.06,1}, {0,0,0,1}, 6.0f, 1.0f, 0}},
        {"Ceramic",    {{0.20,0.20,0.22,1}, {0.88,0.88,0.90,1}, {0.95,0.95,0.95,1}, {0.05,0.05,0.05,1}, 64.0f, 1.0f, 0}},
    };

    auto it = presets.find(presetName);
    if (it == presets.end()) return;

    const Preset& p = it.value();

    // Enable material first
    m_materialEnabled->setChecked(true);

    // Set color mode to OFF (use material colors)
    m_colorModeCombo->setCurrentIndex(0);

    // Set colors on buttons
    setButtonColor(m_ambientColorBtn, p.ambient);
    setButtonColor(m_diffuseColorBtn, p.diffuse);
    setButtonColor(m_specularColorBtn, p.specular);
    setButtonColor(m_emissionColorBtn, p.emission);

    // Shininess
    m_shininessSpin->setValue(p.shininess);

    // Alpha
    m_alphaSlider->setValue(static_cast<int>(p.alpha * 100));
    m_alphaSpin->setValue(p.alpha);

    // Blend mode
    int blendIdx = m_blendModeCombo->findData(p.blendMode);
    if (blendIdx >= 0) m_blendModeCombo->setCurrentIndex(blendIdx);

    // Depth write: enable for opaque, disable for transparent
    m_depthWriteCheck->setChecked(p.blendMode == 0);

    // Apply to the node's StateSet
    applyMaterial();
}

void NodeEditorDock::setButtonColor(QPushButton* btn, const osg::Vec4& color)
{
    int r = static_cast<int>(color.r() * 255);
    int g = static_cast<int>(color.g() * 255);
    int b = static_cast<int>(color.b() * 255);
    int a = static_cast<int>(color.a() * 255);
    QColor c(r, g, b, a);
    // Use stylesheet for color indicator buttons - QPalette is overridden by theme QSS.
    // Widget-level stylesheet takes priority over application-level QSS.
    QString textColor = (r + g + b) / 3 < 128 ? "white" : "black";
    btn->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; "
                "border: 1px solid #666; border-radius: 3px; padding: 2px 6px; }"
                "QPushButton:hover { border: 2px solid #aaa; }")
            .arg(c.name(QColor::HexArgb), textColor));
    btn->setText(QString("%1, %2, %3, %4").arg(r).arg(g).arg(b).arg(a));
    btn->setProperty("osg_color", c);
}

// ============================================================================
// Texture Tab
// ============================================================================

QWidget* NodeEditorDock::createTextureTab()
{
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    m_textureScroll = new QScrollArea;
    m_textureScroll->setWidgetResizable(true);
    m_textureScroll->setFrameShape(QFrame::NoFrame);
    QWidget* content = new QWidget;
    QFormLayout* form = new QFormLayout(content);
    form->setContentsMargins(4, 4, 4, 4);

    m_textureEnabled = new QCheckBox(tr("Enable Texture"));
    form->addRow(m_textureEnabled);

    // Texture file path
    auto pathLayout = new QHBoxLayout;
    m_texturePathEdit = new QLineEdit;
    m_texturePathEdit->setReadOnly(true);
    m_textureBrowseBtn = new QPushButton(tr("Browse..."));
    pathLayout->addWidget(m_texturePathEdit, 1);
    pathLayout->addWidget(m_textureBrowseBtn);
    form->addRow(tr("Texture File"), pathLayout);

    // Preview
    m_texturePreview = new QLabel;
    m_texturePreview->setFixedSize(128, 128);
    m_texturePreview->setAlignment(Qt::AlignCenter);
    m_texturePreview->setFrameShape(QFrame::StyledPanel);
    m_texturePreview->setText(tr("No Texture"));
    form->addRow(tr("Preview"), m_texturePreview);

    // Wrap mode
    m_wrapSCombo = new QComboBox;
    m_wrapSCombo->addItem(tr("Repeat"), GL_REPEAT);
    m_wrapSCombo->addItem(tr("Clamp to Edge"), GL_CLAMP_TO_EDGE);
    m_wrapSCombo->addItem(tr("Mirror Repeat"), GL_MIRRORED_REPEAT);
    form->addRow(tr("Wrap S"), m_wrapSCombo);

    m_wrapTCombo = new QComboBox;
    m_wrapTCombo->addItem(tr("Repeat"), GL_REPEAT);
    m_wrapTCombo->addItem(tr("Clamp to Edge"), GL_CLAMP_TO_EDGE);
    m_wrapTCombo->addItem(tr("Mirror Repeat"), GL_MIRRORED_REPEAT);
    form->addRow(tr("Wrap T"), m_wrapTCombo);

    // Filter mode
    m_filterCombo = new QComboBox;
    m_filterCombo->addItem(tr("Nearest"), 0);
    m_filterCombo->addItem(tr("Linear"), 1);
    m_filterCombo->addItem(tr("MipMap (Trilinear)"), 2);
    form->addRow(tr("Filter"), m_filterCombo);

    m_removeTextureBtn = new QPushButton(tr("Remove Texture"));
    form->addRow(m_removeTextureBtn);

    form->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_textureScroll->setWidget(content);
    layout->addWidget(m_textureScroll);

    // Connections
    connect(m_textureEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_updatingTexture || !m_currentNode.valid()) return;
        osg::StateSet* ss = m_currentNode->getOrCreateStateSet();
        if (!checked)
        {
            ss->removeTextureAttribute(0, osg::StateAttribute::TEXTURE);
        }
        updateTextureUI();
    });
    connect(m_textureBrowseBtn, &QPushButton::clicked, this, &NodeEditorDock::browseTexture);
    connect(m_removeTextureBtn, &QPushButton::clicked, this, &NodeEditorDock::removeTexture);
    connect(m_wrapSCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingTexture && m_currentNode.valid()) applyTextureWrapFilter();
    });
    connect(m_wrapTCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingTexture && m_currentNode.valid()) applyTextureWrapFilter();
    });
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        if (!m_updatingTexture && m_currentNode.valid()) applyTextureWrapFilter();
    });

    return page;
}

void NodeEditorDock::applyTextureWrapFilter()
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    osg::StateSet* ss = node->getOrCreateStateSet();
    osg::Texture2D* tex = dynamic_cast<osg::Texture2D*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
    if (!tex) return;

    // Save old values
    osg::Texture::WrapMode oldWrapS = tex->getWrap(osg::Texture::WRAP_S);
    osg::Texture::WrapMode oldWrapT = tex->getWrap(osg::Texture::WRAP_T);
    osg::Texture::FilterMode oldMinF = tex->getFilter(osg::Texture::MIN_FILTER);
    osg::Texture::FilterMode oldMagF = tex->getFilter(osg::Texture::MAG_FILTER);

    // Compute new values
    osg::Texture::WrapMode newWrapS = static_cast<osg::Texture::WrapMode>(m_wrapSCombo->currentData().toInt());
    osg::Texture::WrapMode newWrapT = static_cast<osg::Texture::WrapMode>(m_wrapTCombo->currentData().toInt());

    int filterIdx = m_filterCombo->currentData().toInt();
    osg::Texture::FilterMode newMinF, newMagF;
    if (filterIdx == 0) { newMinF = osg::Texture::NEAREST; newMagF = osg::Texture::NEAREST; }
    else if (filterIdx == 1) { newMinF = osg::Texture::LINEAR; newMagF = osg::Texture::LINEAR; }
    else { newMinF = osg::Texture::LINEAR_MIPMAP_LINEAR; newMagF = osg::Texture::LINEAR; }

    m_undoStack->push(new TextureWrapFilterCommand(tex,
        oldWrapS, oldWrapT, oldMinF, oldMagF,
        newWrapS, newWrapT, newMinF, newMagF));
}

void NodeEditorDock::updateTextureUI()
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    m_updatingTexture = true;

    osg::StateSet* ss = node->getStateSet();
    osg::Texture2D* tex = nullptr;
    if (ss)
    {
        tex = dynamic_cast<osg::Texture2D*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
    }

    m_textureEnabled->setChecked(tex != nullptr);

    if (tex)
    {
        m_texturePathEdit->setText(QString::fromStdString(tex->getName()));

        // Wrap mode
        osg::Texture::WrapMode ws = tex->getWrap(osg::Texture::WRAP_S);
        osg::Texture::WrapMode wt = tex->getWrap(osg::Texture::WRAP_T);
        m_wrapSCombo->setCurrentIndex(ws == osg::Texture::REPEAT ? 0 : (ws == osg::Texture::CLAMP_TO_EDGE ? 1 : 2));
        m_wrapTCombo->setCurrentIndex(wt == osg::Texture::REPEAT ? 0 : (wt == osg::Texture::CLAMP_TO_EDGE ? 1 : 2));

        // Filter mode
        osg::Texture::FilterMode minF = tex->getFilter(osg::Texture::MIN_FILTER);
        m_filterCombo->setCurrentIndex(minF == osg::Texture::NEAREST ? 0 : (minF == osg::Texture::LINEAR ? 1 : 2));

        // Preview
        osg::Image* img = tex->getImage();
        if (img && img->data())
        {
            QImage::Format fmt = QImage::Format_RGBA8888;
            if (img->getPixelFormat() == GL_RGB) fmt = QImage::Format_RGB888;
            else if (img->getPixelFormat() == GL_RGBA) fmt = QImage::Format_RGBA8888;

            QImage qimg(img->data(), img->s(), img->t(), static_cast<int>(img->getRowStepInBytes()), fmt);
            if (!qimg.isNull())
                m_texturePreview->setPixmap(QPixmap::fromImage(qimg.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            else
                m_texturePreview->setText(tr("Preview N/A"));
        }
        else
        {
            m_texturePreview->setText(tr("No Preview"));
            m_texturePreview->setPixmap(QPixmap());
        }
    }
    else
    {
        m_texturePathEdit->clear();
        m_texturePreview->setText(tr("No Texture"));
        m_texturePreview->setPixmap(QPixmap());
        m_wrapSCombo->setCurrentIndex(0);
        m_wrapTCombo->setCurrentIndex(0);
        m_filterCombo->setCurrentIndex(2);
    }

    m_updatingTexture = false;
}

void NodeEditorDock::browseTexture()
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    QString filter = tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tga *.tiff *.tif *.gif *.psd);;All Files (*)");
    QString path = QFileDialog::getOpenFileName(this, tr("Select Texture"), QString(), filter);
    if (path.isEmpty()) return;

    osg::ref_ptr<osg::Image> image = osgDB::readImageFile(path.toLocal8Bit().toStdString());
    if (!image.valid())
    {
        bool hasUnicode = false;
        for (const QChar& c : path)
        {
            if (c.unicode() > 127) { hasUnicode = true; break; }
        }
        if (hasUnicode)
        {
            QTemporaryDir tempDir;
            if (tempDir.isValid())
            {
                QString tempPath = tempDir.path() + "/tex_temp." + QFileInfo(path).suffix();
                if (QFile::copy(path, tempPath))
                    image = osgDB::readImageFile(tempPath.toLocal8Bit().toStdString());
            }
        }
    }

    if (!image.valid())
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load texture: %1").arg(path));
        return;
    }

    osg::Texture2D* oldTex = nullptr;
    osg::StateSet* ss = node->getOrCreateStateSet();
    oldTex = dynamic_cast<osg::Texture2D*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

    osg::ref_ptr<osg::Texture2D> newTex = new osg::Texture2D(image);
    newTex->setName(path.toStdString());
    newTex->setWrap(osg::Texture::WRAP_S, static_cast<osg::Texture::WrapMode>(m_wrapSCombo->currentData().toInt()));
    newTex->setWrap(osg::Texture::WRAP_T, static_cast<osg::Texture::WrapMode>(m_wrapTCombo->currentData().toInt()));

    int filterIdx = m_filterCombo->currentData().toInt();
    if (filterIdx == 0)
    {
        newTex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
        newTex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);
    }
    else if (filterIdx == 1)
    {
        newTex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        newTex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    }
    else
    {
        newTex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
        newTex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    }

    m_undoStack->push(new TextureCommand(node, oldTex, newTex.get()));
    m_textureEnabled->setChecked(true);
    updateTextureUI();
}

void NodeEditorDock::removeTexture()
{
    osg::Node* node = m_currentNode.get();
    if (!node) return;

    osg::StateSet* ss = node->getOrCreateStateSet();
    osg::Texture2D* oldTex = dynamic_cast<osg::Texture2D*>(ss->getTextureAttribute(0, osg::StateAttribute::TEXTURE));

    m_undoStack->push(new TextureCommand(node, oldTex, nullptr));
    m_textureEnabled->setChecked(false);
    updateTextureUI();
}

// ============================================================================
// Public API
// ============================================================================

void NodeEditorDock::setNode(osg::Node* node)
{
    m_currentNode = node;
    syncFromNode();
}

void NodeEditorDock::syncFromNode()
{
    osg::Node* node = m_currentNode.get();

    // Scene tab is always active (scene-level, not node-level)
    syncSceneUI();

    if (!node)
    {
        // Only disable node-level tabs
        m_tabWidget->setTabEnabled(0, false);  // Transform
        m_tabWidget->setTabEnabled(1, false);  // Material
        m_tabWidget->setTabEnabled(2, false);  // Texture
        // Switch to Scene tab if a node-level tab was active
        if (m_tabWidget->currentIndex() < 3)
            m_tabWidget->setCurrentIndex(3);
        return;
    }

    m_tabWidget->setTabEnabled(0, true);
    m_tabWidget->setTabEnabled(1, true);
    m_tabWidget->setTabEnabled(2, true);
    updateTransformUI();
    updateMaterialUI();
    updateTextureUI();
}

// ============================================================================
// Scene Tab
// ============================================================================

QWidget* NodeEditorDock::createSceneTab()
{
    QWidget* page = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 4, 4, 4);

    // Light settings group
    QGroupBox* lightGroup = new QGroupBox(tr("Light Settings"));
    QVBoxLayout* lightLayout = new QVBoxLayout(lightGroup);

    m_lightEnabledCheck = new QCheckBox(tr("Enable Lighting"));
    m_lightEnabledCheck->setChecked(true);
    connect(m_lightEnabledCheck, &QCheckBox::stateChanged, this, [this](int state) {
        if (m_updatingScene || !m_osgWidget) return;
        m_osgWidget->setLightEnabled(state == Qt::Checked);
    });
    lightLayout->addWidget(m_lightEnabledCheck);

    m_sceneTwoSidedCheck = new QCheckBox(tr("Two-sided Lighting"));
    m_sceneTwoSidedCheck->setChecked(true);
    connect(m_sceneTwoSidedCheck, &QCheckBox::stateChanged, this, [this](int state) {
        if (m_updatingScene || !m_osgWidget) return;
        m_osgWidget->setTwoSidedLighting(state == Qt::Checked);
    });
    lightLayout->addWidget(m_sceneTwoSidedCheck);

    // Light colors
    QFormLayout* colorLayout = new QFormLayout();

    m_lightAmbientBtn = new QPushButton;
    m_lightAmbientBtn->setMinimumHeight(26);
    connect(m_lightAmbientBtn, &QPushButton::clicked, this, [this]() {
        if (m_updatingScene || !m_osgWidget) return;
        osg::Vec4 c = m_osgWidget->ambientColor();
        QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Ambient Color"));
        if (color.isValid())
        {
            m_osgWidget->setAmbientColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
            setLightColorButton(m_lightAmbientBtn, color);
        }
    });
    colorLayout->addRow(tr("Ambient:"), m_lightAmbientBtn);

    m_lightDiffuseBtn = new QPushButton;
    m_lightDiffuseBtn->setMinimumHeight(26);
    connect(m_lightDiffuseBtn, &QPushButton::clicked, this, [this]() {
        if (m_updatingScene || !m_osgWidget) return;
        osg::Vec4 c = m_osgWidget->diffuseColor();
        QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Diffuse Color"));
        if (color.isValid())
        {
            m_osgWidget->setDiffuseColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
            setLightColorButton(m_lightDiffuseBtn, color);
        }
    });
    colorLayout->addRow(tr("Diffuse:"), m_lightDiffuseBtn);

    m_lightSpecularBtn = new QPushButton;
    m_lightSpecularBtn->setMinimumHeight(26);
    connect(m_lightSpecularBtn, &QPushButton::clicked, this, [this]() {
        if (m_updatingScene || !m_osgWidget) return;
        osg::Vec4 c = m_osgWidget->specularColor();
        QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Specular Color"));
        if (color.isValid())
        {
            m_osgWidget->setSpecularColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
            setLightColorButton(m_lightSpecularBtn, color);
        }
    });
    colorLayout->addRow(tr("Specular:"), m_lightSpecularBtn);

    lightLayout->addLayout(colorLayout);

    // Light position
    QGroupBox* posGroup = new QGroupBox(tr("Light Position"));
    QFormLayout* posLayout = new QFormLayout(posGroup);

    m_lightPosX = new QDoubleSpinBox;
    m_lightPosX->setRange(-100, 100); m_lightPosX->setSingleStep(0.1); m_lightPosX->setValue(1.0);
    m_lightPosY = new QDoubleSpinBox;
    m_lightPosY->setRange(-100, 100); m_lightPosY->setSingleStep(0.1); m_lightPosY->setValue(1.0);
    m_lightPosZ = new QDoubleSpinBox;
    m_lightPosZ->setRange(-100, 100); m_lightPosZ->setSingleStep(0.1); m_lightPosZ->setValue(1.0);

    auto onLightPosChanged = [this]() {
        if (m_updatingScene || !m_osgWidget) return;
        m_osgWidget->setLightPosition(osg::Vec4(
            m_lightPosX->value(), m_lightPosY->value(), m_lightPosZ->value(), 0.0f));
    };
    connect(m_lightPosX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onLightPosChanged);
    connect(m_lightPosY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onLightPosChanged);
    connect(m_lightPosZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, onLightPosChanged);

    posLayout->addRow("X:", m_lightPosX);
    posLayout->addRow("Y:", m_lightPosY);
    posLayout->addRow("Z:", m_lightPosZ);

    lightLayout->addWidget(posGroup);
    layout->addWidget(lightGroup);

    // Background color
    QGroupBox* bgGroup = new QGroupBox(tr("Background"));
    QHBoxLayout* bgLayout = new QHBoxLayout(bgGroup);
    m_bgColorBtn = new QPushButton;
    m_bgColorBtn->setMinimumHeight(26);
    connect(m_bgColorBtn, &QPushButton::clicked, this, [this]() {
        if (m_updatingScene || !m_osgWidget) return;
        osg::Vec4 c = m_osgWidget->backgroundColor();
        QColor color = QColorDialog::getColor(QColor(int(c.r()*255), int(c.g()*255), int(c.b()*255)), this, tr("Select Background Color"));
        if (color.isValid())
        {
            m_osgWidget->setBackgroundColor(osg::Vec4(color.redF(), color.greenF(), color.blueF(), 1.0f));
            setLightColorButton(m_bgColorBtn, color);
        }
    });
    bgLayout->addWidget(new QLabel(tr("Color:")));
    bgLayout->addWidget(m_bgColorBtn);
    bgLayout->addStretch();
    layout->addWidget(bgGroup);

    layout->addStretch();

    return page;
}

void NodeEditorDock::syncSceneUI()
{
    if (!m_osgWidget) return;

    m_updatingScene = true;

    m_lightEnabledCheck->setChecked(m_osgWidget->isLightEnabled());
    m_sceneTwoSidedCheck->setChecked(m_osgWidget->isTwoSidedLighting());

    osg::Vec4 ambient = m_osgWidget->ambientColor();
    osg::Vec4 diffuse = m_osgWidget->diffuseColor();
    osg::Vec4 specular = m_osgWidget->specularColor();
    osg::Vec4 bg = m_osgWidget->backgroundColor();

    setLightColorButton(m_lightAmbientBtn, QColor(int(ambient.r()*255), int(ambient.g()*255), int(ambient.b()*255)));
    setLightColorButton(m_lightDiffuseBtn, QColor(int(diffuse.r()*255), int(diffuse.g()*255), int(diffuse.b()*255)));
    setLightColorButton(m_lightSpecularBtn, QColor(int(specular.r()*255), int(specular.g()*255), int(specular.b()*255)));
    setLightColorButton(m_bgColorBtn, QColor(int(bg.r()*255), int(bg.g()*255), int(bg.b()*255)));

    osg::Vec4 pos = m_osgWidget->lightPosition();
    m_lightPosX->setValue(pos.x());
    m_lightPosY->setValue(pos.y());
    m_lightPosZ->setValue(pos.z());

    m_updatingScene = false;
}

void NodeEditorDock::setLightColorButton(QPushButton* btn, const QColor& color)
{
    // Widget-level stylesheet takes priority over application-level QSS theme
    QString textColor = (color.red() + color.green() + color.blue()) / 3 < 128 ? "white" : "black";
    btn->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; "
                "border: 1px solid #666; border-radius: 3px; padding: 2px 6px; }"
                "QPushButton:hover { border: 2px solid #aaa; }")
            .arg(color.name(QColor::HexArgb), textColor));
    btn->setText(QString("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue()));
}
