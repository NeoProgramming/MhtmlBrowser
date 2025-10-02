
#ifndef BROWSER_H
#define BROWSER_H

#include <QVector>
#include <QWebEngineProfile>

class BrowserWindow;

class Browser
{
public:
    Browser();

    QVector<BrowserWindow*> windows() { return m_windows; }

    BrowserWindow *createWindow(bool offTheRecord = false);
    BrowserWindow *createDevToolsWindow();


private:
    QVector<BrowserWindow*> m_windows;
    QScopedPointer<QWebEngineProfile> m_otrProfile;
};
#endif // BROWSER_H
