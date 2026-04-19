
#include "browserwindow.h"
#include <QApplication>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

QUrl commandLineUrlArgument()
{
    const QStringList args = QCoreApplication::arguments();
    for (const QString &arg : args.mid(1)) {
        if (!arg.startsWith(QLatin1Char('-')))
            return QUrl::fromUserInput(arg);
    }
    return QUrl(QStringLiteral("https://www.qt.io"));
}

int main(int argc, char **argv)
{
	QCoreApplication::setOrganizationName("MHTML Sorter");
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);

	QWebEngineSettings::defaultSettings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);

	BrowserWindow *window = new BrowserWindow(QWebEngineProfile::defaultProfile());
	window->show();
	
	int result = app.exec();

	// явно удал€ем окно до разрушени€ QApplication
//	delete window;

	return result;
}
