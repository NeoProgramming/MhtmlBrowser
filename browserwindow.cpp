
#include "browser.h"
#include "browserwindow.h"
#include "tabwidget.h"
#include "webview.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QScreen>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#include <QWebEngineFindTextResult>
#endif
#include <QWebEngineProfile>
#include <QDockWidget>
#include <QTreeView>
#include <QPushButton>
#include <QFileSystemModel>
#include <QLabel>
#include <QSettings>
#include <QTimer>

#include "EmptyFoldersFileSystemModel.h"

BrowserWindow::BrowserWindow(Browser *browser, QWebEngineProfile *profile, bool forDevTools)
    : m_browser(browser)
    , m_profile(profile)
    , m_tabWidget(new TabWidget(profile, this))
    , m_progressBar(nullptr)
    , m_historyBackAction(nullptr)
    , m_historyForwardAction(nullptr)
    , m_stopAction(nullptr)
    , m_reloadAction(nullptr)
    , m_stopReloadAction(nullptr)
    , m_urlLineEdit(nullptr)
    , m_favAction(nullptr)
{

	// Создаем док-виджет для боковой панели
	m_sidebarDock = new QDockWidget(tr("Categories"), this);
	m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	// Создаем содержимое для док-виджета
	QWidget *sidebarContent = new QWidget;
	QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarContent);

	// Добавляем элементы управления
	m_categoryTree = new QTreeView;
	QPushButton *newCategoryBtn = new QPushButton(tr("New theme"));
	QPushButton *moveArticleBtn = new QPushButton(tr("MOVE ARTICLE"));
	m_tagsEdit = new QLineEdit;
	m_tagsEdit->setPlaceholderText(tr("Comma-separated tags"));

	sidebarLayout->addWidget(moveArticleBtn);
	sidebarLayout->addWidget(new QLabel(tr("Themes:")));
	sidebarLayout->addWidget(m_categoryTree, 1);
	sidebarLayout->addWidget(newCategoryBtn);	
	sidebarLayout->addWidget(new QLabel(tr("Tags:")));
	sidebarLayout->addWidget(m_tagsEdit);
	sidebarLayout->addStretch(); // Растягивающийся элемент

	sidebarContent->setLayout(sidebarLayout);
	m_sidebarDock->setWidget(sidebarContent);

	// Добавляем док-виджет в главное окно
	addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);
	
	// Настройка модели для дерева файлов
	m_categoriesModel = new EmptyFoldersFileSystemModel(this);
	m_categoriesModel->setRootPath(QDir::homePath());
	m_categoriesModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	
	// Если уже есть сохраненный путь, используем его
	QSettings settings;
	m_categoriesRootFolder = settings.value("categoriesRootFolder").toString();

	if (m_categoriesRootFolder.isEmpty()) {
		// По умолчанию используем домашнюю папку или создаем подпапку
		m_categoriesRootFolder = QDir::homePath() + "/MHTML_Categories";
		QDir().mkpath(m_categoriesRootFolder);
	}

	// Устанавливаем корневой путь
	m_categoriesModel->setRootPath(m_categoriesRootFolder);
	m_categoryTree->setModel(m_categoriesModel);
	m_categoryTree->setRootIndex(m_categoriesModel->index(m_categoriesRootFolder));
	
	// Подключаем сигналы
	connect(newCategoryBtn, &QPushButton::clicked, this, &BrowserWindow::createNewCategory);
	connect(moveArticleBtn, &QPushButton::clicked, this, &BrowserWindow::moveCurrentArticle);

	// Настройка внешнего вида
	m_sidebarDock->setFeatures(QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable |
		QDockWidget::DockWidgetClosable);

	// Запоминание состояния layout
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

	// Настройка дерева
	m_categoryTree->setHeaderHidden(true);
	m_categoryTree->setAnimated(true);
	m_categoryTree->hideColumn(1); // Скрываем колонки размера, типа и даты
	m_categoryTree->hideColumn(2);
	m_categoryTree->hideColumn(3);

	///
    setAttribute(Qt::WA_DeleteOnClose, true);
    setFocusPolicy(Qt::ClickFocus);

    if (!forDevTools) {
        m_progressBar = new QProgressBar(this);

        QToolBar *toolbar = createToolBar();
        addToolBar(toolbar);
        menuBar()->addMenu(createFileMenu(m_tabWidget));
        menuBar()->addMenu(createEditMenu());
        menuBar()->addMenu(createViewMenu(toolbar));
        menuBar()->addMenu(createWindowMenu(m_tabWidget));
        menuBar()->addMenu(createHelpMenu());
    }

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    if (!forDevTools) {
        addToolBarBreak();

        m_progressBar->setMaximumHeight(1);
        m_progressBar->setTextVisible(false);
        m_progressBar->setStyleSheet(QStringLiteral("QProgressBar {border: 0px} QProgressBar::chunk {background-color: #da4453}"));

        layout->addWidget(m_progressBar);
    }

    layout->addWidget(m_tabWidget);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    connect(m_tabWidget, &TabWidget::titleChanged, this, &BrowserWindow::handleWebViewTitleChanged);
    if (!forDevTools) {
        connect(m_tabWidget, &TabWidget::linkHovered, [this](const QString& url) {
            statusBar()->showMessage(url);
        });
        connect(m_tabWidget, &TabWidget::loadProgress, this, &BrowserWindow::handleWebViewLoadProgress);
        connect(m_tabWidget, &TabWidget::webActionEnabledChanged, this, &BrowserWindow::handleWebActionEnabledChanged);
        connect(m_tabWidget, &TabWidget::urlChanged, [this](const QUrl &url) {
            m_urlLineEdit->setText(url.toDisplayString());
        });
        connect(m_tabWidget, &TabWidget::favIconChanged, m_favAction, &QAction::setIcon);
        connect(m_tabWidget, &TabWidget::devToolsRequested, this, &BrowserWindow::handleDevToolsRequested);
        connect(m_urlLineEdit, &QLineEdit::returnPressed, [this]() {
            m_tabWidget->setUrl(QUrl::fromUserInput(m_urlLineEdit->text()));
        });
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        connect(m_tabWidget, &TabWidget::findTextFinished, this, &BrowserWindow::handleFindTextFinished);
#endif

        QAction *focusUrlLineEditAction = new QAction(this);
        addAction(focusUrlLineEditAction);
        focusUrlLineEditAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
        connect(focusUrlLineEditAction, &QAction::triggered, this, [this] () {
            m_urlLineEdit->setFocus(Qt::ShortcutFocusReason);
        });
    }

    handleWebViewTitleChanged(QString());
    m_tabWidget->createTab();

	// Загружаем настройки
	readSettings();

}

