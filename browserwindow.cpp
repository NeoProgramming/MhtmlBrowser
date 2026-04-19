
#include "browserwindow.h"
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

#include "WebPage.h"
#include "EmptyFoldersFileSystemModel.h"

BrowserWindow::BrowserWindow(QWebEngineProfile *profile)
    : m_profile(profile)    
    , m_progressBar(nullptr)    
    , m_stopAction(nullptr)
    , m_reloadAction(nullptr)
    , m_stopReloadAction(nullptr)
    , m_urlLineEdit(nullptr)
    , m_favAction(nullptr)
{

	// Создаем док-виджет для боковой панели
	m_sidebarDock = new QDockWidget(tr("Categories"), this);
	m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	// Создаем основную область 
	m_webView = new WebView(this);
	WebPage *webPage = new WebPage(profile, m_webView);
	m_webView->setPage(webPage);

	// Создаем содержимое для док-виджета
	QWidget *sidebarContent = new QWidget;
	QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarContent);

	// Добавляем элементы управления
	m_categoryTree = new QTreeView;
	QPushButton *newCategoryBtn = new QPushButton(tr("New theme"));
	QPushButton *moveArticleBtn = new QPushButton(tr("MOVE ARTICLE"));

	sidebarLayout->addWidget(m_labSrc = new QLabel(tr("Src: <NOT SELECTED>")));
	sidebarLayout->addWidget(moveArticleBtn);
	sidebarLayout->addWidget(m_labDst = new QLabel(tr("Dst: <NOT SELECTED>")));
	sidebarLayout->addWidget(m_categoryTree, 1);
	sidebarLayout->addWidget(newCategoryBtn);	

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
	m_categoriesModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	m_categoryTree->setModel(m_categoriesModel);
	m_categoryTree->setRootIndex(m_categoriesModel->index(m_categoriesRootFolder));
	// Голубой цвет для выделения без фокуса
	QString styleSheet =
		"QTreeView::item:selected:!active {"
		"    background-color: #2196F3;"
		"    color: #000000;"
		"}";
	m_categoryTree->setStyleSheet(styleSheet);
	
	// Подключаем сигналы к кнопкам
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

	// создание меню
	m_progressBar = new QProgressBar(this);
    QToolBar *toolbar = createToolBar();
    addToolBar(toolbar);
    menuBar()->addMenu(createFileMenu());
    menuBar()->addMenu(createViewMenu(toolbar));
    menuBar()->addMenu(createHelpMenu());

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    addToolBarBreak();

    m_progressBar->setMaximumHeight(1);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(QStringLiteral("QProgressBar {border: 0px} QProgressBar::chunk {background-color: #da4453}"));
    
	layout->addWidget(m_progressBar);
	layout->addWidget(m_webView);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

	// Подключаем сигналы (вместо tabWidget-сигналов):
	connect(m_webView, &WebView::webLoadProgress, this, &BrowserWindow::handleWebViewLoadProgress);
	connect(m_webView, &WebView::titleChanged, this, &BrowserWindow::handleWebViewTitleChanged);
	connect(m_webView, &QWebEngineView::urlChanged, [this](const QUrl &url) {
		m_urlLineEdit->setText(url.toDisplayString());
	});
	
    connect(m_urlLineEdit, &QLineEdit::returnPressed, [this]() {
		m_webView->setUrl(QUrl::fromUserInput(m_urlLineEdit->text()));
    });

    QAction *focusUrlLineEditAction = new QAction(this);
    addAction(focusUrlLineEditAction);
    focusUrlLineEditAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(focusUrlLineEditAction, &QAction::triggered, this, [this] () {
        m_urlLineEdit->setFocus(Qt::ShortcutFocusReason);
    });
    
    handleWebViewTitleChanged(QString());
  
	// Загружаем настройки
	readSettings();
	m_labSrc->setText(m_sourceFolder);
	m_labDst->setText(m_categoriesRootFolder);
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

