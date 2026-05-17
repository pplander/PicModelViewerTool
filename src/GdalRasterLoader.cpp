#include "GdalRasterLoader.h"
#include "GdalInit.h"
#include <gdal_priv.h>
#include <osg/Image>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <vector>
#include <cmath>

static const QSet<QString> RASTER_EXTS = {
    "tif","tiff","img","vrt","nc","hdf","h4","h5",
    "adf","asc","dem","dt0","dt1","dt2","grd",
    "bil","bsq","bip","ecw","jp2","sid","ers",
    "pix","rst","sdat","aig","ntf","nitf"
};

const QSet<QString>& GdalRasterLoader::rasterExtensions() {
    return RASTER_EXTS;
}

bool GdalRasterLoader::isRasterFile(const QString& ext) {
    return RASTER_EXTS.contains(ext.toLower());
}

QString GdalRasterLoader::supportedFormatsFilter() {
    QStringList patterns;
    QStringList sorted = RASTER_EXTS.values();
    sorted.sort();
    for (const QString& e : sorted)
        patterns << "*." + e;
    return QString("Raster (%1);;All Files (*)").arg(patterns.join(' '));
}

osg::ref_ptr<osg::Image> GdalRasterLoader::load(
    const QString&  filePath,
    GdalRasterMeta* meta,
    QString*        errorMsg)
{
    ensureGdalRegistered();

    GDALDataset* ds = static_cast<GDALDataset*>(
        GDALOpen(filePath.toUtf8().constData(), GA_ReadOnly));
    if (!ds) {
        if (errorMsg)
            *errorMsg = QString("GDAL failed to open: %1").arg(filePath);
        return nullptr;
    }

    const int fullW = ds->GetRasterXSize();
    const int fullH = ds->GetRasterYSize();
    const int bands = ds->GetRasterCount();

    if (fullW <= 0 || fullH <= 0 || bands <= 0) {
        if (errorMsg) *errorMsg = "Invalid raster dimensions or no bands.";
        GDALClose(ds);
        return nullptr;
    }

    // Downsample very large rasters to max 4096 on the longer axis
    const int MAX_DIM = 4096;
    int outW = fullW, outH = fullH;
    if (fullW > MAX_DIM || fullH > MAX_DIM) {
        const double scale = static_cast<double>(MAX_DIM) / std::max(fullW, fullH);
        outW = std::max(1, static_cast<int>(fullW * scale));
        outH = std::max(1, static_cast<int>(fullH * scale));
    }

    // Fill metadata
    if (meta) {
        meta->width     = fullW;
        meta->height    = fullH;
        meta->bandCount = bands;

        GDALRasterBand* b1 = ds->GetRasterBand(1);
        meta->dataType = GDALGetDataTypeName(b1->GetRasterDataType());

        double gt[6] = {0,1,0,0,0,1};
        if (ds->GetGeoTransform(gt) == CE_None) {
            meta->pixelSizeX = std::abs(gt[1]);
            meta->pixelSizeY = std::abs(gt[5]);
        }

        const char* wkt = ds->GetProjectionRef();
        meta->crs = (wkt && *wkt) ? QString::fromUtf8(wkt) : QString("Unknown");
    }

    // Determine band mapping → RGBA
    const int useBands = std::min(bands, 3);

    const int npix = outW * outH;
    std::vector<std::vector<float>> bandData(useBands, std::vector<float>(npix));

    for (int bi = 0; bi < useBands; ++bi) {
        GDALRasterBand* band = ds->GetRasterBand(bi + 1);

        double bMin = 0, bMax = 255, bMean = 0, bStd = 0;
        CPLErr statErr = band->GetStatistics(FALSE, TRUE, &bMin, &bMax, &bMean, &bStd);
        if (statErr != CE_None || bMin == bMax) {
            CPLErr compErr = band->ComputeStatistics(FALSE, &bMin, &bMax, nullptr, nullptr, nullptr, nullptr);
            if (compErr != CE_None || bMin == bMax) {
                bMin = 0.0;
                bMax = 255.0;
            }
        }
        if (bMin == bMax) bMax = bMin + 1.0;

        band->RasterIO(GF_Read,
            0, 0, fullW, fullH,
            bandData[bi].data(), outW, outH,
            GDT_Float32, 0, 0);

        const double scale = 255.0 / (bMax - bMin);
        for (float& v : bandData[bi])
            v = static_cast<float>(std::max(0.0, std::min(255.0, (v - bMin) * scale)));
    }

    GDALClose(ds);

    // Build osg::Image (RGBA, bottom-up row order)
    osg::ref_ptr<osg::Image> img = new osg::Image;
    img->allocateImage(outW, outH, 1, GL_RGBA, GL_UNSIGNED_BYTE);
    unsigned char* data = img->data();

    for (int row = 0; row < outH; ++row) {
        // OSG row 0 = bottom, GDAL row 0 = top → flip Y
        const int gdalRow = outH - 1 - row;
        for (int col = 0; col < outW; ++col) {
            const int gdalIdx = gdalRow * outW + col;
            const int osgIdx  = (row * outW + col) * 4;

            if (useBands >= 3) {
                data[osgIdx + 0] = static_cast<unsigned char>(bandData[0][gdalIdx]);
                data[osgIdx + 1] = static_cast<unsigned char>(bandData[1][gdalIdx]);
                data[osgIdx + 2] = static_cast<unsigned char>(bandData[2][gdalIdx]);
                data[osgIdx + 3] = 255;
            } else if (useBands == 2) {
                const unsigned char gray = static_cast<unsigned char>(bandData[0][gdalIdx]);
                data[osgIdx + 0] = gray;
                data[osgIdx + 1] = gray;
                data[osgIdx + 2] = gray;
                data[osgIdx + 3] = static_cast<unsigned char>(bandData[1][gdalIdx]);
            } else {
                const unsigned char gray = static_cast<unsigned char>(bandData[0][gdalIdx]);
                data[osgIdx + 0] = gray;
                data[osgIdx + 1] = gray;
                data[osgIdx + 2] = gray;
                data[osgIdx + 3] = 255;
            }
        }
    }

    return img;
}
