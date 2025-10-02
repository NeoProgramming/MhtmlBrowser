
#ifndef BROWSERWINDOW_H
#define BROWSERWINDOW_H

#include <QMainWindow>
#include <QTime>
#include <QWebEnginePage>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QProgressBar;
QT_END_NAMESPACE

class Browser;
class TabWidget;
class WebView;
class QTreeView;
class QFileSystemModel;

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(Browser *browser, QWebEngineProfile *profile, bool forDevTools = false);
	~BrowserWindow();
    QSize sizeHint() const override;
    TabWidget *tabWidget() const;
    WebView *currentTab() const;
    Browser *browser() { return m_browser; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void handleNewWindowTriggered();
    void handleNewIncognitoWindowTriggered();
    void handleFileOpenTriggered();
    void handleFindActionTriggered();
    void handleShowWindowTriggered();
    void handleWebViewLoadProgress(int);
    void handleWebViewTitleChanged(const QString &title);
    void handleWebActionEnabledChanged(QWebEnginePage::WebAction action, bool enabled);
    void handleDevToolsRequested(QWebEnginePage *source);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    void handleFindTextFinished(const QWebEngineFindTextResult &result);
#endif

	void createNewCategory();
	void moveCurrentArticle();
	void selectSourceFolder();
	void selectCategoriesRootFolder();
	void updateWindowTitle();
private:
    QMenu *createFileMenu(TabWidget *tabWidget);
    QMenu *createEditMenu();
    QMenu *createViewMenu(QToolBar *toolBar);
    QMenu *createWindowMenu(TabWidget *tabWidget);
    QMenu *createHelpMenu();
    QToolBar *createToolBar();
	QString getCurrentArticlePath() const;
	void loadNextUncategorizedArticle();
	void loadNextUnprocessedFile();
	QString findNextUnprocessedFile();
	void loadFirstArticleFromFolder();
	void loadMhtmlFile(const QString &filePath);
	void setCategoriesRootPath(const QString &path);
	void readSettings();
	void writeSettings();
private:
    Browser *m_browser;
    QWebEngineProfile *m_profile;
    TabWidget *m_tabWidget;
    QProgressBar *m_progressBar;
    QAction *m_historyBackAction;
    QAction *m_historyForwardAction;
    QAction *m_stopAction;
    QAction *m_reloadAction;
    QAction *m_stopReloadAction;
    QLineEdit *m_urlLineEdit;
    QAction *m_favAction;
    QString m_lastSearch;

	QDockWidget *m_sidebarDock;
	QTreeView *m_categoryTree;
	QLineEdit *m_tagsEdit;
	QString m_currentArticlePath;
	QString m_sourceFolder;
	QString m_categoriesRootFolder;
	QFileSystemModel *m_categoriesModel;
};

#endif // BROWSERWINDOW_H