BrowserWindow::~BrowserWindow()
{
	writeSettings();
}

QSize BrowserWindow::sizeHint() const
{
    QRect desktopRect = QApplication::primaryScreen()->geometry();
    QSize size = desktopRect.size() * qreal(0.9);
    return size;
}

QMenu *BrowserWindow::createFileMenu(TabWidget *tabWidget)
{
    QMenu *fileMenu = new QMenu(tr("&File"));
    fileMenu->addAction(tr("&New Window"), this, &BrowserWindow::handleNewWindowTriggered, QKeySequence::New);
    fileMenu->addAction(tr("New &Incognito Window"), this, &BrowserWindow::handleNewIncognitoWindowTriggered);

    QAction *newTabAction = new QAction(tr("New &Tab"), this);
    newTabAction->setShortcuts(QKeySequence::AddTab);
    connect(newTabAction, &QAction::triggered, this, [this]() {
        m_tabWidget->createTab();
        m_urlLineEdit->setFocus();
    });
    fileMenu->addAction(newTabAction);

    fileMenu->addAction(tr("&Open File..."), this, &BrowserWindow::handleFileOpenTriggered, QKeySequence::Open);

	// Action для выбора папки с исходными файлами
	QAction *openSourceFolderAction = new QAction(tr("&Open Source Folder..."), this);
	openSourceFolderAction->setShortcut(tr("Ctrl+O"));
	connect(openSourceFolderAction, &QAction::triggered, this, &BrowserWindow::selectSourceFolder);
	fileMenu->addAction(openSourceFolderAction);

	// Action для выбора корневой папки категорий
	QAction *openCategoriesRootAction = new QAction(tr("Open &Categories Root..."), this);
	openCategoriesRootAction->setShortcut(tr("Ctrl+Shift+O"));
	connect(openCategoriesRootAction, &QAction::triggered, this, &BrowserWindow::selectCategoriesRootFolder);
	fileMenu->addAction(openCategoriesRootAction);

    fileMenu->addSeparator();

    QAction *closeTabAction = new QAction(tr("&Close Tab"), this);
    closeTabAction->setShortcuts(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, [tabWidget]() {
        tabWidget->closeTab(tabWidget->currentIndex());
    });
    fileMenu->addAction(closeTabAction);

    QAction *closeAction = new QAction(tr("&Quit"),this);
    closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(closeAction);

    connect(fileMenu, &QMenu::aboutToShow, [this, closeAction]() {
        if (m_browser->windows().count() == 1)
            closeAction->setText(tr("&Quit"));
        else
            closeAction->setText(tr("&Close Window"));
    });
    return fileMenu;
}

