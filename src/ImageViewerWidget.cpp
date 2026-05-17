#include "ImageViewerWidget.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <algorithm>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/BlendFunc>
#include <osgViewer/Viewer>

ImageViewerWidget::ImageViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    setFormat(fmt);
}

ImageViewerWidget::~ImageViewerWidget() {
    makeCurrent();
    m_viewer = nullptr;
    doneCurrent();
}

void ImageViewerWidget::setupViewer() {
    m_root = new osg::Group;

    const qreal dpr = devicePixelRatioF();
    const int fbw = std::max(1, static_cast<int>(width()  * dpr));
    const int fbh = std::max(1, static_cast<int>(height() * dpr));

    m_viewer = new osgViewer::Viewer;
    m_viewer->setUpViewerAsEmbeddedInWindow(0, 0, fbw, fbh);

    osg::Camera* cam = m_viewer->getCamera();
    cam->setClearColor(osg::Vec4(0.12f, 0.12f, 0.14f, 1.0f));
    cam->setProjectionMatrixAsOrtho2D(0, fbw, 0, fbh);
    cam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    cam->setViewMatrix(osg::Matrix::identity());

    m_viewer->setCameraManipulator(nullptr);
    m_viewer->setSceneData(m_root);
    m_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
    m_viewer->realize();
    m_initialized = true;
}

void ImageViewerWidget::initializeGL() {
    setupViewer();
    if (m_pendingImage.valid()) {
        loadImage(m_pendingImage.get(), m_pendingPath);
        m_pendingImage = nullptr;
        m_pendingPath.clear();
    }
}

void ImageViewerWidget::paintGL() {
    if (!m_viewer) return;

    GLint vp[4] = {0, 0, 1, 1};
    if (auto* ctx = QOpenGLContext::currentContext())
        if (auto* f = ctx->functions())
            f->glGetIntegerv(GL_VIEWPORT, vp);

    const int vw = std::max(1, static_cast<int>(vp[2]));
    const int vh = std::max(1, static_cast<int>(vp[3]));

    osg::Camera* cam = m_viewer->getCamera();
    if (cam) {
        auto* cur = cam->getViewport();
        if (!cur || cur->width() != vw || cur->height() != vh) {
            cam->setViewport(0, 0, vw, vh);
            if (auto* gw = dynamic_cast<osgViewer::GraphicsWindowEmbedded*>(cam->getGraphicsContext())) {
                gw->resized(0, 0, vw, vh);
                gw->getEventQueue()->windowResize(0, 0, vw, vh);
            }
        }
    }
    m_viewer->frame();
}

void ImageViewerWidget::resizeGL(int w, int h) {
    if (!m_viewer || !m_currentImage.valid()) return;
    const qreal dpr = devicePixelRatioF();
    const int fbw = std::max(1, static_cast<int>(w * dpr));
    const int fbh = std::max(1, static_cast<int>(h * dpr));

    osg::Camera* cam = m_viewer->getCamera();
    cam->setViewport(0, 0, fbw, fbh);
    if (auto* gw = dynamic_cast<osgViewer::GraphicsWindowEmbedded*>(cam->getGraphicsContext())) {
        gw->resized(0, 0, fbw, fbh);
        gw->getEventQueue()->windowResize(0, 0, fbw, fbh);
    }
    rebuildQuad();
    resetView();
    update();
}

