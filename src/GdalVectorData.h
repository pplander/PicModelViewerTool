#pragma once
#include <QString>
#include <QVector>
#include <QPointF>
#include <QPolygonF>
#include <QPair>

struct GdalFeature {
    QVector<QPolygonF> rings;  // polygon rings OR a single line in rings[0]
    bool    isPoint = false;
    QPointF point;
    bool    isLine  = false;

    qlonglong fid = -1;
    QString   layerName;
    QString   geometryType;
    QVector<QPair<QString, QString>> attributes;
};

struct GdalVectorData {
    QString filePath;
    QString format;        // e.g. "ESRI Shapefile", "GeoJSON"
    QString crs;           // WKT string
    QString geomType;      // "Point", "LineString", "Polygon", etc.
    int     featureCount = 0;       // source feature count (dataset reported)
    int     displayedCount = 0;     // flattened features actually prepared for drawing
    bool    truncated = false;      // true when display data is capped
    int     layerCount   = 0;

    double  minX = 0, minY = 0, maxX = 0, maxY = 0;

    QVector<GdalFeature> features;

    bool isValid() const { return featureCount >= 0; }
};
