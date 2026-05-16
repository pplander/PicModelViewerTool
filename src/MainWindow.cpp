#include "MainWindow.h"
#include "OSGWidget.h"
#include "ModelLoader.h"
#include "ModelInfoDock.h"
#include "SceneTreeDock.h"
#include "LightControlDock.h"
#include "I18nManager.h"
#include "WelcomeWidget.h"
#include "ModelConverter.h"
#include "BatchConvertDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QScreen>
#include <QImage>
#include <QSettings>
#include <QApplication>
#include <QOpenGLWidget>
#include <QMenu>
#include <QFile>
#include <QFrame>
#include <QDir>
#include <QDirIterator>

#include <osgDB/WriteFile>
#include <osg/Node>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();

    // Load saved theme
    QSettings settings("PicModelViewer", "PicModelViewerTool");
    m_recentFiles = settings.value("RecentFiles").toStringList();
    Theme savedTheme = static_cast<Theme>(settings.value("Theme", 0).toInt());
    setTheme(savedTheme);

    // Set initial language
    I18nManager::Language lang = I18nManager::systemLanguage();
    I18nManager::instance().setLanguage(lang);

    // Apply initial language to menu
    if (lang == I18nManager::Chinese)
        m_chineseAction->setChecked(true);
    else
        m_englishAction->setChecked(true);

    updateRecentFiles();
}

MainWindow::~MainWindow()
{
    // Save settings
    QSettings settings("PicModelViewer", "PicModelViewerTool");
    settings.setValue("RecentFiles", m_recentFiles);
    settings.setValue("Theme", static_cast<int>(m_currentTheme));
}

void MainWindow::setupUI()
{
    setWindowTitle("PicModelViewer");
    resize(1280, 800);

    // Center the window
    QScreen* screen = QApplication::primaryScreen();
    if (screen)
    {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }

    // Enable drag and drop
    setAcceptDrops(true);

    // Create stacked widget for welcome page + OSG view
    m_stackWidget = new QStackedWidget(this);
    setCentralWidget(m_stackWidget);

    // Create welcome page
    m_welcomeWidget = new WelcomeWidget(this);
    m_stackWidget->addWidget(m_welcomeWidget);

    // Create OSG widget
    m_osgWidget = new OSGWidget(this);
    m_stackWidget->addWidget(m_osgWidget);

    // Start on welcome page
    m_stackWidget->setCurrentIndex(0);

    // Create model loader
    m_modelLoader = new ModelLoader(this);

    // Create model converter
    m_modelConverter = new ModelConverter(this);

    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDockWidgets();
    setupConnections();
}

