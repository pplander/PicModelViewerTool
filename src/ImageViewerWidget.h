#pragma once
#include <QOpenGLWidget>
#include <osg/ref_ptr>
#include <osg/Image>
#include <osg/Group>
#include <osgViewer/Viewer>

// 2D image viewer based on OSG + QOpenGLWidget.
// Renders both raster images (QImage-loaded) and GIS raster data (GDAL-loaded)
// as a textured quad with pan/zoom (orthographic projection).
class ImageViewerWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit ImageViewerWidget(QWidget* parent = nullptr);
    ~ImageViewerWidget() override;

    void loadImage(osg::Image* image, const QString& filePath = QString());
    void clearImage();

    int imageWidth()  const { return m_imgWidth; }
    int imageHeight() const { return m_imgHeight; }

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void setupViewer();
    void rebuildQuad();
    void resetView();

    osg::ref_ptr<osgViewer::Viewer> m_viewer;
    osg::ref_ptr<osg::Group>        m_root;
    osg::ref_ptr<osg::Image>        m_currentImage;
    bool m_initialized = false;
    int  m_imgWidth  = 0;
    int  m_imgHeight = 0;

    // Pending image queued before GL initialization completes
    osg::ref_ptr<osg::Image> m_pendingImage;
    QString m_pendingPath;

    // pan/zoom state
    double m_lastMouseX = 0, m_lastMouseY = 0;
    bool   m_dragging = false;
};
