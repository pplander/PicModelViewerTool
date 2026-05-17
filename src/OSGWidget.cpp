#include "OSGWidget.h"
#include "PreProcessManager.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

#include <osg/DisplaySettings>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PolygonMode>
#include <osg/PolygonOffset>
#include <osg/Material>
#include <osg/LineWidth>
#include <osg/CopyOp>
#include <osgGA/TrackballManipulator>
#include <osgUtil/Optimizer>
#include <osgUtil/IntersectionVisitor>
#include <osg/ComputeBoundsVisitor>
#include <osg/CullSettings>

namespace {
// Custom clamp callback: forces a very small minimum zNear independent of
// nearFarRatio, so models close to the camera (or camera inside the model)
// are never clipped by the near plane.
struct TightNearClamp : public osg::CullSettings::ClampProjectionMatrixCallback
{
    static constexpr double kMinNear = 1e-3;

    template<typename M>
    static void adjust(M& projection, double& znear, double& zfar)
    {
        if (zfar < znear) std::swap(znear, zfar);
        if (znear < kMinNear) znear = kMinNear;
        if (zfar  < znear * 2.0) zfar = znear * 2.0;
        // rebuild perspective projection (or shifted frustum) preserving x/y planes
        double l, r, b, t, n, f;
        if (projection.getFrustum(l, r, b, t, n, f)) {
            double sx = znear / n;
            projection.makeFrustum(l * sx, r * sx, b * sx, t * sx, znear, zfar);
        } else {
            // ortho: keep as-is, only update near/far
            double oL, oR, oB, oT, oN, oF;
            if (projection.getOrtho(oL, oR, oB, oT, oN, oF)) {
                projection.makeOrtho(oL, oR, oB, oT, znear, zfar);
            }
        }
    }

    bool clampProjectionMatrixImplementation(osg::Matrixf& projection, double& znear, double& zfar) const override
    { adjust(projection, znear, zfar); return true; }
    bool clampProjectionMatrixImplementation(osg::Matrixd& projection, double& znear, double& zfar) const override
    { adjust(projection, znear, zfar); return true; }
};
} // namespace

#include <OpenThreads/Thread>
#include <vector>

OSGWidget::OSGWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 200);

    // Ensure widget expands to fill available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_preProcessManager = new PreProcessManager();

    initViewer();

    // Render timer ~60 FPS
    connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&OSGWidget::update));
    m_timer.start(16);
}

OSGWidget::~OSGWidget()
{
    m_timer.stop();
    delete m_preProcessManager;
}

void OSGWidget::initViewer()
{
    m_viewer = new osgViewer::Viewer;

    // Graphics window embedded - use 1x1 initially, will be updated in resizeGL
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 0;
    traits->y = 0;
    traits->width = 1;
    traits->height = 1;
    traits->windowDecoration = false;
    traits->doubleBuffer = true;
    traits->sharedContext = nullptr;
    traits->inheritedWindowData = nullptr;
    traits->samples = 4;  // Match QSurfaceFormat multisampling

    m_graphicsWindow = new osgViewer::GraphicsWindowEmbedded(traits.get());
    m_viewer->getCamera()->setGraphicsContext(m_graphicsWindow.get());
    m_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, 1, 1));

    // Default background color
    m_viewer->getCamera()->setClearColor(osg::Vec4(0.2f, 0.2f, 0.25f, 1.0f));

    // Auto near/far: use primitives for tighter bounds, smaller ratio so near plane
    // is closer to camera (avoid clipping when zooming in). Custom clamp callback
    // additionally forces a sub-millimeter minimum zNear regardless of zFar/ratio.
    m_viewer->getCamera()->setComputeNearFarMode(osg::CullSettings::COMPUTE_NEAR_FAR_USING_PRIMITIVES);
    m_viewer->getCamera()->setNearFarRatio(0.00001);
    m_viewer->getCamera()->setClampProjectionMatrixCallback(new TightNearClamp);

    // Set up camera manipulator
    m_viewer->setCameraManipulator(new osgGA::TrackballManipulator);

    // Threading model
    m_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

    // Light
    setupLight();

    // Display state set for wireframe/points modes
    m_displayStateSet = new osg::StateSet;

    // Register OSG built-in event handlers
    m_viewer->addEventHandler(new osgViewer::StatsHandler);
    m_viewer->addEventHandler(new osgViewer::ThreadingHandler);
    m_viewer->addEventHandler(new osgViewer::ScreenCaptureHandler);
    m_viewer->addEventHandler(new osgViewer::HelpHandler);
    m_viewer->addEventHandler(new osgViewer::LODScaleHandler);
    m_viewer->addEventHandler(new osgViewer::WindowSizeHandler);
}

