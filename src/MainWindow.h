#pragma once

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QStackedWidget>
#include <QUndoStack>

#include <osg/ref_ptr>
#include <osg/Node>

class OSGWidget;
class ModelLoader;
class ModelInfoDock;
class SceneTreeDock;
class NodeEditorDock;
class WelcomeWidget;
class ModelConverter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum Theme
    {
        Dark = 0,
        Light,
        Nord,
        Solarized
    };
    Q_ENUM(Theme)

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void loadModel(const QString& filePath);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDockWidgets();
    void setupConnections();

    void openFile();
    void closeModel();
    void takeScreenshot();
    void toggleFullScreen();
    void batchConvert();
    void switchLanguage();
    void showAbout();

    void prevFile();
    void nextFile();
    void updateFileNavState();
    void scanDirectoryFiles(const QString& filePath);

    void setTheme(Theme theme);
    void loadStyleSheet(const QString& path);
    QString themeStyleSheetPath(Theme theme) const;
    QString themeName(Theme theme) const;
    QString themeAccentColor(Theme theme) const;
    void updateWindowIcon();

    void updateRecentFiles();
    void addToRecentFiles(const QString& filePath);
    void clearRecentFiles();
    void openRecentFile();

    void retranslateUI();

    // Widgets
    QStackedWidget* m_stackWidget = nullptr;
    WelcomeWidget* m_welcomeWidget = nullptr;
    OSGWidget* m_osgWidget = nullptr;
    ModelLoader* m_modelLoader = nullptr;
    ModelConverter* m_modelConverter = nullptr;

    // Dock widgets
    ModelInfoDock* m_modelInfoDock = nullptr;
    SceneTreeDock* m_sceneTreeDock = nullptr;
    NodeEditorDock* m_nodeEditorDock = nullptr;

    // Undo system
    QUndoStack* m_undoStack = nullptr;

    // Menu
    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_toolsMenu = nullptr;
    QMenu* m_languageMenu = nullptr;
    QMenu* m_helpMenu = nullptr;
    QMenu* m_recentFilesMenu = nullptr;
    QMenu* m_displayModeMenu = nullptr;
    QMenu* m_themeMenu = nullptr;
    QMenu* m_handlerMenu = nullptr;

    // Actions
    QAction* m_openAction = nullptr;
    QAction* m_closeAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_deleteNodeAction = nullptr;
    QAction* m_hideNodeAction = nullptr;
    QAction* m_duplicateNodeAction = nullptr;
    QAction* m_resetViewAction = nullptr;
    QAction* m_fullScreenAction = nullptr;
    QAction* m_screenshotAction = nullptr;
    QAction* m_batchConvertAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_clearRecentAction = nullptr;
    QAction* m_prevFileAction = nullptr;
    QAction* m_nextFileAction = nullptr;

    // OSG Handler actions
    QAction* m_toggleStatsAction = nullptr;
    QAction* m_toggleHelpAction = nullptr;
    QAction* m_captureScreenAction = nullptr;
    QAction* m_cycleThreadingAction = nullptr;
    QAction* m_increaseLODAction = nullptr;
    QAction* m_decreaseLODAction = nullptr;
    QAction* m_toggleFullscreenAction = nullptr;

    QActionGroup* m_displayModeGroup = nullptr;
    QAction* m_solidAction = nullptr;
    QAction* m_wireframeAction = nullptr;
    QAction* m_pointsAction = nullptr;
    QAction* m_solidWireframeAction = nullptr;

    QActionGroup* m_languageGroup = nullptr;
    QAction* m_chineseAction = nullptr;
    QAction* m_englishAction = nullptr;

    QActionGroup* m_themeGroup = nullptr;
    QAction* m_darkThemeAction = nullptr;
    QAction* m_lightThemeAction = nullptr;
    QAction* m_nordThemeAction = nullptr;
    QAction* m_solarizedThemeAction = nullptr;

    // Status bar labels
    QLabel* m_fpsLabel = nullptr;
    QLabel* m_vertexLabel = nullptr;
    QLabel* m_faceLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_fileIndexLabel = nullptr;

    // State
    QStringList m_recentFiles;
    QString m_currentFilePath;
    static const int MAX_RECENT_FILES = 10;

    // File navigation state
    QStringList m_dirFiles;       // model files in current directory
    int m_currentFileIndex = -1;  // index in m_dirFiles
    Theme m_currentTheme = Dark;
    bool m_silentLoad = false;
    bool m_loading = false;
};