void MainWindow::setupMenuBar()
{
    // File menu
    m_fileMenu = menuBar()->addMenu(tr("&File"));

    m_openAction = new QAction(tr("&Open..."), this);
    m_openAction->setIcon(QIcon(":/icons/open.svg"));
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip(tr("Open a model file"));
    m_fileMenu->addAction(m_openAction);

    m_recentFilesMenu = m_fileMenu->addMenu(tr("&Recent Files"));
    m_recentFilesMenu->setIcon(QIcon(":/icons/recent.svg"));
    m_clearRecentAction = new QAction(tr("Clear Recent"), this);
    m_recentFilesMenu->addAction(m_clearRecentAction);

    m_fileMenu->addSeparator();

    m_prevFileAction = new QAction(tr("Previous File"), this);
    m_prevFileAction->setIcon(QIcon(":/icons/prev.svg"));
    m_prevFileAction->setShortcut(QKeySequence("Alt+Left"));
    m_prevFileAction->setStatusTip(tr("Open previous model file in directory"));
    m_prevFileAction->setEnabled(false);
    m_fileMenu->addAction(m_prevFileAction);

    m_nextFileAction = new QAction(tr("Next File"), this);
    m_nextFileAction->setIcon(QIcon(":/icons/next.svg"));
    m_nextFileAction->setShortcut(QKeySequence("Alt+Right"));
    m_nextFileAction->setStatusTip(tr("Open next model file in directory"));
    m_nextFileAction->setEnabled(false);
    m_fileMenu->addAction(m_nextFileAction);

    m_fileMenu->addSeparator();

    m_closeAction = new QAction(tr("&Close"), this);
    m_closeAction->setIcon(QIcon(":/icons/close.svg"));
    m_closeAction->setShortcut(QKeySequence::Close);
    m_closeAction->setStatusTip(tr("Close current model"));
    m_fileMenu->addAction(m_closeAction);

    m_fileMenu->addSeparator();

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setIcon(QIcon(":/icons/exit.svg"));
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip(tr("Exit application"));
    m_fileMenu->addAction(m_exitAction);

    // View menu
    m_viewMenu = menuBar()->addMenu(tr("&View"));

    m_resetViewAction = new QAction(tr("Reset View"), this);
    m_resetViewAction->setIcon(QIcon(":/icons/home.svg"));
    m_resetViewAction->setShortcut(QKeySequence("Home"));
    m_resetViewAction->setStatusTip(tr("Reset camera to default position"));
    m_viewMenu->addAction(m_resetViewAction);

    m_displayModeMenu = m_viewMenu->addMenu(tr("Display Mode"));
    m_displayModeGroup = new QActionGroup(this);
    m_displayModeGroup->setExclusive(true);

    m_solidAction = new QAction(tr("Solid"), m_displayModeGroup);
    m_solidAction->setIcon(QIcon(":/icons/solid.svg"));
    m_solidAction->setCheckable(true);
    m_solidAction->setChecked(true);
    m_solidAction->setShortcut(QKeySequence("1"));
    m_displayModeMenu->addAction(m_solidAction);

    m_wireframeAction = new QAction(tr("Wireframe"), m_displayModeGroup);
    m_wireframeAction->setIcon(QIcon(":/icons/wireframe.svg"));
    m_wireframeAction->setCheckable(true);
    m_wireframeAction->setShortcut(QKeySequence("2"));
    m_displayModeMenu->addAction(m_wireframeAction);

    m_pointsAction = new QAction(tr("Points"), m_displayModeGroup);
    m_pointsAction->setIcon(QIcon(":/icons/points.svg"));
    m_pointsAction->setCheckable(true);
    m_pointsAction->setShortcut(QKeySequence("3"));
    m_displayModeMenu->addAction(m_pointsAction);

    m_solidWireframeAction = new QAction(tr("Solid + Wireframe"), m_displayModeGroup);
    m_solidWireframeAction->setIcon(QIcon(":/icons/solidwireframe.svg"));
    m_solidWireframeAction->setCheckable(true);
    m_solidWireframeAction->setShortcut(QKeySequence("4"));
    m_displayModeMenu->addAction(m_solidWireframeAction);

    m_viewMenu->addSeparator();

    m_fullScreenAction = new QAction(tr("Full Screen"), this);
    m_fullScreenAction->setIcon(QIcon(":/icons/fullscreen.svg"));
    m_fullScreenAction->setShortcut(QKeySequence::FullScreen);
    m_fullScreenAction->setCheckable(true);
    m_fullScreenAction->setStatusTip(tr("Toggle full screen mode"));
    m_viewMenu->addAction(m_fullScreenAction);

    // View -> Dock widgets
    m_viewMenu->addSeparator();

    // Tools menu
    m_toolsMenu = menuBar()->addMenu(tr("&Tools"));

    m_screenshotAction = new QAction(tr("Take Screenshot"), this);
    m_screenshotAction->setIcon(QIcon(":/icons/screenshot.svg"));
    m_screenshotAction->setShortcut(QKeySequence("Ctrl+P"));
    m_screenshotAction->setStatusTip(tr("Save screenshot to file"));
    m_toolsMenu->addAction(m_screenshotAction);

    m_batchConvertAction = new QAction(tr("Batch Convert..."), this);
    m_batchConvertAction->setStatusTip(tr("Convert models between different formats"));
    m_toolsMenu->addAction(m_batchConvertAction);

    // OSG Handler menu under Tools
    m_handlerMenu = m_toolsMenu->addMenu(tr("OSG &Handlers"));

    m_toggleStatsAction = new QAction(tr("Toggle Stats (S)"), this);
    m_toggleStatsAction->setShortcut(QKeySequence("S"));
    m_toggleStatsAction->setShortcutContext(Qt::WidgetShortcut);
    m_toggleStatsAction->setStatusTip(tr("Toggle on-screen statistics display"));
    m_handlerMenu->addAction(m_toggleStatsAction);
    m_osgWidget->addAction(m_toggleStatsAction);

    m_toggleHelpAction = new QAction(tr("Toggle Help (H)"), this);
    m_toggleHelpAction->setShortcut(QKeySequence("H"));
    m_toggleHelpAction->setShortcutContext(Qt::WidgetShortcut);
    m_toggleHelpAction->setStatusTip(tr("Toggle on-screen help display"));
    m_handlerMenu->addAction(m_toggleHelpAction);
    m_osgWidget->addAction(m_toggleHelpAction);

    m_captureScreenAction = new QAction(tr("Screen Capture (C)"), this);
    m_captureScreenAction->setShortcut(QKeySequence("C"));
    m_captureScreenAction->setShortcutContext(Qt::WidgetShortcut);
    m_captureScreenAction->setStatusTip(tr("Capture screenshot via OSG handler"));
    m_handlerMenu->addAction(m_captureScreenAction);
    m_osgWidget->addAction(m_captureScreenAction);

    m_handlerMenu->addSeparator();

    m_cycleThreadingAction = new QAction(tr("Cycle Threading Model (M)"), this);
    m_cycleThreadingAction->setShortcut(QKeySequence("M"));
    m_cycleThreadingAction->setShortcutContext(Qt::WidgetShortcut);
    m_cycleThreadingAction->setStatusTip(tr("Change OSG threading model"));
    m_handlerMenu->addAction(m_cycleThreadingAction);
    m_osgWidget->addAction(m_cycleThreadingAction);

    m_handlerMenu->addSeparator();

    m_increaseLODAction = new QAction(tr("Increase LOD Scale (+)"), this);
    m_increaseLODAction->setShortcut(QKeySequence("+"));
    m_increaseLODAction->setShortcutContext(Qt::WidgetShortcut);
    m_increaseLODAction->setStatusTip(tr("Increase level-of-detail scale"));
    m_handlerMenu->addAction(m_increaseLODAction);
    m_osgWidget->addAction(m_increaseLODAction);

    m_decreaseLODAction = new QAction(tr("Decrease LOD Scale (-)"), this);
    m_decreaseLODAction->setShortcut(QKeySequence("-"));
    m_decreaseLODAction->setShortcutContext(Qt::WidgetShortcut);
    m_decreaseLODAction->setStatusTip(tr("Decrease level-of-detail scale"));
    m_handlerMenu->addAction(m_decreaseLODAction);
    m_osgWidget->addAction(m_decreaseLODAction);

    m_handlerMenu->addSeparator();

    m_toggleFullscreenAction = new QAction(tr("Toggle Fullscreen (F)"), this);
    m_toggleFullscreenAction->setShortcut(QKeySequence("F"));
    m_toggleFullscreenAction->setShortcutContext(Qt::WidgetShortcut);
    m_toggleFullscreenAction->setStatusTip(tr("Toggle fullscreen via OSG handler"));
    m_handlerMenu->addAction(m_toggleFullscreenAction);
    m_osgWidget->addAction(m_toggleFullscreenAction);

    // Language menu (under Help)
    m_languageMenu = new QMenu(tr("&Language"), this);
    m_languageMenu->setIcon(QIcon(":/icons/light.svg"));
    m_languageGroup = new QActionGroup(this);
    m_languageGroup->setExclusive(true);

    m_chineseAction = new QAction(tr("\u4e2d\u6587"), m_languageGroup);
    m_chineseAction->setIcon(QIcon(":/icons/light.svg"));
    m_chineseAction->setCheckable(true);
    m_languageMenu->addAction(m_chineseAction);

    m_englishAction = new QAction(tr("English"), m_languageGroup);
    m_englishAction->setCheckable(true);
    m_languageMenu->addAction(m_englishAction);

    // Theme menu (under Help)
    m_themeMenu = new QMenu(tr("&Theme"), this);
    m_themeMenu->setIcon(QIcon(":/icons/theme.svg"));
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);

    m_darkThemeAction = new QAction(tr("Dark"), m_themeGroup);
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(true);
    m_darkThemeAction->setData(static_cast<int>(Dark));
    m_themeMenu->addAction(m_darkThemeAction);

    m_lightThemeAction = new QAction(tr("Light"), m_themeGroup);
    m_lightThemeAction->setCheckable(true);
    m_lightThemeAction->setData(static_cast<int>(Light));
    m_themeMenu->addAction(m_lightThemeAction);

    m_nordThemeAction = new QAction(tr("Nord"), m_themeGroup);
    m_nordThemeAction->setCheckable(true);
    m_nordThemeAction->setData(static_cast<int>(Nord));
    m_themeMenu->addAction(m_nordThemeAction);

    m_solarizedThemeAction = new QAction(tr("Solarized"), m_themeGroup);
    m_solarizedThemeAction->setCheckable(true);
    m_solarizedThemeAction->setData(static_cast<int>(Solarized));
    m_themeMenu->addAction(m_solarizedThemeAction);

    // Help menu
    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->addMenu(m_languageMenu);
    m_helpMenu->addMenu(m_themeMenu);
    m_helpMenu->addSeparator();

    m_aboutAction = new QAction(tr("About"), this);
    m_aboutAction->setIcon(QIcon(":/icons/about.svg"));
    m_aboutAction->setStatusTip(tr("About PicModelViewer"));
    m_helpMenu->addAction(m_aboutAction);
}