void OSGWidget::setupLight()
{
    m_light = new osg::Light;
    m_light->setLightNum(0);
    m_light->setPosition(osg::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
    m_light->setAmbient(osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));
    m_light->setDiffuse(osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
    m_light->setSpecular(osg::Vec4(0.5f, 0.5f, 0.5f, 1.0f));

    m_viewer->setLight(m_light.get());
    m_viewer->getCamera()->setReferenceFrame(osg::Transform::ABSOLUTE_RF);

    // Enable lighting by default
    m_viewer->getCamera()->getOrCreateStateSet()->setMode(GL_LIGHTING,
        osg::StateAttribute::ON);
    m_viewer->getCamera()->getOrCreateStateSet()->setMode(GL_LIGHT0,
        osg::StateAttribute::ON);
}

void OSGWidget::setSceneData(osg::Node* node)
{
    if (!m_viewer) return;

    // Stop rendering timer while replacing scene to avoid race condition
    m_timer.stop();

    // Clear any existing selection highlight
    clearSelection();

    // Remove overlay group from old scene
    if (m_overlayGroup && m_overlayGroup->getNumParents() > 0)
    {
        m_overlayGroup->getParent(0)->removeChild(m_overlayGroup);
    }

    // Detach wireframe overlay from old scene and clear its children so it
    // does not hold a stale reference to the previous model. The overlay
    // itself is rebuilt lazily on demand inside updateDisplayMode().
    if (m_wireframeOverlay)
    {
        for (unsigned i = m_wireframeOverlay->getNumParents(); i > 0; --i)
            m_wireframeOverlay->getParent(i - 1)->removeChild(m_wireframeOverlay);
        m_wireframeOverlay->removeChildren(0, m_wireframeOverlay->getNumChildren());
    }
    m_modelRoot = nullptr;

    // Re-create overlay group
    m_overlayGroup = new osg::Group;
    m_overlayGroup->setNodeMask(HIGHLIGHT_NODE_MASK);
    m_overlayGroup->setName("_highlight_overlay");

    if (node)
    {
        osgUtil::Optimizer optimizer;
        optimizer.optimize(node, osgUtil::Optimizer::DEFAULT_OPTIMIZATIONS);

        // Set new scene data BEFORE detaching old root,
        // so the viewer always holds a valid scene graph.
        osg::Group* root = node->asGroup();
        if (!root)
        {
            // Wrap non-Group root (e.g. a single Geode) so overlay can be attached
            root = new osg::Group;
            root->addChild(node);
        }
        root->addChild(m_overlayGroup);

        // Remember the user-supplied node; the wireframe overlay (when used)
        // will clone this on demand the first time the user switches to
        // Solid+Wireframe mode, so loading a model does NOT alter the scene
        // graph for the common Solid/Wireframe/Points display modes.
        m_modelRoot = node;

        m_viewer->setSceneData(root);
        fitToView();
    }
    else
    {
        m_viewer->setSceneData(nullptr);
    }

    updateDisplayMode();

    // Restart rendering
    m_timer.start(16);

    // Re-attach pre-process if any effect is enabled
    if (m_preProcessManager && m_preProcessManager->hasEnabledEffects()) {
        if (auto* root = dynamic_cast<osg::Group*>(m_viewer->getSceneData())) {
            m_preProcessManager->attach(root);
        }
    }

    emit sceneChanged();
}

osg::Node* OSGWidget::getSceneData() const
{
    return m_viewer ? m_viewer->getSceneData() : nullptr;
}

void OSGWidget::setDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;
    updateDisplayMode();
    update();
}

