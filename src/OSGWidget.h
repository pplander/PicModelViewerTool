#pragma once

#include <QOpenGLWidget>
#include <QTimer>
#include <QSizePolicy>

#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osg/Node>
#include <osg/Vec4>
#include <osg/Light>
#include <osg/Material>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/LineWidth>
#include <osgUtil/LineSegmentIntersector>

class PreProcessManager;

class OSGWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit OSGWidget(QWidget* parent = nullptr);
    ~OSGWidget() override;

    void setSceneData(osg::Node* node);
    osg::Node* getSceneData() const;

    osgViewer::Viewer* getViewer() const { return m_viewer.get(); }

    enum DisplayMode
    {
        Solid = 0,
        Wireframe,
        Points,
        SolidAndWireframe
    };

    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_displayMode; }

    void setBackgroundColor(const osg::Vec4& color);
    osg::Vec4 backgroundColor() const;

    void resetCamera();
    void fitToView();

    void setLightEnabled(bool enabled);
    bool isLightEnabled() const { return m_lightEnabled; }

    void setLightPosition(const osg::Vec4& pos);
    osg::Vec4 lightPosition() const;

    void setAmbientColor(const osg::Vec4& color);
    osg::Vec4 ambientColor() const;

    void setDiffuseColor(const osg::Vec4& color);
    osg::Vec4 diffuseColor() const;

    void setSpecularColor(const osg::Vec4& color);
    osg::Vec4 specularColor() const;

    void setTwoSidedLighting(bool enabled);
    bool isTwoSidedLighting() const { return m_twoSidedLighting; }

    // Node selection and highlighting
    void selectNode(osg::Node* node);
    void clearSelection();
    osg::Node* getSelectedNode() const { return m_selectedNode.get(); }

    // Node mask for highlight overlay geodes (excluded from picking and scene tree)
    static const unsigned int HIGHLIGHT_NODE_MASK = 0x80000000u;

    // Pre-process effects
    PreProcessManager* preProcessManager() const { return m_preProcessManager; }
    void ensurePreProcessInitialized();

    // Send a key event to OSG event handlers (ASCII key code)
    void sendOSGKeyEvent(int key);

    // Custom handler toggles (for handlers not built into OSG)
    void toggleLight();       // LightHandler (L)
    void toggleWireframe();   // WireframeHandler (W)
    void toggleAxes();        // AxesHandler (A)
    void toggleTexture();     // TextureHandler (T)

    bool isAxesVisible() const { return m_axesVisible; }
    bool isTextureEnabled() const { return m_textureEnabled; }

signals:
    void sceneChanged();
    void fpsChanged(double fps);
    void nodeClicked(osg::Node* node);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void initViewer();
    void setupLight();
    void updateDisplayMode();
    void updateFPS();
    void pickNode(int x, int y);
    void highlightNode(osg::Node* node);
    void unhighlightNode(osg::Node* node);
    osg::Geode* createAxesGeode();
    osg::Geode* createBoundingBoxGeode(const osg::BoundingBox& bb);

    osg::ref_ptr<osgViewer::Viewer> m_viewer;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> m_graphicsWindow;
    osg::ref_ptr<osg::Light> m_light;
    osg::ref_ptr<osg::StateSet> m_displayStateSet;

    // Selection state
    osg::ref_ptr<osg::Node> m_selectedNode;
    osg::ref_ptr<osg::Geode> m_highlightGeode;
    osg::ref_ptr<osg::Group> m_overlayGroup;  // Persistent overlay layer for highlight visuals
    osg::ref_ptr<osg::Group> m_wireframeOverlay;  // Lazily built second-pass wireframe (Solid+Wireframe only)
    osg::ref_ptr<osg::Node> m_modelRoot;  // The user-supplied scene root, kept so we can clone on demand

    // Click detection for pick
    QPoint m_mousePressPos;
    bool m_mouseDragged = false;

    QTimer m_timer;
    DisplayMode m_displayMode = Solid;
    bool m_lightEnabled = true;
    bool m_twoSidedLighting = true;

    // Custom handler state
    bool m_axesVisible = false;
    bool m_textureEnabled = true;
    osg::ref_ptr<osg::Group> m_axesGroup;
    osg::ref_ptr<osg::StateSet> m_savedRootStateSet;

    // Pre-process effects
    PreProcessManager* m_preProcessManager = nullptr;
    float m_preProcessTime = 0.0f;

    int m_lastFrameCount = 0;
    double m_lastFPSTime = 0.0;
};
