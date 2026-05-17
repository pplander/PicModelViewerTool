#pragma once
#include <QWidget>
#include <QPointF>
#include "GdalVectorData.h"

// 2D vector data viewer based on QPainter.
// Supports pan/zoom, hover/select feature picking, and emits hover/select signals.
class VectorViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit VectorViewerWidget(QWidget* parent = nullptr);

    void loadData(const GdalVectorData* data);
    void clearData();

signals:
    void featureHovered(int index);
    void featureSelected(int index);
    void coordinateChanged(double x, double y);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void resetView();
    QPointF worldToScreen(double wx, double wy) const;
    QPointF screenToWorld(const QPointF& screenPos) const;
    QPolygonF worldRingToScreen(const QPolygonF& ring) const;
    int hitTest(const QPointF& screenPos) const;

    const GdalVectorData* m_data = nullptr;

    double  m_scale   = 1.0;
    double  m_offsetX = 0.0;
    double  m_offsetY = 0.0;

    bool    m_dragging = false;
    QPointF m_lastMouse;

    int m_hoveredIndex  = -1;
    int m_selectedIndex = -1;

    QColor  m_bgColor       { 0x1a, 0x1b, 0x1e };
    QColor  m_polygonFill   { 0x4d, 0xa6, 0xff, 55  };
    QColor  m_polygonStroke { 0x4d, 0xa6, 0xff, 200 };
    QColor  m_lineColor     { 0x5e, 0xc4, 0x6e, 220 };
    QColor  m_pointColor    { 0xff, 0xb8, 0x4d, 255 };
    QColor  m_hoverColor    { 0xff, 0xee, 0x58, 240 };
    QColor  m_selectColor   { 0xff, 0x70, 0x43, 255 };
};