QMenu *BrowserWindow::createFileMenu()
{
    QMenu *fileMenu = new QMenu(tr("&File"));
    	
    fileMenu->addAction(tr("&Open File..."), this, &BrowserWindow::handleFileOpenTriggered, QKeySequence::Open);

	// Action для выбора папки с исходными файлами
	QAction *openSourceFolderAction = new QAction(tr("Select Source Folder..."), this);
	connect(openSourceFolderAction, &QAction::triggered, this, &BrowserWindow::selectSourceFolder);
	fileMenu->addAction(openSourceFolderAction);

	// Action для выбора корневой папки категорий
	QAction *openCategoriesRootAction = new QAction(tr("Select Categories Root..."), this);
	connect(openCategoriesRootAction, &QAction::triggered, this, &BrowserWindow::selectCategoriesRootFolder);
	fileMenu->addAction(openCategoriesRootAction);

	// Action для перемещения в произвольную папку
	QAction *moveToCustomFolderAction = new QAction(tr("Move to Custom Folder..."), this);
	connect(moveToCustomFolderAction, &QAction::triggered, this, &BrowserWindow::moveToCustomFolder);
	fileMenu->addAction(moveToCustomFolderAction);

	// Action для удаления текущего файла
	QAction *deleteFileAction = new QAction(tr("&Delete File"), this);
	connect(deleteFileAction, &QAction::triggered, this, &BrowserWindow::deleteCurrentFile);
	fileMenu->addSeparator();
	fileMenu->addAction(deleteFileAction);

    fileMenu->addSeparator();    

    QAction *closeAction = new QAction(tr("&Quit"),this);
    closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(closeAction);

    return fileMenu;
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
		m_webView->triggerPageAction(QWebEnginePage::Stop);
    });

    m_reloadAction = viewMenu->addAction(tr("Reload Page"));
    m_reloadAction->setShortcuts(QKeySequence::Refresh);
    connect(m_reloadAction, &QAction::triggered, [this]() {
		m_webView->triggerPageAction(QWebEnginePage::Reload);
    });

    QAction *zoomIn = viewMenu->addAction(tr("Zoom &In"));
    zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(zoomIn, &QAction::triggered, [this]() {
		m_webView->setZoomFactor(m_webView->zoomFactor() + 0.1);
    });

    QAction *zoomOut = viewMenu->addAction(tr("Zoom &Out"));
    zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(zoomOut, &QAction::triggered, [this]() {
		m_webView->setZoomFactor(m_webView->zoomFactor() - 0.1);
    });

    QAction *resetZoom = viewMenu->addAction(tr("Reset &Zoom"));
    resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(resetZoom, &QAction::triggered, [this]() {
		m_webView->setZoomFactor(1.0);
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

    m_stopReloadAction = new QAction(this);
    connect(m_stopReloadAction, &QAction::triggered, [this]() {
		
		m_webView->triggerPageAction(QWebEnginePage::WebAction(m_stopReloadAction->data().toInt()));
		
		// загружаем новый файл ТОЛЬКО если это была перезагрузка, а не остановка
		QWebEnginePage::WebAction action = QWebEnginePage::WebAction(m_stopReloadAction->data().toInt());
		if (action == QWebEnginePage::Reload) {
			loadNextUnprocessedFile();
		}
    });
    navigationBar->addAction(m_stopReloadAction);

    m_urlLineEdit = new QLineEdit(this);
    m_favAction = new QAction(this);
    m_urlLineEdit->addAction(m_favAction, QLineEdit::LeadingPosition);
    m_urlLineEdit->setClearButtonEnabled(true);
    navigationBar->addWidget(m_urlLineEdit);
	   
    return navigationBar;
}

void BrowserWindow::handleWebActionEnabledChanged(QWebEnginePage::WebAction action, bool enabled)
{
    switch (action) {
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
    QString suffix = tr("MHTML Browser");

    if (title.isEmpty())
        setWindowTitle(suffix);
    else
        setWindowTitle(title + " - " + suffix);
}


void BrowserWindow::handleFileOpenTriggered()
{
    QUrl url = QFileDialog::getOpenFileUrl(this, tr("Open Web Resource"), QString(),
                                                tr("Web Resources (*.html *.htm *.svg *.png *.gif *.svgz);;All files (*.*)"));
    if (url.isEmpty())
        return;
	m_webView->setUrl(url);
}

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    deleteLater();
}

WebView  *BrowserWindow::currentView() const
{
    return m_webView;
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
	if (m_currentFilePath.isEmpty()) return;

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

	moveCurrentFileToFolder(destinationPath);

//	// Перемещаем файл
//	QFileInfo articleInfo(currentArticle);
//	QString newPath = destinationPath + "/" + articleInfo.fileName();
//
//	if (QFile::rename(currentArticle, newPath)) {
//		// Загружаем следующую статью
//		loadNextUnprocessedFile();
//	}
//	else {
//		QMessageBox::warning(this, tr("Error"),
//			tr("Failed to move article"));
//	}
}

void BrowserWindow::loadNextUnprocessedFile()
{
	QString nextFile = findNextUnprocessedFile();
	if (!nextFile.isEmpty()) {
		loadMhtmlFile(nextFile);
	}
	else {
		// Нет больше файлов
		currentView()->setHtml("<h1>All files processed!</h1>");
		statusBar()->showMessage(tr("All files processed - no more articles"));
		m_currentFilePath.clear();
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

		m_labSrc->setText(m_sourceFolder);
		
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
	m_currentFilePath = filePath;

	// Прямая загрузка через file:// URL
	QUrl fileUrl = QUrl::fromLocalFile(filePath);
	m_webView->setUrl(fileUrl);

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

	m_labDst->setText(m_categoriesRootFolder);
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

void BrowserWindow::deleteCurrentFile()
{
	if (m_currentFilePath.isEmpty()) {
		QMessageBox::information(this,
			tr("No File"),
			tr("No file is currently loaded."));
		return;
	}

	QFileInfo fileInfo(m_currentFilePath);

	// Диалог подтверждения
	QMessageBox::StandardButton reply = QMessageBox::question(
		this,
		tr("Confirm Delete"),
		tr("Are you sure you want to delete the file:\n\"%1\"?\n\nThis action cannot be undone.")
		.arg(fileInfo.fileName()),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No
	);

	if (reply == QMessageBox::Yes) {
		// Пытаемся удалить файл
		if (QFile::remove(m_currentFilePath)) {
			statusBar()->showMessage(tr("File deleted: %1").arg(fileInfo.fileName()), 3000);

			// Загружаем следующий файл
			loadNextUnprocessedFile();
		}
		else {
			QMessageBox::warning(this,
				tr("Delete Failed"),
				tr("Failed to delete the file:\n%1").arg(m_currentFilePath));
		}
	}
}

void BrowserWindow::moveToCustomFolder()
{
	if (m_currentFilePath.isEmpty()) {
		QMessageBox::information(this,
			tr("No File"),
			tr("No file is currently loaded."));
		return;
	}

	// Диалог выбора папки без ограничений
	QString selectedFolder = QFileDialog::getExistingDirectory(
		this,
		tr("Select Destination Folder"),
		QDir::homePath(),  // Начинаем с домашней папки
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (!selectedFolder.isEmpty()) {
		moveCurrentFileToFolder(selectedFolder);
	}
}

QString BrowserWindow::generateUniqueFileName(const QString &destinationFolder, const QString &originalFileName)
{
	QFileInfo fileInfo(originalFileName);
	QString baseName = fileInfo.completeBaseName(); // Имя без расширения
	QString extension = fileInfo.suffix();

	QString basePath = QDir(destinationFolder).absoluteFilePath(baseName);
	QString newFilePath = basePath + "." + extension;

	// Если файл с исходным именем не существует - возвращаем его
	if (!QFile::exists(newFilePath)) {
		return newFilePath;
	}

	// Ищем доступное имя с номером в скобках
	int counter = 1;
	do {
		newFilePath = basePath + "(" + QString::number(counter) + ")." + extension;
		counter++;
	} while (QFile::exists(newFilePath) && counter < 1000); // Защита от бесконечного цикла

	return newFilePath;
}

void BrowserWindow::moveCurrentFileToFolder(const QString &destinationFolder)
{
	if (m_currentFilePath.isEmpty()) return;

	QFileInfo currentFileInfo(m_currentFilePath);
	QString originalFileName = currentFileInfo.fileName();
	//QString destinationPath = QDir(destinationFolder).absoluteFilePath(currentFileInfo.fileName());
	// Генерируем уникальное имя файла
	QString destinationPath = generateUniqueFileName(destinationFolder, originalFileName);


	// Проверяем, не пытаемся ли переместить файл в ту же папку
	if (QDir::cleanPath(m_currentFilePath) == QDir::cleanPath(destinationPath)) {
		QMessageBox::information(this,
			tr("Same Folder"),
			tr("File is already in the selected folder."));
		return;
	}
	

	// Перемещаем файл
	if (QFile::rename(m_currentFilePath, destinationPath)) {

		statusBar()->showMessage(tr("File moved to: %1").arg(destinationFolder), 3000);

		// Загружаем следующий файл
		loadNextUnprocessedFile();
	}
	else {
		QMessageBox::warning(this,
			tr("Move Failed"),
			tr("Failed to move the file to:\n%1\n\nCheck permissions or if file is in use.")
			.arg(destinationFolder));
	}
}
