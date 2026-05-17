#include <QApplication>
#include <QSurfaceFormat>
#include <QSettings>

#include "MainWindow.h"
#include "I18nManager.h"

int main(int argc, char* argv[])
{
    // Set OpenGL format before creating QApplication
    QSurfaceFormat format;
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setSamples(4);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    // Application info
    QApplication::setApplicationName("PicModelViewer");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("PicModelViewer");

    // Set OSG environment variable for plugin path
    // OSG_LIBRARY_PATH should point to the directory containing osgPlugins-3.6.5/
    QString appDir = QCoreApplication::applicationDirPath();
    qputenv("OSG_LIBRARY_PATH", appDir.toUtf8().constData());

    // GDAL/PROJ data directories (deployed by CMake POST_BUILD)
    qputenv("GDAL_DATA", (appDir + "/gdal-data").toUtf8().constData());
    qputenv("PROJ_LIB",  (appDir + "/proj-data").toUtf8().constData());

    // Initialize i18n with system language (default Chinese)
    I18nManager::Language lang = I18nManager::systemLanguage();
    I18nManager::instance().setLanguage(lang);

    MainWindow window;
    window.show();

    // If a file was passed as argument, open it
    if (argc > 1)
    {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        window.loadModel(filePath);
    }

    return app.exec();
}