QMenu *BrowserWindow::createEditMenu()
{
    QMenu *editMenu = new QMenu(tr("&Edit"));
    QAction *findAction = editMenu->addAction(tr("&Find"));
    findAction->setShortcuts(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &BrowserWindow::handleFindActionTriggered);

    QAction *findNextAction = editMenu->addAction(tr("Find &Next"));
    findNextAction->setShortcut(QKeySequence::FindNext);
    connect(findNextAction, &QAction::triggered, [this]() {
        if (!currentTab() || m_lastSearch.isEmpty())
            return;
        currentTab()->findText(m_lastSearch);
    });

    QAction *findPreviousAction = editMenu->addAction(tr("Find &Previous"));
    findPreviousAction->setShortcut(QKeySequence::FindPrevious);
    connect(findPreviousAction, &QAction::triggered, [this]() {
        if (!currentTab() || m_lastSearch.isEmpty())
            return;
        currentTab()->findText(m_lastSearch, QWebEnginePage::FindBackward);
    });

    return editMenu;
}

QMenu *BrowserWindow::createViewMenu(QToolBar *toolbar)
{
    QMenu *viewMenu = new QMenu(tr("&View"));
    m_stopAction = viewMenu->addAction(tr("&Stop"));
    QList<QKeySequence> shortcuts;
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Period));
    shortcuts.append(Qt::Key_Escape);
    m_stopAction->setShortcuts(shortcuts);
    connect(m_stopAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Stop);
    });

    m_reloadAction = viewMenu->addAction(tr("Reload Page"));
    m_reloadAction->setShortcuts(QKeySequence::Refresh);
    connect(m_reloadAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Reload);
    });

    QAction *zoomIn = viewMenu->addAction(tr("Zoom &In"));
    zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(zoomIn, &QAction::triggered, [this]() {
        if (currentTab())
            currentTab()->setZoomFactor(currentTab()->zoomFactor() + 0.1);
    });

    QAction *zoomOut = viewMenu->addAction(tr("Zoom &Out"));
    zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(zoomOut, &QAction::triggered, [this]() {
        if (currentTab())
            currentTab()->setZoomFactor(currentTab()->zoomFactor() - 0.1);
    });

    QAction *resetZoom = viewMenu->addAction(tr("Reset &Zoom"));
    resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(resetZoom, &QAction::triggered, [this]() {
        if (currentTab())
            currentTab()->setZoomFactor(1.0);
    });


    viewMenu->addSeparator();
    QAction *viewToolbarAction = new QAction(tr("Toolbar"),this);
	viewToolbarAction->setCheckable(true); // Делаем action переключаемым
	viewToolbarAction->setChecked(true);   // По умолчанию включено
    connect(viewToolbarAction, &QAction::toggled, [toolbar](bool checked) {
        if (checked) {
            toolbar->close();
        } else {
            toolbar->show();
        }
    });
	// Опционально: добавляем toggle в меню View
	viewMenu->addAction(m_sidebarDock->toggleViewAction());
    viewMenu->addAction(viewToolbarAction);

    QAction *viewStatusbarAction = new QAction(tr("Status Bar"), this);
    viewStatusbarAction->setCheckable(true);
	viewStatusbarAction->setChecked(true);
    connect(viewStatusbarAction, &QAction::toggled, [this, viewStatusbarAction](bool checked) {
        if (statusBar()->isVisible()) {
            statusBar()->close();
        } else {
            statusBar()->show();
        }
    });
    viewMenu->addAction(viewStatusbarAction);



    return viewMenu;
}