void OSGWidget::updateDisplayMode()
{
    if (!m_viewer || !m_viewer->getSceneData()) return;

    osg::StateSet* rootSS = m_viewer->getSceneData()->getOrCreateStateSet();

    // Remove previous polygon mode
    rootSS->removeAttribute(osg::StateAttribute::POLYGONMODE);
    rootSS->removeAttribute(osg::StateAttribute::POLYGONOFFSET);
    rootSS->setMode(GL_POLYGON_OFFSET_FILL, osg::StateAttribute::OFF);

    // Wireframe overlay (second pass) is off by default; only enabled when
    // SolidAndWireframe is selected.
    if (m_wireframeOverlay)
        m_wireframeOverlay->setNodeMask(0u);

    // Tear down the lazily-built wireframe overlay whenever we leave the
    // Solid+Wireframe mode, so the scene graph stays exactly as the loader
    // produced it for the standard display modes (no clones, no extra
    // memory, no surprise side effects on picking / bounds / export).
    auto destroyWireframeOverlay = [this]() {
        if (!m_wireframeOverlay) return;
        for (unsigned i = m_wireframeOverlay->getNumParents(); i > 0; --i)
            m_wireframeOverlay->getParent(i - 1)->removeChild(m_wireframeOverlay);
        m_wireframeOverlay->removeChildren(0, m_wireframeOverlay->getNumChildren());
        m_wireframeOverlay = nullptr;
    };

    switch (m_displayMode)
    {
    case Solid:
        rootSS->setAttributeAndModes(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL));
        break;
    case Wireframe:
        rootSS->setAttributeAndModes(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE));
        break;
    case Points:
        rootSS->setAttributeAndModes(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::POINT));
        break;
    case SolidAndWireframe:
    {
        // First pass: solid fill, pushed back via polygon offset so that the
        // wireframe pass sits cleanly on top without z-fighting.
        rootSS->setAttributeAndModes(
            new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL));
        rootSS->setAttributeAndModes(new osg::PolygonOffset(2.0f, 2.0f));
        rootSS->setMode(GL_POLYGON_OFFSET_FILL, osg::StateAttribute::ON);

        // Second pass: lazily build (the first time we enter this mode) a
        // sibling node that re-renders the same model with PolygonMode=LINE
        // + bright emissive material. This stays attached only while this
        // mode is selected; it is torn down below for any other mode.
        if (!m_wireframeOverlay && m_modelRoot.valid())
        {
            m_wireframeOverlay = new osg::Group;
            m_wireframeOverlay->setName("_wireframe_overlay");

            auto* wss = m_wireframeOverlay->getOrCreateStateSet();
            wss->setAttributeAndModes(
                new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK,
                                     osg::PolygonMode::LINE),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
            wss->setAttributeAndModes(
                new osg::LineWidth(1.5f),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
            wss->setMode(GL_LIGHTING,
                osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
            wss->setTextureMode(0, GL_TEXTURE_2D,
                osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

            auto* mat = new osg::Material;
            mat->setColorMode(osg::Material::OFF);
            mat->setEmission(osg::Material::FRONT_AND_BACK,
                             osg::Vec4(1.0f, 0.85f, 0.1f, 1.0f));
            mat->setAmbient(osg::Material::FRONT_AND_BACK,
                            osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            mat->setDiffuse(osg::Material::FRONT_AND_BACK,
                            osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            mat->setSpecular(osg::Material::FRONT_AND_BACK,
                             osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            wss->setAttributeAndModes(mat,
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

            // Deep-copy node hierarchy only; drawables / state are shared,
            // so we never multi-parent the original model node (which can
            // crash OSG visitors that walk a node's parent chain).
            osg::ref_ptr<osg::Object> objClone =
                m_modelRoot->clone(osg::CopyOp(osg::CopyOp::DEEP_COPY_NODES));
            osg::ref_ptr<osg::Node> wfClone =
                dynamic_cast<osg::Node*>(objClone.get());
            if (wfClone.valid())
                m_wireframeOverlay->addChild(wfClone.get());
        }
        if (m_wireframeOverlay)
        {
            if (auto* sceneRoot = dynamic_cast<osg::Group*>(m_viewer->getSceneData()))
            {
                if (m_wireframeOverlay->getNumParents() == 0)
                    sceneRoot->addChild(m_wireframeOverlay);
            }
            m_wireframeOverlay->setNodeMask(HIGHLIGHT_NODE_MASK);
        }
        break;
    }
    }

    if (m_displayMode != SolidAndWireframe)
        destroyWireframeOverlay();
}

void OSGWidget::setBackgroundColor(const osg::Vec4& color)
{
    if (m_viewer)
    {
        m_viewer->getCamera()->setClearColor(color);
    }
}

osg::Vec4 OSGWidget::backgroundColor() const
{
    if (m_viewer)
    {
        return m_viewer->getCamera()->getClearColor();
    }
    return osg::Vec4();
}

void OSGWidget::resetCamera()
{
    if (m_viewer && m_viewer->getCameraManipulator())
    {
        m_viewer->getCameraManipulator()->home(0.0);
    }
}

void OSGWidget::fitToView()
{
    if (!m_viewer || !m_viewer->getSceneData()) return;
    resetCamera();
}

void OSGWidget::setLightEnabled(bool enabled)
{
    m_lightEnabled = enabled;
    if (m_viewer && m_viewer->getCamera())
    {
        osg::StateSet* ss = m_viewer->getCamera()->getOrCreateStateSet();
        ss->setMode(GL_LIGHTING, enabled ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
        ss->setMode(GL_LIGHT0, enabled ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
    }
}

void OSGWidget::setLightPosition(const osg::Vec4& pos)
{
    if (m_light)
    {
        m_light->setPosition(pos);
    }
}

osg::Vec4 OSGWidget::lightPosition() const
{
    return m_light ? m_light->getPosition() : osg::Vec4();
}

void OSGWidget::setAmbientColor(const osg::Vec4& color)
{
    if (m_light) m_light->setAmbient(color);
}

osg::Vec4 OSGWidget::ambientColor() const
{
    return m_light ? m_light->getAmbient() : osg::Vec4();
}

void OSGWidget::setDiffuseColor(const osg::Vec4& color)
{
    if (m_light) m_light->setDiffuse(color);
}

osg::Vec4 OSGWidget::diffuseColor() const
{
    return m_light ? m_light->getDiffuse() : osg::Vec4();
}

void OSGWidget::setSpecularColor(const osg::Vec4& color)
{
    if (m_light) m_light->setSpecular(color);
}

osg::Vec4 OSGWidget::specularColor() const
{
    return m_light ? m_light->getSpecular() : osg::Vec4();
}

void OSGWidget::setTwoSidedLighting(bool enabled)
{
    m_twoSidedLighting = enabled;
    if (m_viewer && m_viewer->getCamera())
    {
        osg::StateSet* ss = m_viewer->getCamera()->getOrCreateStateSet();
        ss->setMode(GL_LIGHT_MODEL_TWO_SIDE,
            enabled ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
    }
}

void OSGWidget::initializeGL()
{
    if (m_viewer)
    {
        m_viewer->realize();
    }
}

void OSGWidget::resizeGL(int width, int height)
{
    // Account for device pixel ratio (high DPI support)
    qreal pixelRatio = devicePixelRatioF();
    int pixelW = static_cast<int>(width * pixelRatio);
    int pixelH = static_cast<int>(height * pixelRatio);

    if (m_graphicsWindow)
    {
        m_graphicsWindow->resized(0, 0, pixelW, pixelH);
        m_graphicsWindow->getEventQueue()->windowResize(0, 0, pixelW, pixelH);
    }
    if (m_viewer && m_viewer->getCamera())
    {
        m_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, pixelW, pixelH));
        double aspectRatio = static_cast<double>(pixelW) / static_cast<double>(pixelH > 0 ? pixelH : 1);
        // Use auto near/far computation by setting a wide range
        // The TrackballManipulator will adjust based on scene bounding sphere
        m_viewer->getCamera()->setProjectionMatrixAsPerspective(45.0, aspectRatio, 0.001, 100000.0);
    }
}

void OSGWidget::paintGL()
{
    if (m_viewer)
    {
        // Ensure the viewport matches the actual widget pixel size
        qreal pixelRatio = devicePixelRatioF();
        int pixelW = static_cast<int>(this->width() * pixelRatio);
        int pixelH = static_cast<int>(this->height() * pixelRatio);
        m_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, pixelW, pixelH));

        m_viewer->frame();
        updateFPS();

        // Update pre-process time for animated effects
        if (m_preProcessManager && m_preProcessManager->hasEnabledEffects()) {
            m_preProcessTime += 0.016f;
            m_preProcessManager->updateTime(m_preProcessTime);
        }
    }
}

void OSGWidget::updateFPS()
{
    if (!m_viewer || !m_viewer->getFrameStamp()) return;

    static int frameCount = 0;
    frameCount++;

    double currentTime = m_viewer->getFrameStamp()->getReferenceTime();
    if (m_lastFPSTime == 0.0)
    {
        m_lastFPSTime = currentTime;
        m_lastFrameCount = 0;
        return;
    }

    double elapsed = currentTime - m_lastFPSTime;
    if (elapsed >= 1.0)
    {
        double fps = (frameCount - m_lastFrameCount) / elapsed;
        emit fpsChanged(fps);
        m_lastFPSTime = currentTime;
        m_lastFrameCount = frameCount;
    }
}

void OSGWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_graphicsWindow)
    {
        int button = 0;
        switch (event->button())
        {
        case Qt::LeftButton:  button = 1; break;
        case Qt::MiddleButton: button = 2; break;
        case Qt::RightButton: button = 3; break;
        default: break;
        }
        float x = static_cast<float>(event->position().x());
        float y = static_cast<float>(event->position().y());
        m_graphicsWindow->getEventQueue()->mouseButtonPress(x, y, button);
    }

    // Track mouse press position for click detection
    m_mousePressPos = event->pos();
    m_mouseDragged = false;
}

void OSGWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_graphicsWindow)
    {
        int button = 0;
        switch (event->button())
        {
        case Qt::LeftButton:  button = 1; break;
        case Qt::MiddleButton: button = 2; break;
        case Qt::RightButton: button = 3; break;
        default: break;
        }
        float x = static_cast<float>(event->position().x());
        float y = static_cast<float>(event->position().y());
        m_graphicsWindow->getEventQueue()->mouseButtonRelease(x, y, button);
    }

    // Detect click (not drag) for node picking
    if (event->button() == Qt::LeftButton && !m_mouseDragged)
    {
        pickNode(static_cast<int>(event->position().x()), static_cast<int>(event->position().y()));
    }
}

void OSGWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_graphicsWindow)
    {
        float x = static_cast<float>(event->position().x());
        float y = static_cast<float>(event->position().y());
        m_graphicsWindow->getEventQueue()->mouseMotion(x, y);
    }

    // Detect drag (move more than 4px from press position)
    if (!m_mouseDragged)
    {
        QPoint delta = event->pos() - m_mousePressPos;
        if (delta.manhattanLength() > 4)
            m_mouseDragged = true;
    }
}

void OSGWidget::wheelEvent(QWheelEvent* event)
{
    if (m_graphicsWindow)
    {
        int delta = event->angleDelta().y();
        float x = static_cast<float>(event->position().x());
        float y = static_cast<float>(event->position().y());
        // Update mouse position before scroll
        m_graphicsWindow->getEventQueue()->mouseMotion(x, y);
        m_graphicsWindow->getEventQueue()->mouseScroll(
            delta > 0 ? osgGA::GUIEventAdapter::SCROLL_UP : osgGA::GUIEventAdapter::SCROLL_DOWN);
    }
}

void OSGWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Double click also triggers pick
    if (event->button() == Qt::LeftButton)
    {
        pickNode(static_cast<int>(event->position().x()), static_cast<int>(event->position().y()));
    }
    // Pass to base for default handling
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void OSGWidget::keyPressEvent(QKeyEvent* event)
{
    if (m_graphicsWindow)
    {
        // Convert Qt key code to OSG-compatible key code
        int key = event->key();

        // Letter keys: Qt uses uppercase codes (A=65, Z=90)
        // OSG handlers check against lowercase ASCII ('a'=97, 'z'=122)
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
        {
            key = key + 32; // Convert to lowercase ASCII
        }
        m_graphicsWindow->getEventQueue()->keyPress(
            static_cast<osgGA::GUIEventAdapter::KeySymbol>(key));
    }
}

void OSGWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (m_graphicsWindow)
    {
        int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z)
        {
            key = key + 32;
        }
        m_graphicsWindow->getEventQueue()->keyRelease(
            static_cast<osgGA::GUIEventAdapter::KeySymbol>(key));
    }
}

void OSGWidget::ensurePreProcessInitialized()
{
    if (!m_preProcessManager || !m_viewer) return;
    if (m_preProcessManager->isAttached()) return;

    auto* sceneData = m_viewer->getSceneData();
    if (!sceneData) return;

    auto* root = sceneData->asGroup();
    if (!root) {
        // Scene might be a leaf node; wrap check
        osg::Group* parent = new osg::Group;
        parent->addChild(sceneData);
        m_viewer->setSceneData(parent);
        root = parent;
    }
    m_preProcessManager->attach(root);
}

void OSGWidget::sendOSGKeyEvent(int key)
{
    if (m_graphicsWindow)
    {
        m_graphicsWindow->getEventQueue()->keyPress(
            static_cast<osgGA::GUIEventAdapter::KeySymbol>(key));
        m_graphicsWindow->getEventQueue()->keyRelease(
            static_cast<osgGA::GUIEventAdapter::KeySymbol>(key));
    }
}

// ---- Node Selection and Highlighting ----

void OSGWidget::pickNode(int x, int y)
{
    if (!m_viewer || !m_viewer->getSceneData()) return;

    // Convert widget coordinates to OSG window pixel coordinates
    qreal pixelRatio = devicePixelRatioF();
    float px = static_cast<float>(x * pixelRatio);
    float py = static_cast<float>(height() * pixelRatio - y * pixelRatio); // flip Y for OpenGL

    // Use LineSegmentIntersector with WINDOW coordinate frame on the camera
    // This correctly applies camera matrices to compute the ray
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
        new osgUtil::LineSegmentIntersector(osgUtil::LineSegmentIntersector::WINDOW, px, py);
    osgUtil::IntersectionVisitor iv(intersector.get());
    iv.setTraversalMask(~OSGWidget::HIGHLIGHT_NODE_MASK);

    // Must apply on camera - the visitor traverses from camera through scene
    const_cast<osg::Camera*>(m_viewer->getCamera())->accept(iv);

    if (intersector->containsIntersections())
    {
        osgUtil::LineSegmentIntersector::Intersections& intersections = intersector->getIntersections();
        for (auto& hit : intersections)
        {
            const osg::NodePath& nodePath = hit.nodePath;
            // Walk from leaf to root, find the most specific selectable node
            // Prefer Geode or leaf nodes over intermediate Group nodes
            // so that editing material only affects the clicked sub-object
            for (int i = static_cast<int>(nodePath.size()) - 1; i >= 0; --i)
            {
                osg::Node* node = nodePath[i];
                // Skip the scene root, overlay group, and highlight nodes
                if (node && node != m_viewer->getSceneData() &&
                    node->getName() != "_highlight_overlay" &&
                    (node->getNodeMask() & ~HIGHLIGHT_NODE_MASK) != 0)
                {
                    selectNode(node);
                    emit nodeClicked(node);
                    return;
                }
            }
        }
    }

    // Clicked on empty space - clear selection
    clearSelection();
    emit nodeClicked(nullptr);
}

