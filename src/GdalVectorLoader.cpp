#include "GdalVectorLoader.h"
#include "GdalInit.h"
#include <ogrsf_frmts.h>
#include <QSet>
#include <algorithm>

static const QSet<QString> VECTOR_EXTS = {
    "shp","geojson","json","kml","kmz","gpx","gml",
    "gpkg","sqlite","db","tab","mif","mid"
};

const QSet<QString>& GdalVectorLoader::vectorExtensions() {
    return VECTOR_EXTS;
}

bool GdalVectorLoader::isVectorFile(const QString& ext) {
    return VECTOR_EXTS.contains(ext.toLower());
}

QString GdalVectorLoader::supportedFormatsFilter() {
    QStringList patterns;
    QStringList sorted = VECTOR_EXTS.values();
    sorted.sort();
    for (const QString& e : sorted)
        patterns << "*." + e;
    return QString("Vector (%1);;All Files (*)").arg(patterns.join(' '));
}

static QVector<QPair<QString, QString>> extractAttributes(OGRFeature* feat) {
    QVector<QPair<QString, QString>> attrs;
    if (!feat) return attrs;

    OGRFeatureDefn* defn = feat->GetDefnRef();
    if (!defn) return attrs;

    const int count = defn->GetFieldCount();
    attrs.reserve(count);
    for (int i = 0; i < count; ++i) {
        OGRFieldDefn* fd = defn->GetFieldDefn(i);
        if (!fd) continue;

        const QString key = QString::fromUtf8(fd->GetNameRef());
        QString value;
        if (feat->IsFieldSetAndNotNull(i)) {
            value = QString::fromUtf8(feat->GetFieldAsString(i));
        } else {
            value = QStringLiteral("<null>");
        }
        attrs.append({key, value});
    }
    return attrs;
}

// Flatten a single OGRGeometry into GdalFeature entries
static void flattenGeometry(
    OGRGeometry* geom,
    QVector<GdalFeature>& out,
    int maxFeatures,
    bool& truncated,
    const qlonglong fid,
    const QString& layerName,
    const QVector<QPair<QString, QString>>& attributes)
{
    if (!geom || out.size() >= maxFeatures) {
        if (geom && out.size() >= maxFeatures) truncated = true;
        return;
    }

    const OGRwkbGeometryType type = wkbFlatten(geom->getGeometryType());

    auto makeBaseFeature = [&]() {
        GdalFeature f;
        f.fid = fid;
        f.layerName = layerName;
        f.geometryType = QString::fromUtf8(OGRGeometryTypeToName(type));
        f.attributes = attributes;
        return f;
    };

    switch (type) {
    case wkbPoint: {
        OGRPoint* pt = static_cast<OGRPoint*>(geom);
        GdalFeature f = makeBaseFeature();
        f.isPoint = true;
        f.point   = QPointF(pt->getX(), pt->getY());
        out.append(f);
        break;
    }
    case wkbMultiPoint: {
        OGRMultiPoint* mp = static_cast<OGRMultiPoint*>(geom);
        for (int i = 0; i < mp->getNumGeometries(); ++i) {
            flattenGeometry(mp->getGeometryRef(i), out, maxFeatures, truncated, fid, layerName, attributes);
            if (out.size() >= maxFeatures) {
                truncated = true;
                break;
            }
        }
        break;
    }
    case wkbLineString: {
        OGRLineString* ls = static_cast<OGRLineString*>(geom);
        GdalFeature f = makeBaseFeature();
        f.isLine = true;
        QPolygonF ring;
        ring.reserve(ls->getNumPoints());
        for (int i = 0; i < ls->getNumPoints(); ++i)
            ring << QPointF(ls->getX(i), ls->getY(i));
        f.rings.append(ring);
        out.append(f);
        break;
    }
    case wkbMultiLineString: {
        OGRMultiLineString* mls = static_cast<OGRMultiLineString*>(geom);
        for (int i = 0; i < mls->getNumGeometries(); ++i) {
            flattenGeometry(mls->getGeometryRef(i), out, maxFeatures, truncated, fid, layerName, attributes);
            if (out.size() >= maxFeatures) {
                truncated = true;
                break;
            }
        }
        break;
    }
    case wkbPolygon: {
        OGRPolygon* poly = static_cast<OGRPolygon*>(geom);
        OGRLinearRing* outer = poly->getExteriorRing();
        if (!outer) break;

        GdalFeature f = makeBaseFeature();

        QPolygonF outerRing;
        outerRing.reserve(outer->getNumPoints());
        for (int i = 0; i < outer->getNumPoints(); ++i)
            outerRing << QPointF(outer->getX(i), outer->getY(i));
        f.rings.append(outerRing);

        for (int h = 0; h < poly->getNumInteriorRings(); ++h) {
            OGRLinearRing* hole = poly->getInteriorRing(h);
            QPolygonF holeRing;
            holeRing.reserve(hole->getNumPoints());
            for (int i = 0; i < hole->getNumPoints(); ++i)
                holeRing << QPointF(hole->getX(i), hole->getY(i));
            f.rings.append(holeRing);
        }

        out.append(f);
        break;
    }
    case wkbMultiPolygon: {
        OGRMultiPolygon* mp = static_cast<OGRMultiPolygon*>(geom);
        for (int i = 0; i < mp->getNumGeometries(); ++i) {
            flattenGeometry(mp->getGeometryRef(i), out, maxFeatures, truncated, fid, layerName, attributes);
            if (out.size() >= maxFeatures) {
                truncated = true;
                break;
            }
        }
        break;
    }
    case wkbGeometryCollection: {
        OGRGeometryCollection* gc = static_cast<OGRGeometryCollection*>(geom);
        for (int i = 0; i < gc->getNumGeometries(); ++i) {
            flattenGeometry(gc->getGeometryRef(i), out, maxFeatures, truncated, fid, layerName, attributes);
            if (out.size() >= maxFeatures) {
                truncated = true;
                break;
            }
        }
        break;
    }
    default:
        break;
    }
}