QMenu *BrowserWindow::createWindowMenu(TabWidget *tabWidget)
{
    QMenu *menu = new QMenu(tr("&Window"));

    QAction *nextTabAction = new QAction(tr("Show Next Tab"), this);
    QList<QKeySequence> shortcuts;
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BraceRight));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Less));
    nextTabAction->setShortcuts(shortcuts);
    connect(nextTabAction, &QAction::triggered, tabWidget, &TabWidget::nextTab);

    QAction *previousTabAction = new QAction(tr("Show Previous Tab"), this);
    shortcuts.clear();
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BraceLeft));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Greater));
    previousTabAction->setShortcuts(shortcuts);
    connect(previousTabAction, &QAction::triggered, tabWidget, &TabWidget::previousTab);

    connect(menu, &QMenu::aboutToShow, [this, menu, nextTabAction, previousTabAction]() {
        menu->clear();
        menu->addAction(nextTabAction);
        menu->addAction(previousTabAction);
        menu->addSeparator();

        QVector<BrowserWindow*> windows = m_browser->windows();
        int index(-1);
        for (auto window : windows) {
            QAction *action = menu->addAction(window->windowTitle(), this, &BrowserWindow::handleShowWindowTriggered);
            action->setData(++index);
            action->setCheckable(true);
            if (window == this)
                action->setChecked(true);
        }
    });
    return menu;
}

QMenu *BrowserWindow::createHelpMenu()
{
    QMenu *helpMenu = new QMenu(tr("&Help"));
    helpMenu->addAction(tr("About &Qt"), qApp, QApplication::aboutQt);
    return helpMenu;
}

QToolBar *BrowserWindow::createToolBar()
{
    QToolBar *navigationBar = new QToolBar(tr("Navigation"));
    navigationBar->setMovable(false);
    navigationBar->toggleViewAction()->setEnabled(false);

    m_historyBackAction = new QAction(this);
    QList<QKeySequence> backShortcuts = QKeySequence::keyBindings(QKeySequence::Back);
    for (auto it = backShortcuts.begin(); it != backShortcuts.end();) {
        // Chromium already handles navigate on backspace when appropriate.
        if ((*it)[0] == Qt::Key_Backspace)
            it = backShortcuts.erase(it);
        else
            ++it;
    }
    // For some reason Qt doesn't bind the dedicated Back key to Back.
    backShortcuts.append(QKeySequence(Qt::Key_Back));
    m_historyBackAction->setShortcuts(backShortcuts);
    m_historyBackAction->setIconVisibleInMenu(false);
    m_historyBackAction->setIcon(QIcon(QStringLiteral(":go-previous.png")));
    m_historyBackAction->setToolTip(tr("Go back in history"));
    connect(m_historyBackAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Back);
    });
    navigationBar->addAction(m_historyBackAction);

    m_historyForwardAction = new QAction(this);
    QList<QKeySequence> fwdShortcuts = QKeySequence::keyBindings(QKeySequence::Forward);
    for (auto it = fwdShortcuts.begin(); it != fwdShortcuts.end();) {
        if (((*it)[0] & Qt::Key_unknown) == Qt::Key_Backspace)
            it = fwdShortcuts.erase(it);
        else
            ++it;
    }
    fwdShortcuts.append(QKeySequence(Qt::Key_Forward));
    m_historyForwardAction->setShortcuts(fwdShortcuts);
    m_historyForwardAction->setIconVisibleInMenu(false);
    m_historyForwardAction->setIcon(QIcon(QStringLiteral(":go-next.png")));
    m_historyForwardAction->setToolTip(tr("Go forward in history"));
    connect(m_historyForwardAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::Forward);
    });
    navigationBar->addAction(m_historyForwardAction);

    m_stopReloadAction = new QAction(this);
    connect(m_stopReloadAction, &QAction::triggered, [this]() {
        m_tabWidget->triggerWebPageAction(QWebEnginePage::WebAction(m_stopReloadAction->data().toInt()));
    });
    navigationBar->addAction(m_stopReloadAction);

    m_urlLineEdit = new QLineEdit(this);
    m_favAction = new QAction(this);
    m_urlLineEdit->addAction(m_favAction, QLineEdit::LeadingPosition);
    m_urlLineEdit->setClearButtonEnabled(true);
    navigationBar->addWidget(m_urlLineEdit);

    auto downloadsAction = new QAction(this);
    downloadsAction->setIcon(QIcon(QStringLiteral(":go-bottom.png")));
    downloadsAction->setToolTip(tr("Show downloads"));
    navigationBar->addAction(downloadsAction);
   
    return navigationBar;
}