void MainWindow::setupToolBar()
{
    QToolBar* toolBar = addToolBar(tr("Main Toolbar"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(22, 22));
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    toolBar->addAction(m_openAction);
    toolBar->addAction(m_prevFileAction);
    toolBar->addAction(m_nextFileAction);
    toolBar->addSeparator();
    toolBar->addAction(m_resetViewAction);
    toolBar->addSeparator();
    toolBar->addAction(m_solidAction);
    toolBar->addAction(m_wireframeAction);
    toolBar->addAction(m_pointsAction);
    toolBar->addAction(m_solidWireframeAction);
    toolBar->addSeparator();
    toolBar->addAction(m_screenshotAction);
    toolBar->addAction(m_fullScreenAction);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"));
    m_fpsLabel = new QLabel("FPS: --");
    m_vertexLabel = new QLabel(tr("Vertices: 0"));
    m_faceLabel = new QLabel(tr("Faces: 0"));
    m_fileIndexLabel = new QLabel("");

    QFrame* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedWidth(1);

    QFrame* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedWidth(1);

    QFrame* sep3 = new QFrame;
    sep3->setFrameShape(QFrame::VLine);
    sep3->setFixedWidth(1);

    QFrame* sep4 = new QFrame;
    sep4->setFrameShape(QFrame::VLine);
    sep4->setFixedWidth(1);

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addWidget(sep1);
    statusBar()->addPermanentWidget(m_fileIndexLabel);
    statusBar()->addWidget(sep2);
    statusBar()->addPermanentWidget(m_vertexLabel);
    statusBar()->addWidget(sep3);
    statusBar()->addPermanentWidget(m_faceLabel);
    statusBar()->addWidget(sep4);
    statusBar()->addPermanentWidget(m_fpsLabel);
}

void MainWindow::setupDockWidgets()
{
    // Model Info dock
    m_modelInfoDock = new ModelInfoDock(this);
    m_modelInfoDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_modelInfoDock->setWindowIcon(QIcon(":/icons/model-info.svg"));
    addDockWidget(Qt::RightDockWidgetArea, m_modelInfoDock);
    m_viewMenu->addAction(m_modelInfoDock->toggleViewAction());

    // Scene Tree dock
    m_sceneTreeDock = new SceneTreeDock(this);
    m_sceneTreeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_sceneTreeDock->setWindowIcon(QIcon(":/icons/scene-tree.svg"));
    addDockWidget(Qt::LeftDockWidgetArea, m_sceneTreeDock);
    m_viewMenu->addAction(m_sceneTreeDock->toggleViewAction());

    // Light Control dock
    m_lightControlDock = new LightControlDock(m_osgWidget, this);
    m_lightControlDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_lightControlDock->setWindowIcon(QIcon(":/icons/light-control.svg"));
    addDockWidget(Qt::RightDockWidgetArea, m_lightControlDock);
    m_viewMenu->addAction(m_lightControlDock->toggleViewAction());
}

void MainWindow::setupConnections()
{
    // File
    connect(m_openAction, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_closeAction, &QAction::triggered, this, &MainWindow::closeModel);
    connect(m_exitAction, &QAction::triggered, this, &QMainWindow::close);
    connect(m_clearRecentAction, &QAction::triggered, this, &MainWindow::clearRecentFiles);
    connect(m_prevFileAction, &QAction::triggered, this, &MainWindow::prevFile);
    connect(m_nextFileAction, &QAction::triggered, this, &MainWindow::nextFile);

    // View
    connect(m_resetViewAction, &QAction::triggered, m_osgWidget, &OSGWidget::resetCamera);
    connect(m_fullScreenAction, &QAction::triggered, this, &MainWindow::toggleFullScreen);

    // Display mode
    connect(m_solidAction, &QAction::triggered, [this]() { m_osgWidget->setDisplayMode(OSGWidget::Solid); });
    connect(m_wireframeAction, &QAction::triggered, [this]() { m_osgWidget->setDisplayMode(OSGWidget::Wireframe); });
    connect(m_pointsAction, &QAction::triggered, [this]() { m_osgWidget->setDisplayMode(OSGWidget::Points); });
    connect(m_solidWireframeAction, &QAction::triggered, [this]() { m_osgWidget->setDisplayMode(OSGWidget::SolidAndWireframe); });

    // Tools
    connect(m_screenshotAction, &QAction::triggered, this, &MainWindow::takeScreenshot);

    // Batch convert
    connect(m_batchConvertAction, &QAction::triggered, this, &MainWindow::batchConvert);

    // OSG Handlers - send correct ASCII key codes directly to OSG event queue
    connect(m_toggleStatsAction, &QAction::triggered, [this]() {
        m_osgWidget->sendOSGKeyEvent('s');  // StatsHandler default key
    });
    connect(m_toggleHelpAction, &QAction::triggered, [this]() {
        m_osgWidget->sendOSGKeyEvent('h');  // HelpHandler default key
    });
    connect(m_captureScreenAction, &QAction::triggered, [this]() {
        m_osgWidget->sendOSGKeyEvent('c');  // ScreenCaptureHandler default key
    });
    connect(m_cycleThreadingAction, &QAction::triggered, [this]() {
        m_osgWidget->sendOSGKeyEvent('m');  // ThreadingHandler default key
    });
    connect(m_increaseLODAction, &QAction::triggered, [this]() {
        m_osgWidget->sendOSGKeyEvent('*');  // LODScaleHandler increase key
    });
    connect(m_decreaseLODAction, &QAction::triggered, [this]() {
        m_osgWidget->sendOSGKeyEvent('/');  // LODScaleHandler decrease key
    });
    connect(m_toggleFullscreenAction, &QAction::triggered, [this]() {
        m_osgWidget->sendOSGKeyEvent('f');  // WindowSizeHandler fullscreen key
    });

    // Language
    connect(m_chineseAction, &QAction::triggered, [this]() {
        I18nManager::instance().setLanguage(I18nManager::Chinese);
    });
    connect(m_englishAction, &QAction::triggered, [this]() {
        I18nManager::instance().setLanguage(I18nManager::English);
    });
    connect(&I18nManager::instance(), &I18nManager::languageChanged, this, &MainWindow::retranslateUI);

    // Theme
    connect(m_themeGroup, &QActionGroup::triggered, [this](QAction* action) {
        Theme theme = static_cast<Theme>(action->data().toInt());
        setTheme(theme);
    });

    // Help
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    // FPS
    connect(m_osgWidget, &OSGWidget::fpsChanged, [this](double fps) {
        m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
    });

    // Scene changed
    connect(m_osgWidget, &OSGWidget::sceneChanged, [this]() {
        if (m_osgWidget->getSceneData())
        {
            m_sceneTreeDock->updateTree(m_osgWidget->getSceneData());
        }
    });

    // Model loader progress
    connect(m_modelLoader, &ModelLoader::loadProgress, [this](const QString& msg, int percent) {
        m_statusLabel->setText(msg);
        Q_UNUSED(percent);
    });

    connect(m_modelLoader, &ModelLoader::loadFinished, [this](bool success, const QString& msg) {
        m_statusLabel->setText(msg);
        if (!success)
        {
            QMessageBox::warning(this, tr("Error"), msg);
        }
    });

    // Scene tree selection -> highlight & info
    connect(m_sceneTreeDock, &SceneTreeDock::nodeSelected, this, [this](osg::Node* node) {
        if (node)
        {
            m_osgWidget->selectNode(node);
            m_modelInfoDock->updateNodeInfo(node);
        }
        else
        {
            m_osgWidget->clearSelection();
            m_modelInfoDock->updateNodeInfo(nullptr);
        }
    });

    // OSG 3D pick -> sync with tree & info
    connect(m_osgWidget, &OSGWidget::nodeClicked, this, [this](osg::Node* node) {
        if (node)
        {
            m_sceneTreeDock->selectNode(node);
            m_modelInfoDock->updateNodeInfo(node);
        }
        else
        {
            m_sceneTreeDock->selectNode(nullptr);
            m_modelInfoDock->updateNodeInfo(nullptr);
        }
    });

    // Welcome page
    connect(m_welcomeWidget, &WelcomeWidget::openClicked, this, &MainWindow::openFile);
    connect(m_welcomeWidget, &WelcomeWidget::recentFileClicked, this, &MainWindow::loadModel);
}

void MainWindow::openFile()
{
    QStringList filters = ModelLoader::getSupportedFormatsFilter();
    QString filePath = QFileDialog::getOpenFileName(this,
        tr("Open Model File"), QString(), filters.join(";;"));

    if (!filePath.isEmpty())
    {
        loadModel(filePath);
    }
}

void MainWindow::closeModel()
{
    m_osgWidget->setSceneData(nullptr);
    m_modelInfoDock->clearInfo();
    m_sceneTreeDock->clearTree();
    m_currentFilePath.clear();
    m_dirFiles.clear();
    m_currentFileIndex = -1;
    m_statusLabel->setText(tr("Ready"));
    m_vertexLabel->setText(tr("Vertices: 0"));
    m_faceLabel->setText(tr("Faces: 0"));
    m_fileIndexLabel->setText("");
    setWindowTitle("PicModelViewer");
    updateFileNavState();

    // Switch back to welcome page
    m_stackWidget->setCurrentIndex(0);
}

void MainWindow::loadModel(const QString& filePath)
{
    m_statusLabel->setText(tr("Loading..."));

    osg::Node* node = m_modelLoader->loadFile(filePath);
    if (node)
    {
        m_osgWidget->setSceneData(node);

        // Update info
        ModelInfo info = m_modelLoader->getModelInfo();
        m_modelInfoDock->updateInfo(info);
        m_vertexLabel->setText(tr("Vertices: %1").arg(info.vertexCount));
        m_faceLabel->setText(tr("Faces: %1").arg(info.faceCount));

        m_currentFilePath = filePath;
        setWindowTitle(QString("PicModelViewer - %1").arg(QFileInfo(filePath).fileName()));

        // Switch to OSG view
        m_stackWidget->setCurrentIndex(1);

        // Scan directory for file navigation
        scanDirectoryFiles(filePath);

        // Add to recent files
        addToRecentFiles(filePath);
    }
}

void MainWindow::takeScreenshot()
{
    QString filePath = QFileDialog::getSaveFileName(this,
        tr("Save Screenshot"), QString(),
        tr("PNG Image (*.png);;JPEG Image (*.jpg);;BMP Image (*.bmp)"));

    if (!filePath.isEmpty())
    {
        QImage image = m_osgWidget->grabFramebuffer();
        if (image.save(filePath))
        {
            m_statusLabel->setText(tr("Screenshot saved: %1").arg(filePath));
        }
        else
        {
            QMessageBox::warning(this, tr("Error"), tr("Failed to save screenshot"));
        }
    }
}

void MainWindow::batchConvert()
{
    BatchConvertDialog dialog(m_modelConverter, this);
    dialog.exec();
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen())
    {
        showNormal();
        m_fullScreenAction->setChecked(false);
    }
    else
    {
        showFullScreen();
        m_fullScreenAction->setChecked(true);
    }
}

