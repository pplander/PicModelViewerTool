#include "OSGWidget.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

#include <osg/DisplaySettings>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PolygonMode>
#include <osg/PolygonOffset>
#include <osg/Material>
#include <osgGA/TrackballManipulator>
#include <osgUtil/Optimizer>
#include <osgUtil/IntersectionVisitor>
#include <osg/ComputeBoundsVisitor>

#include <OpenThreads/Thread>

OSGWidget::OSGWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 200);

    // Ensure widget expands to fill available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Create highlight material
    m_highlightMaterial = new osg::Material;
    m_highlightMaterial->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.3f, 0.6f, 1.0f, 1.0f));
    m_highlightMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.3f, 0.6f, 1.0f, 1.0f));
    m_highlightMaterial->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.5f, 0.7f, 1.0f, 1.0f));
    m_highlightMaterial->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.1f, 0.2f, 0.4f, 1.0f));

    initViewer();

    // Render timer ~60 FPS
    connect(&m_timer, &QTimer::timeout, this, QOverload<>::of(&OSGWidget::update));
    m_timer.start(16);
}

OSGWidget::~OSGWidget()
{
    m_timer.stop();
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

    if (node)
    {
        osgUtil::Optimizer optimizer;
        optimizer.optimize(node, osgUtil::Optimizer::DEFAULT_OPTIMIZATIONS);
    }

    m_viewer->setSceneData(node);
    if (node)
    {
        fitToView();
    }
    updateDisplayMode();
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
        osg::ref_ptr<osg::PolygonMode> pm = new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL);
        rootSS->setAttributeAndModes(pm.get());
        // Add polygon offset for wireframe overlay to avoid z-fighting
        osg::ref_ptr<osg::PolygonOffset> po = new osg::PolygonOffset(1.0f, 1.0f);
        rootSS->setAttributeAndModes(po.get());
        rootSS->setMode(GL_POLYGON_OFFSET_FILL, osg::StateAttribute::ON);
        // Set rendering bin to draw wireframe on top
        rootSS->setRenderBinDetails(1, "RenderBin");
        break;
    }
    }
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
    iv.setTraversalMask(~0);

    // Must apply on camera - the visitor traverses from camera through scene
    const_cast<osg::Camera*>(m_viewer->getCamera())->accept(iv);

    if (intersector->containsIntersections())
    {
        osgUtil::LineSegmentIntersector::Intersections& intersections = intersector->getIntersections();
        for (auto& hit : intersections)
        {
            const osg::NodePath& nodePath = hit.nodePath;
            // Walk from leaf to root, skip the root scene node
            for (int i = static_cast<int>(nodePath.size()) - 1; i >= 0; --i)
            {
                osg::Node* node = nodePath[i];
                if (node && node != m_viewer->getSceneData())
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
    m_savedStateSet = nullptr;
}

void OSGWidget::highlightNode(osg::Node* node)
{
    if (!node) return;

    // Save original state set
    m_savedStateSet = node->getStateSet();

    // Create a new state set that inherits from the original
    osg::StateSet* highlightSS = new osg::StateSet;
    if (m_savedStateSet.valid())
    {
        highlightSS->merge(*m_savedStateSet);
    }
    highlightSS->setAttributeAndModes(m_highlightMaterial.get(), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    highlightSS->setMode(GL_LIGHTING, osg::StateAttribute::ON);

    node->setStateSet(highlightSS);
}

void OSGWidget::unhighlightNode(osg::Node* node)
{
    if (!node) return;

    // Restore original state set
    if (m_savedStateSet.valid())
    {
        node->setStateSet(m_savedStateSet.get());
    }
    else
    {
        node->setStateSet(nullptr);
    }
    m_savedStateSet = nullptr;
}