std::unique_ptr<GdalVectorData> GdalVectorLoader::load(
    const QString& filePath,
    QString*       errorMsg)
{
    ensureGdalRegistered();

    GDALDataset* ds = static_cast<GDALDataset*>(
        GDALOpenEx(filePath.toUtf8().constData(),
                   GDAL_OF_VECTOR | GDAL_OF_READONLY,
                   nullptr, nullptr, nullptr));
    if (!ds) {
        if (errorMsg)
            *errorMsg = QString("GDAL failed to open vector file: %1").arg(filePath);
        return nullptr;
    }

    auto data = std::make_unique<GdalVectorData>();
    data->filePath   = filePath;
    data->format     = QString::fromUtf8(ds->GetDriverName());
    data->layerCount = ds->GetLayerCount();

    bool bboxInit = false;
    double minX = 0, minY = 0, maxX = 0, maxY = 0;

    constexpr int MAX_FEATURES = 50000;
    data->truncated = false;

    for (int li = 0; li < ds->GetLayerCount(); ++li) {
        OGRLayer* layer = ds->GetLayer(li);
        if (!layer) continue;

        const QString layerName = QString::fromUtf8(layer->GetName());

        if (data->crs.isEmpty()) {
            OGRSpatialReference* srs = layer->GetSpatialRef();
            if (srs) {
                char* wkt = nullptr;
                srs->exportToWkt(&wkt);
                if (wkt) {
                    data->crs = QString::fromUtf8(wkt);
                    CPLFree(wkt);
                }
            }
        }

        if (data->geomType.isEmpty()) {
            const OGRwkbGeometryType gt = wkbFlatten(layer->GetGeomType());
            data->geomType = QString::fromUtf8(OGRGeometryTypeToName(gt));
        }

        const GIntBig cnt = layer->GetFeatureCount(TRUE);
        if (cnt > 0) data->featureCount += static_cast<int>(cnt);

        OGREnvelope env;
        if (layer->GetExtent(&env, TRUE) == OGRERR_NONE) {
            if (!bboxInit) {
                minX = env.MinX; minY = env.MinY;
                maxX = env.MaxX; maxY = env.MaxY;
                bboxInit = true;
            } else {
                minX = std::min(minX, env.MinX);
                minY = std::min(minY, env.MinY);
                maxX = std::max(maxX, env.MaxX);
                maxY = std::max(maxY, env.MaxY);
            }
        }

        layer->ResetReading();
        OGRFeature* feat = nullptr;
        while ((feat = layer->GetNextFeature()) != nullptr) {
            OGRGeometry* geom = feat->GetGeometryRef();
            if (geom) {
                const qlonglong fid = static_cast<qlonglong>(feat->GetFID());
                const auto attrs = extractAttributes(feat);
                flattenGeometry(geom, data->features, MAX_FEATURES, data->truncated, fid, layerName, attrs);

                OGREnvelope fenv;
                geom->getEnvelope(&fenv);
                if (!bboxInit) {
                    minX = fenv.MinX; minY = fenv.MinY;
                    maxX = fenv.MaxX; maxY = fenv.MaxY;
                    bboxInit = true;
                } else {
                    minX = std::min(minX, fenv.MinX);
                    minY = std::min(minY, fenv.MinY);
                    maxX = std::max(maxX, fenv.MaxX);
                    maxY = std::max(maxY, fenv.MaxY);
                }
            }

            OGRFeature::DestroyFeature(feat);
            if (data->features.size() >= MAX_FEATURES) {
                data->truncated = true;
                break;
            }
        }

        if (data->features.size() >= MAX_FEATURES) {
            data->truncated = true;
            break;
        }
    }

    data->displayedCount = data->features.size();
    if (bboxInit) {
        data->minX = minX; data->minY = minY;
        data->maxX = maxX; data->maxY = maxY;
    }

    GDALClose(ds);
    return data;
}