void MainWindow::switchLanguage()
{
    // Handled by action group
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About PicModelViewer"),
        tr("Universal 3D Model Viewer") + "\n\n" +
        tr("Version") + ": 1.0.0\n\n" +
        tr("A universal 3D model viewer based on Qt, OSG and Assimp."));
}

void MainWindow::updateRecentFiles()
{
    m_recentFilesMenu->clear();

    if (m_recentFiles.isEmpty())
    {
        QAction* emptyAction = m_recentFilesMenu->addAction(tr("No recent files"));
        emptyAction->setEnabled(false);
    }
    else
    {
        for (const QString& file : m_recentFiles)
        {
            QAction* action = m_recentFilesMenu->addAction(QFileInfo(file).fileName());
            action->setToolTip(file);
            action->setData(file);
            connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        }
        m_recentFilesMenu->addSeparator();
    }

    m_recentFilesMenu->addAction(m_clearRecentAction);

    // Update welcome page recent files
    if (m_welcomeWidget)
        m_welcomeWidget->setRecentFiles(m_recentFiles);
}

void MainWindow::addToRecentFiles(const QString& filePath)
{
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    while (m_recentFiles.size() > MAX_RECENT_FILES)
    {
        m_recentFiles.removeLast();
    }
    updateRecentFiles();
}