void OSGWidget::selectNode(osg::Node* node)
{
    // Unhighlight previous
    if (m_selectedNode.valid())
    {
        unhighlightNode(m_selectedNode.get());
    }

    m_selectedNode = node;

    if (node)
    {
        highlightNode(node);
    }
}

void OSGWidget::clearSelection()
{
    if (m_selectedNode.valid())
    {
        unhighlightNode(m_selectedNode.get());
    }
    m_selectedNode = nullptr;
}

void OSGWidget::highlightNode(osg::Node* node)
{
    if (!node || !m_overlayGroup) return;

    // Compute bounding box in the node's OWN local coordinate system.
    // For Group/Transform nodes: traverse children directly (skip the node's
    // own transform so the BB is in local space, not parent space).
    // For leaf nodes (Geode etc.): accept the node directly.
    osg::ComputeBoundsVisitor cbv;
    cbv.setTraversalMask(~HIGHLIGHT_NODE_MASK);

    osg::Group* group = node->asGroup();
    if (group)
    {
        for (unsigned int i = 0; i < group->getNumChildren(); ++i)
        {
            osg::Node* child = group->getChild(i);
            if ((child->getNodeMask() & ~HIGHLIGHT_NODE_MASK) != 0)
                child->accept(cbv);
        }
    }
    else
    {
        node->accept(cbv);
    }

    osg::BoundingBox localBB = cbv.getBoundingBox();
    if (!localBB.valid()) return;

    // Build the world matrix from the node's local space to the scene root
    // (explicitly exclude the camera's view matrix by stopping at scene root).
    osg::Matrix worldMat;
    worldMat.makeIdentity();
    {
        osg::Node* root = m_viewer->getSceneData();
        std::vector<osg::Transform*> transforms;
        for (osg::Node* cur = node; cur && cur != root; )
        {
            osg::Transform* xform = dynamic_cast<osg::Transform*>(cur);
            if (xform)
                transforms.push_back(xform);
            cur = (cur->getNumParents() > 0) ? cur->getParent(0) : nullptr;
        }
        // Apply root-to-node order (reverse of the collected path)
        for (auto it = transforms.rbegin(); it != transforms.rend(); ++it)
            (*it)->computeLocalToWorldMatrix(worldMat, nullptr);
    }

    // Transform local bounds to world space (overlay group coordinates)
    osg::BoundingBox worldBB;
    for (unsigned int i = 0; i < 8; ++i)
        worldBB.expandBy(localBB.corner(i) * worldMat);

    m_highlightGeode = createBoundingBoxGeode(worldBB);
    m_overlayGroup->addChild(m_highlightGeode);
}