void BrowserWindow::handleWebActionEnabledChanged(QWebEnginePage::WebAction action, bool enabled)
{
    switch (action) {
    case QWebEnginePage::Back:
        m_historyBackAction->setEnabled(enabled);
        break;
    case QWebEnginePage::Forward:
        m_historyForwardAction->setEnabled(enabled);
        break;
    case QWebEnginePage::Reload:
        m_reloadAction->setEnabled(enabled);
        break;
    case QWebEnginePage::Stop:
        m_stopAction->setEnabled(enabled);
        break;
    default:
        qWarning("Unhandled webActionChanged signal");
    }
}

void BrowserWindow::handleWebViewTitleChanged(const QString &title)
{
    QString suffix = m_profile->isOffTheRecord()
        ? tr("Qt Simple Browser (Incognito)")
        : tr("Qt Simple Browser");

    if (title.isEmpty())
        setWindowTitle(suffix);
    else
        setWindowTitle(title + " - " + suffix);
}

void BrowserWindow::handleNewWindowTriggered()
{
    BrowserWindow *window = m_browser->createWindow();
    window->m_urlLineEdit->setFocus();
}

void BrowserWindow::handleNewIncognitoWindowTriggered()
{
    BrowserWindow *window = m_browser->createWindow(/* offTheRecord: */ true);
    window->m_urlLineEdit->setFocus();
}

void BrowserWindow::handleFileOpenTriggered()
{
    QUrl url = QFileDialog::getOpenFileUrl(this, tr("Open Web Resource"), QString(),
                                                tr("Web Resources (*.html *.htm *.svg *.png *.gif *.svgz);;All files (*.*)"));
    if (url.isEmpty())
        return;
    currentTab()->setUrl(url);
}

void BrowserWindow::handleFindActionTriggered()
{
    if (!currentTab())
        return;
    bool ok = false;
    QString search = QInputDialog::getText(this, tr("Find"),
                                           tr("Find:"), QLineEdit::Normal,
                                           m_lastSearch, &ok);
    if (ok && !search.isEmpty()) {
        m_lastSearch = search;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        currentTab()->findText(m_lastSearch);
#else
        currentTab()->findText(m_lastSearch, 0, [this](bool found) {
            if (!found)
                statusBar()->showMessage(tr("\"%1\" not found.").arg(m_lastSearch));
        });
#endif
    }
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    if (m_tabWidget->count() > 1) {
        int ret = QMessageBox::warning(this, tr("Confirm close"),
                                       tr("Are you sure you want to close the window ?\n"
                                          "There are %1 tabs open.").arg(m_tabWidget->count()),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::No) {
            event->ignore();
            return;
        }
    }
    event->accept();
    deleteLater();
}

TabWidget *BrowserWindow::tabWidget() const
{
    return m_tabWidget;
}

WebView *BrowserWindow::currentTab() const
{
    return m_tabWidget->currentWebView();
}

