
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
class WebView;
class QTreeView;
class QFileSystemModel;
class QLabel;

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    BrowserWindow(QWebEngineProfile *profile);
	~BrowserWindow();
    QSize sizeHint() const override;
    WebView *currentView() const;
  
protected:
    void closeEvent(QCloseEvent *event) override;

private slots:

    void handleFileOpenTriggered();
    void handleWebViewLoadProgress(int);
    void handleWebViewTitleChanged(const QString &title);
    void handleWebActionEnabledChanged(QWebEnginePage::WebAction action, bool enabled);

	void createNewCategory();
	void moveCurrentArticle();
	void selectSourceFolder();
	void selectCategoriesRootFolder();
	void deleteCurrentFile();
	void moveToCustomFolder();
	void updateWindowTitle();

private:
    QMenu *createFileMenu();
    QMenu *createViewMenu(QToolBar *toolBar);   
    QMenu *createHelpMenu();
    QToolBar *createToolBar();
	void loadNextUnprocessedFile();
	QString findNextUnprocessedFile();
	void loadMhtmlFile(const QString &filePath);
	void setCategoriesRootPath(const QString &path);
	void moveCurrentFileToFolder(const QString &destinationFolder);
	void readSettings();
	void writeSettings();
	QString generateUniqueFileName(const QString &destinationFolder, const QString &originalFileName);

private:
    QWebEngineProfile *m_profile;
	WebView *m_webView;
    QProgressBar *m_progressBar;

    QAction *m_stopAction;
    QAction *m_reloadAction;
    QAction *m_stopReloadAction;
    QLineEdit *m_urlLineEdit;
    QAction *m_favAction;
    QString m_lastSearch;

	QLabel *m_labSrc;	// папка-источник сортируемого контента
	QLabel *m_labDst;	// корнева€ папка дл€ дерева, в которое мы перемещаем контент
	QDockWidget *m_sidebarDock; // докинг-панель
	QTreeView *m_categoryTree;  // визуальное дерево папок, в которые мы перемещаем контент
	QFileSystemModel *m_categoriesModel;	// модель файловой системы дл€ дерева папок, в которые мы перемещаем контент

	QString m_currentFilePath;	// путь к текущему объекту контента
	QString m_sourceFolder;		// 
	QString m_categoriesRootFolder;		

//	QLineEdit *m_tagsEdit;		// 
};

#endif // BROWSERWINDOW_H