// Build a textured quad centered in the viewport, scaled to fit with 5% margin
void ImageViewerWidget::rebuildQuad() {
    if (!m_currentImage.valid() || !m_root) return;

    m_root->removeChildren(0, m_root->getNumChildren());

    const qreal dpr = devicePixelRatioF();
    const int vpw = std::max(1, static_cast<int>(width()  * dpr));
    const int vph = std::max(1, static_cast<int>(height() * dpr));

    const float iw = static_cast<float>(m_currentImage->s());
    const float ih = static_cast<float>(m_currentImage->t());
    const float scale = std::min(vpw / iw, vph / ih) * 0.95f;
    const float qw = iw * scale;
    const float qh = ih * scale;
    const float x0 = (vpw - qw) * 0.5f;
    const float y0 = (vph - qh) * 0.5f;

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

    auto* verts = new osg::Vec3Array(4);
    (*verts)[0] = {x0,      y0,      0};
    (*verts)[1] = {x0 + qw, y0,      0};
    (*verts)[2] = {x0 + qw, y0 + qh, 0};
    (*verts)[3] = {x0,      y0 + qh, 0};
    geom->setVertexArray(verts);

    auto* uvs = new osg::Vec2Array(4);
    (*uvs)[0] = {0, 0};
    (*uvs)[1] = {1, 0};
    (*uvs)[2] = {1, 1};
    (*uvs)[3] = {0, 1};
    geom->setTexCoordArray(0, uvs, osg::Array::BIND_PER_VERTEX);

    auto* colors = new osg::Vec4Array(1);
    (*colors)[0] = {1, 1, 1, 1};
    geom->setColorArray(colors, osg::Array::BIND_OVERALL);

    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

    osg::ref_ptr<osg::Texture2D> tex = new osg::Texture2D(m_currentImage.get());
    tex->setResizeNonPowerOfTwoHint(false);
    tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
    tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);

    osg::ref_ptr<osg::StateSet> ss = geom->getOrCreateStateSet();
    ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    ss->setAttributeAndModes(new osg::BlendFunc, osg::StateAttribute::ON);
    ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(geom);
    m_root->addChild(geode);
}

void ImageViewerWidget::resetView() {
    if (!m_viewer) return;
    const qreal dpr = devicePixelRatioF();
    const int vpw = std::max(1, static_cast<int>(width()  * dpr));
    const int vph = std::max(1, static_cast<int>(height() * dpr));
    m_viewer->getCamera()->setProjectionMatrixAsOrtho2D(0, vpw, 0, vph);
    m_viewer->getCamera()->setViewMatrix(osg::Matrix::identity());
}

void ImageViewerWidget::loadImage(osg::Image* image, const QString& filePath) {
    if (!image) return;

    if (!m_initialized) {
        m_pendingImage = image;
        m_pendingPath  = filePath;
        return;
    }

    makeCurrent();
    m_currentImage = image;
    m_imgWidth  = image->s();
    m_imgHeight = image->t();
    rebuildQuad();
    resetView();
    doneCurrent();
    update();
}

void ImageViewerWidget::clearImage() {
    m_currentImage = nullptr;
    m_imgWidth = m_imgHeight = 0;
    if (m_root) m_root->removeChildren(0, m_root->getNumChildren());
    update();
}

void ImageViewerWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton) {
        m_lastMouseX = e->position().x();
        m_lastMouseY = e->position().y();
        m_dragging   = true;
    }
    update();
}

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent* e) {
    Q_UNUSED(e)
    m_dragging = false;
    update();
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!m_dragging || !m_viewer) return;

    const qreal dpr = devicePixelRatioF();
    const double dx = (e->position().x() - m_lastMouseX) * dpr;
    const double dy = (e->position().y() - m_lastMouseY) * dpr;
    m_lastMouseX = e->position().x();
    m_lastMouseY = e->position().y();

    osg::Camera* cam = m_viewer->getCamera();
    osg::Matrix view = cam->getViewMatrix();
    view = view * osg::Matrix::translate(dx, -dy, 0);
    cam->setViewMatrix(view);
    update();
}

void ImageViewerWidget::wheelEvent(QWheelEvent* e) {
    if (!m_viewer) return;

    const double factor = e->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    const qreal dpr = devicePixelRatioF();
    const double cx = e->position().x() * dpr;
    const double cy = (height() - e->position().y()) * dpr;

    osg::Camera* cam = m_viewer->getCamera();
    osg::Matrix view = cam->getViewMatrix();
    view = view
        * osg::Matrix::translate(-cx, -cy, 0)
        * osg::Matrix::scale(factor, factor, 1.0)
        * osg::Matrix::translate(cx, cy, 0);
    cam->setViewMatrix(view);
    update();
}

void ImageViewerWidget::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_F || e->key() == Qt::Key_Home) {
        rebuildQuad();
        resetView();
        update();
    }
}
