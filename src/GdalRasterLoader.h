#pragma once
#include <QString>
#include <QSet>
#include <osg/Image>
#include <osg/ref_ptr>

struct GdalRasterMeta {
    QString crs;
    QString dataType;
    int     bandCount  = 0;
    int     width      = 0;
    int     height     = 0;
    double  pixelSizeX = 0.0;
    double  pixelSizeY = 0.0;
};

class GdalRasterLoader {
public:
    static osg::ref_ptr<osg::Image> load(
        const QString&   filePath,
        GdalRasterMeta*  meta     = nullptr,
        QString*         errorMsg = nullptr);

    static QString supportedFormatsFilter();
    static bool    isRasterFile(const QString& ext);

private:
    static const QSet<QString>& rasterExtensions();
};