void BrowserWindow::handleWebViewLoadProgress(int progress)
{
    static QIcon stopIcon(QStringLiteral(":process-stop.png"));
    static QIcon reloadIcon(QStringLiteral(":view-refresh.png"));

    if (0 < progress && progress < 100) {
        m_stopReloadAction->setData(QWebEnginePage::Stop);
        m_stopReloadAction->setIcon(stopIcon);
        m_stopReloadAction->setToolTip(tr("Stop loading the current page"));
        m_progressBar->setValue(progress);
    } else {
        m_stopReloadAction->setData(QWebEnginePage::Reload);
        m_stopReloadAction->setIcon(reloadIcon);
        m_stopReloadAction->setToolTip(tr("Reload the current page"));
        m_progressBar->setValue(0);
    }
}

void BrowserWindow::handleShowWindowTriggered()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int offset = action->data().toInt();
        QVector<BrowserWindow*> windows = m_browser->windows();
        windows.at(offset)->activateWindow();
        windows.at(offset)->currentTab()->setFocus();
    }
}

void BrowserWindow::handleDevToolsRequested(QWebEnginePage *source)
{
    source->setDevToolsPage(m_browser->createDevToolsWindow()->currentTab()->page());
    source->triggerAction(QWebEnginePage::InspectElement);
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
void BrowserWindow::handleFindTextFinished(const QWebEngineFindTextResult &result)
{
    if (result.numberOfMatches() == 0) {
        statusBar()->showMessage(tr("\"%1\" not found.").arg(m_lastSearch));
    } else {
        statusBar()->showMessage(tr("\"%1\" found: %2/%3").arg(m_lastSearch,
                                                               QString::number(result.activeMatch()),
                                                               QString::number(result.numberOfMatches())));
    }
}
#endif

void BrowserWindow::createNewCategory()
{
	bool ok;
	QString categoryName = QInputDialog::getText(this,
		tr("New Category"),
		tr("Category name:"),
		QLineEdit::Normal,
		"", &ok);
	if (ok && !categoryName.isEmpty()) {
		// Получаем текущую выбранную папку или корневую
		QModelIndex currentIndex = m_categoryTree->currentIndex();

		QString parentPath;
		if (currentIndex.isValid()) {
			parentPath = m_categoriesModel->filePath(currentIndex);
		}
		else {
			parentPath = m_categoriesRootFolder;
		}

		// Создаем новую папку
		QDir parentDir(parentPath);
		if (!parentDir.mkdir(categoryName)) {
			QMessageBox::warning(this, tr("Error"),
				tr("Failed to create folder"));
		}
		else {
			statusBar()->showMessage(tr("Category created: %1").arg(categoryName), 2000);
		}
	}
}

void BrowserWindow::moveCurrentArticle()
{
	QString currentArticle = getCurrentArticlePath();
	if (currentArticle.isEmpty()) return;

	// Получаем выбранную папку в дереве
	QModelIndex selectedIndex = m_categoryTree->currentIndex();
	if (!selectedIndex.isValid()) return;

	QFileSystemModel *model = static_cast<QFileSystemModel*>(m_categoryTree->model());
	QString destinationPath = model->filePath(selectedIndex);

	// Проверяем, что это папка
	QFileInfo destInfo(destinationPath);
	if (!destInfo.isDir()) {
		destinationPath = destInfo.path();
	}

	// Перемещаем файл
	QFileInfo articleInfo(currentArticle);
	QString newPath = destinationPath + "/" + articleInfo.fileName();

	if (QFile::rename(currentArticle, newPath)) {
		// Загружаем следующую статью
		loadNextUnprocessedFile();
	}
	else {
		QMessageBox::warning(this, tr("Error"),
			tr("Failed to move article"));
	}
}

QString BrowserWindow::getCurrentArticlePath() const
{
	// Получаем путь текущей загруженной статьи
	return m_currentArticlePath;
}

void BrowserWindow::loadNextUnprocessedFile()
{
	QString nextFile = findNextUnprocessedFile();
	if (!nextFile.isEmpty()) {
		loadMhtmlFile(nextFile);
	}
	else {
		// Нет больше файлов
		currentTab()->setHtml("<h1>All files processed!</h1>");
		statusBar()->showMessage(tr("All files processed - no more articles"));
		m_currentArticlePath.clear();
	}
}

QString BrowserWindow::findNextUnprocessedFile()
{
	if (m_sourceFolder.isEmpty()) return QString();

	// Ищем ЛЮБОЙ mhtml файл в папке
	QDirIterator it(m_sourceFolder,
		QStringList() << "*.mhtml" << "*.mht",
		QDir::Files,
		QDirIterator::NoIteratorFlags);

	if (it.hasNext()) {
		return it.next();
	}

	return QString(); // Файлов не найдено
}

void BrowserWindow::selectSourceFolder()
{
	QString initialPath = m_sourceFolder.isEmpty() ? QDir::homePath() : m_sourceFolder;

	QString folder = QFileDialog::getExistingDirectory(
		this,
		tr("Select Folder with MHTML Files"),
		initialPath,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (!folder.isEmpty()) {
		m_sourceFolder = folder;

		// Обновляем заголовок окна
		updateWindowTitle();

		// Можно сразу загрузить первую статью из папки
		loadNextUnprocessedFile();

		QMessageBox::information(this,
			tr("Source Folder Selected"),
			tr("Source folder set to: %1").arg(folder));
	}
}

void BrowserWindow::updateWindowTitle()
{
	QString title = tr("MHTML Browser");
	if (!m_sourceFolder.isEmpty()) {
		QDir dir(m_sourceFolder);
		title += " - " + dir.dirName();
	}
	setWindowTitle(title);
}

void BrowserWindow::loadMhtmlFile(const QString &filePath)
{
	if (!QFile::exists(filePath)) {
		statusBar()->showMessage(tr("File not found: %1").arg(filePath));
		return;
	}

	// Сохраняем путь к текущему файлу
	m_currentArticlePath = filePath;

	// Прямая загрузка через file:// URL
	QUrl fileUrl = QUrl::fromLocalFile(filePath);
	currentTab()->setUrl(fileUrl);

	// Обновляем заголовок окна с именем файла
	QFileInfo fileInfo(filePath);
	setWindowTitle(tr("MHTML Browser - %1").arg(fileInfo.fileName()));

	// Обновляем статусную строку
	statusBar()->showMessage(tr("Loaded: %1").arg(fileInfo.fileName()), 2000);
}

void BrowserWindow::selectCategoriesRootFolder()
{
	QString initialPath = m_categoriesRootFolder.isEmpty() ? QDir::homePath() : m_categoriesRootFolder;

	QString folder = QFileDialog::getExistingDirectory(
		this,
		tr("Select Categories Root Folder"),
		initialPath,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (!folder.isEmpty()) {
		setCategoriesRootPath(folder);

		QMessageBox::information(this,
			tr("Categories Root Selected"),
			tr("Categories root folder set to: %1").arg(folder));
	}
}

void BrowserWindow::setCategoriesRootPath(const QString &path)
{
	m_categoriesRootFolder = path;

	// Обновляем модель дерева
	if (m_categoriesModel) {
		m_categoriesModel->setRootPath(path);
		m_categoryTree->setRootIndex(m_categoriesModel->index(path));
	}

	// Создаем папку если не существует
	QDir rootDir(path);
	if (!rootDir.exists()) {
		rootDir.mkpath(".");
	}

	// Обновляем статус
	statusBar()->showMessage(tr("Categories root: %1").arg(path), 3000);
}

void BrowserWindow::readSettings()
{
	QSettings settings;
	m_sourceFolder = settings.value("sourceFolder").toString();
	m_categoriesRootFolder = settings.value("categoriesRootFolder").toString();

	if (!m_sourceFolder.isEmpty()) {
		updateWindowTitle();
		QTimer::singleShot(100, this, &BrowserWindow::loadNextUnprocessedFile);
	}

	// Восстанавливаем корневую папку категорий
	if (!m_categoriesRootFolder.isEmpty()) {
		setCategoriesRootPath(m_categoriesRootFolder);
	}
	else {
		// Папка по умолчанию
		QString defaultCategoriesPath = QDir::homePath() + "/MHTML_Categories";
		QDir().mkpath(defaultCategoriesPath);
		setCategoriesRootPath(defaultCategoriesPath);
	}
}

void BrowserWindow::writeSettings()
{
	QSettings settings;
	settings.setValue("sourceFolder", m_sourceFolder);
	settings.setValue("categoriesRootFolder", m_categoriesRootFolder);
}