void OSGWidget::unhighlightNode(osg::Node* node)
{
    if (!m_highlightGeode || !m_overlayGroup) return;

    // Remove from the overlay group (safe: overlay group is persistent and never null)
    m_overlayGroup->removeChild(m_highlightGeode);
    m_highlightGeode = nullptr;
}

osg::Geode* OSGWidget::createBoundingBoxGeode(const osg::BoundingBox& bb)
{
    osg::Geode* geode = new osg::Geode;
    geode->setNodeMask(HIGHLIGHT_NODE_MASK);

    float xmin = bb.xMin(), xmax = bb.xMax();
    float ymin = bb.yMin(), ymax = bb.yMax();
    float zmin = bb.zMin(), zmax = bb.zMax();

    // 12 edges of a box (24 vertices for GL_LINES)
    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
    // Bottom face
    verts->push_back(osg::Vec3(xmin,ymin,zmin)); verts->push_back(osg::Vec3(xmax,ymin,zmin));
    verts->push_back(osg::Vec3(xmax,ymin,zmin)); verts->push_back(osg::Vec3(xmax,ymax,zmin));
    verts->push_back(osg::Vec3(xmax,ymax,zmin)); verts->push_back(osg::Vec3(xmin,ymax,zmin));
    verts->push_back(osg::Vec3(xmin,ymax,zmin)); verts->push_back(osg::Vec3(xmin,ymin,zmin));
    // Top face
    verts->push_back(osg::Vec3(xmin,ymin,zmax)); verts->push_back(osg::Vec3(xmax,ymin,zmax));
    verts->push_back(osg::Vec3(xmax,ymin,zmax)); verts->push_back(osg::Vec3(xmax,ymax,zmax));
    verts->push_back(osg::Vec3(xmax,ymax,zmax)); verts->push_back(osg::Vec3(xmin,ymax,zmax));
    verts->push_back(osg::Vec3(xmin,ymax,zmax)); verts->push_back(osg::Vec3(xmin,ymin,zmax));
    // Vertical edges
    verts->push_back(osg::Vec3(xmin,ymin,zmin)); verts->push_back(osg::Vec3(xmin,ymin,zmax));
    verts->push_back(osg::Vec3(xmax,ymin,zmin)); verts->push_back(osg::Vec3(xmax,ymin,zmax));
    verts->push_back(osg::Vec3(xmax,ymax,zmin)); verts->push_back(osg::Vec3(xmax,ymax,zmax));
    verts->push_back(osg::Vec3(xmin,ymax,zmin)); verts->push_back(osg::Vec3(xmin,ymax,zmax));

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
    colors->push_back(osg::Vec4(0.3f, 0.6f, 1.0f, 1.0f));  // Blue highlight color

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    geom->setVertexArray(verts);
    geom->setColorArray(colors, osg::Array::BIND_OVERALL);
    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 24));
    geom->setUseDisplayList(false);

    geode->addDrawable(geom);

    // StateSet: unlit blue lines, render on top
    osg::StateSet* ss = geode->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setAttributeAndModes(new osg::LineWidth(2.0f), osg::StateAttribute::ON);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
    osg::PolygonOffset* po = new osg::PolygonOffset(-1.0f, -1.0f);
    ss->setAttributeAndModes(po, osg::StateAttribute::ON);
    ss->setRenderBinDetails(11, "RenderBin");

    return geode;
}
