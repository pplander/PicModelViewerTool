#include "VectorViewerWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QLineF>
#include <algorithm>

VectorViewerWidget::VectorViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void VectorViewerWidget::loadData(const GdalVectorData* data) {
    m_data = data;
    m_hoveredIndex = -1;
    m_selectedIndex = -1;
    resetView();
    update();
}

void VectorViewerWidget::clearData() {
    m_data = nullptr;
    m_hoveredIndex = -1;
    m_selectedIndex = -1;
    update();
}

void VectorViewerWidget::resetView() {
    if (!m_data) return;
    const double worldW = m_data->maxX - m_data->minX;
    const double worldH = m_data->maxY - m_data->minY;
    if (worldW <= 0 || worldH <= 0) {
        m_scale = 1.0;
        m_offsetX = width() / 2.0;
        m_offsetY = height() / 2.0;
        return;
    }
    const double scaleX = width()  * 0.90 / worldW;
    const double scaleY = height() * 0.90 / worldH;
    m_scale = std::min(scaleX, scaleY);

    const double cx = m_data->minX + worldW * 0.5;
    const double cy = m_data->minY + worldH * 0.5;
    m_offsetX = width()  * 0.5 - cx * m_scale;
    m_offsetY = height() * 0.5 + cy * m_scale;
}

QPointF VectorViewerWidget::worldToScreen(double wx, double wy) const {
    return QPointF(wx * m_scale + m_offsetX,
                  -wy * m_scale + m_offsetY);
}

QPointF VectorViewerWidget::screenToWorld(const QPointF& screenPos) const {
    return QPointF((screenPos.x() - m_offsetX) / m_scale,
                  -(screenPos.y() - m_offsetY) / m_scale);
}

QPolygonF VectorViewerWidget::worldRingToScreen(const QPolygonF& ring) const {
    QPolygonF out;
    out.reserve(ring.size());
    for (const QPointF& p : ring)
        out << worldToScreen(p.x(), p.y());
    return out;
}

int VectorViewerWidget::hitTest(const QPointF& screenPos) const {
    if (!m_data) return -1;

    constexpr double pointPickPx = 7.0;
    constexpr double linePickPx = 5.0;

    for (int i = m_data->features.size() - 1; i >= 0; --i) {
        const GdalFeature& f = m_data->features[i];

        if (f.isPoint) {
            const QPointF sp = worldToScreen(f.point.x(), f.point.y());
            if (QLineF(sp, screenPos).length() <= pointPickPx)
                return i;
            continue;
        }

        if (f.isLine) {
            if (f.rings.isEmpty()) continue;
            const QPolygonF poly = worldRingToScreen(f.rings[0]);
            for (int k = 1; k < poly.size(); ++k) {
                const QPointF a = poly[k - 1];
                const QPointF b = poly[k];
                const double vx = b.x() - a.x();
                const double vy = b.y() - a.y();
                const double len2 = vx * vx + vy * vy;
                if (len2 < 1e-12) continue;

                const double wx = screenPos.x() - a.x();
                const double wy = screenPos.y() - a.y();
                double t = (wx * vx + wy * vy) / len2;
                t = std::max(0.0, std::min(1.0, t));

                const QPointF proj(a.x() + t * vx, a.y() + t * vy);
                if (QLineF(proj, screenPos).length() <= linePickPx)
                    return i;
            }
            continue;
        }

        if (f.rings.isEmpty()) continue;
        QPainterPath path;
        path.setFillRule(Qt::OddEvenFill);
        for (const QPolygonF& ring : f.rings) {
            if (ring.isEmpty()) continue;
            QPolygonF sr = worldRingToScreen(ring);
            path.addPolygon(sr);
            path.closeSubpath();
        }
        if (path.contains(screenPos))
            return i;
    }

    return -1;
}

void VectorViewerWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), m_bgColor);

    if (!m_data || m_data->features.isEmpty()) {
        painter.setPen(QColor(150, 150, 160));
        painter.drawText(rect(), Qt::AlignCenter,
                         m_data ? tr("No features to display") : tr("No data loaded"));
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < m_data->features.size(); ++i) {
        const GdalFeature& f = m_data->features[i];
        const bool isHovered = (i == m_hoveredIndex);
        const bool isSelected = (i == m_selectedIndex);

        QColor strokeColor = m_polygonStroke;
        QColor fillColor = m_polygonFill;
        QColor lineColor = m_lineColor;
        QColor pointColor = m_pointColor;
        double lineWidth = 1.2;

        if (isHovered) {
            strokeColor = m_hoverColor;
            lineColor = m_hoverColor;
            pointColor = m_hoverColor;
            fillColor = QColor(m_hoverColor.red(), m_hoverColor.green(), m_hoverColor.blue(), 70);
            lineWidth = 2.0;
        }
        if (isSelected) {
            strokeColor = m_selectColor;
            lineColor = m_selectColor;
            pointColor = m_selectColor;
            fillColor = QColor(m_selectColor.red(), m_selectColor.green(), m_selectColor.blue(), 90);
            lineWidth = 2.4;
        }

        if (f.isPoint) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(pointColor);
            const QPointF sp = worldToScreen(f.point.x(), f.point.y());
            const double radius = isSelected ? 6.0 : (isHovered ? 5.0 : 3.5);
            painter.drawEllipse(sp, radius, radius);

        } else if (f.isLine) {
            if (f.rings.isEmpty()) continue;
            painter.setPen(QPen(lineColor, lineWidth));
            painter.setBrush(Qt::NoBrush);
            painter.drawPolyline(worldRingToScreen(f.rings[0]));

        } else {
            if (f.rings.isEmpty()) continue;
            QPainterPath path;
            path.setFillRule(Qt::OddEvenFill);
            for (const QPolygonF& ring : f.rings) {
                QPolygonF sr = worldRingToScreen(ring);
                path.addPolygon(sr);
                path.closeSubpath();
            }
            painter.setPen(QPen(strokeColor, lineWidth));
            painter.setBrush(fillColor);
            painter.drawPath(path);
        }
    }
}

void VectorViewerWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton) {
        m_lastMouse = e->position();
        m_dragging  = true;
    }
}

void VectorViewerWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (!m_data) {
        m_dragging = false;
        return;
    }

    if ((e->button() == Qt::LeftButton) && m_dragging) {
        const QPointF delta = e->position() - m_lastMouse;
        if (QLineF(QPointF(0, 0), delta).length() < 4.0) {
            const int picked = hitTest(e->position());
            if (picked != m_selectedIndex) {
                m_selectedIndex = picked;
                emit featureSelected(m_selectedIndex);
                update();
            }
        }
    }

    m_dragging = false;
}

void VectorViewerWidget::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging) {
        const QPointF delta = e->position() - m_lastMouse;
        m_lastMouse = e->position();
        m_offsetX += delta.x();
        m_offsetY += delta.y();
        update();
    }

    const QPointF world = screenToWorld(e->position());
    emit coordinateChanged(world.x(), world.y());

    if (!m_dragging) {
        const int hovered = hitTest(e->position());
        if (hovered != m_hoveredIndex) {
            m_hoveredIndex = hovered;
            emit featureHovered(m_hoveredIndex);
            update();
        }
    }
}

void VectorViewerWidget::wheelEvent(QWheelEvent* e) {
    const double factor = e->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    const double cx = e->position().x();
    const double cy = e->position().y();
    m_offsetX = cx + (m_offsetX - cx) * factor;
    m_offsetY = cy + (m_offsetY - cy) * factor;
    m_scale  *= factor;
    update();
}

void VectorViewerWidget::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_F || e->key() == Qt::Key_Home) {
        resetView();
        update();
    }
}

void VectorViewerWidget::resizeEvent(QResizeEvent*) {
    if (m_data) {
        resetView();
        update();
    }
}