void MainWindow::clearRecentFiles()
{
    m_recentFiles.clear();
    updateRecentFiles();
}

void MainWindow::openRecentFile()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action)
    {
        QString filePath = action->data().toString();
        if (!filePath.isEmpty())
        {
            loadModel(filePath);
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty())
        {
            QString filePath = urls.first().toLocalFile();
            if (!filePath.isEmpty())
            {
                loadModel(filePath);
            }
        }
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        // Retranslate all UI elements
        m_fileMenu->setTitle(tr("&File"));
        m_viewMenu->setTitle(tr("&View"));
        m_toolsMenu->setTitle(tr("&Tools"));
        m_languageMenu->setTitle(tr("&Language"));
        m_helpMenu->setTitle(tr("&Help"));
        m_themeMenu->setTitle(tr("&Theme"));

        m_openAction->setText(tr("&Open..."));
        m_openAction->setStatusTip(tr("Open a model file"));
        m_prevFileAction->setText(tr("Previous File"));
        m_prevFileAction->setStatusTip(tr("Open previous model file in directory"));
        m_nextFileAction->setText(tr("Next File"));
        m_nextFileAction->setStatusTip(tr("Open next model file in directory"));
        m_closeAction->setText(tr("&Close"));
        m_closeAction->setStatusTip(tr("Close current model"));
        m_exitAction->setText(tr("E&xit"));
        m_exitAction->setStatusTip(tr("Exit application"));
        m_resetViewAction->setText(tr("Reset View"));
        m_resetViewAction->setStatusTip(tr("Reset camera to default position"));
        m_fullScreenAction->setText(tr("Full Screen"));
        m_fullScreenAction->setStatusTip(tr("Toggle full screen mode"));
        m_screenshotAction->setText(tr("Take Screenshot"));
        m_screenshotAction->setStatusTip(tr("Save screenshot to file"));
        m_aboutAction->setText(tr("About"));

        m_recentFilesMenu->setTitle(tr("&Recent Files"));
        m_clearRecentAction->setText(tr("Clear Recent"));
        m_displayModeMenu->setTitle(tr("Display Mode"));

        m_solidAction->setText(tr("Solid"));
        m_wireframeAction->setText(tr("Wireframe"));
        m_pointsAction->setText(tr("Points"));
        m_solidWireframeAction->setText(tr("Solid + Wireframe"));

        m_chineseAction->setText(tr("\u4e2d\u6587"));
        m_englishAction->setText(tr("English"));

        m_darkThemeAction->setText(tr("Dark"));
        m_lightThemeAction->setText(tr("Light"));
        m_nordThemeAction->setText(tr("Nord"));
        m_solarizedThemeAction->setText(tr("Solarized"));

        m_statusLabel->setText(tr("Ready"));

        // Retranslate dock widgets
        m_modelInfoDock->setWindowTitle(tr("Model Info"));
        m_sceneTreeDock->setWindowTitle(tr("Scene Tree"));
        m_lightControlDock->setWindowTitle(tr("Light Control"));

        // Update recent files menu
        updateRecentFiles();

        // Update file nav label
        updateFileNavState();

        // Retranslate welcome page
        if (m_welcomeWidget)
            m_welcomeWidget->retranslateUI();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUI()
{
    // Qt automatically sends LanguageChange event when translator is installed/removed.
    // The changeEvent handler will process the translation update.
    // No need to manually send the event again.
}

void MainWindow::loadStyleSheet(const QString& path)
{
    QFile file(path);
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        QString styleSheet = QString::fromUtf8(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();
    }
}

QString MainWindow::themeStyleSheetPath(Theme theme) const
{
    switch (theme)
    {
    case Dark:      return ":/style/dark.qss";
    case Light:     return ":/style/light.qss";
    case Nord:      return ":/style/nord.qss";
    case Solarized: return ":/style/solarized.qss";
    default:        return ":/style/dark.qss";
    }
}

QString MainWindow::themeName(Theme theme) const
{
    switch (theme)
    {
    case Dark:      return tr("Dark");
    case Light:     return tr("Light");
    case Nord:      return tr("Nord");
    case Solarized: return tr("Solarized");
    default:        return tr("Dark");
    }
}

void MainWindow::setTheme(Theme theme)
{
    m_currentTheme = theme;
    loadStyleSheet(themeStyleSheetPath(theme));

    // Update checked action
    QList<QAction*> actions = m_themeGroup->actions();
    for (QAction* action : actions)
    {
        if (action->data().toInt() == static_cast<int>(theme))
        {
            action->setChecked(true);
            break;
        }
    }

    m_statusLabel->setText(tr("Theme: %1").arg(themeName(theme)));
}

void MainWindow::scanDirectoryFiles(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();

    // Get supported extensions
    QStringList supportedExts = ModelLoader::getSupportedFormats();
    QStringList nameFilters;
    for (const QString& ext : supportedExts)
    {
        nameFilters << QString("*.%1").arg(ext.toLower());
    }

    // Scan directory for model files
    m_dirFiles.clear();
    QStringList entries = dir.entryList(nameFilters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QString& entry : entries)
    {
        m_dirFiles.append(dir.absoluteFilePath(entry));
    }

    // Find current file index
    QString absPath = fileInfo.absoluteFilePath();
    m_currentFileIndex = m_dirFiles.indexOf(absPath);

    updateFileNavState();
}

void MainWindow::prevFile()
{
    if (m_dirFiles.isEmpty() || m_currentFileIndex <= 0)
        return;
    m_currentFileIndex--;
    loadModel(m_dirFiles[m_currentFileIndex]);
}

void MainWindow::nextFile()
{
    if (m_dirFiles.isEmpty() || m_currentFileIndex >= m_dirFiles.size() - 1)
        return;
    m_currentFileIndex++;
    loadModel(m_dirFiles[m_currentFileIndex]);
}

void MainWindow::updateFileNavState()
{
    bool hasPrev = m_currentFileIndex > 0;
    bool hasNext = m_currentFileIndex >= 0 && m_currentFileIndex < m_dirFiles.size() - 1;

    m_prevFileAction->setEnabled(hasPrev);
    m_nextFileAction->setEnabled(hasNext);

    if (m_currentFileIndex >= 0 && !m_dirFiles.isEmpty())
    {
        m_fileIndexLabel->setText(tr("%1 / %2").arg(m_currentFileIndex + 1).arg(m_dirFiles.size()));
    }
    else
    {
        m_fileIndexLabel->setText("");
    }
}
