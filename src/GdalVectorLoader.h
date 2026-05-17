#pragma once
#include <QString>
#include <QSet>
#include <memory>
#include "GdalVectorData.h"

class GdalVectorLoader {
public:
    static std::unique_ptr<GdalVectorData> load(
        const QString& filePath,
        QString*       errorMsg = nullptr);

    static QString supportedFormatsFilter();
    static bool    isVectorFile(const QString& ext);

private:
    static const QSet<QString>& vectorExtensions();
};
