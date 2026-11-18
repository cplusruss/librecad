#ifndef MAINWINDOWX_H
#define MAINWINDOWX_H

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QPointer>

/**
 * an eXtension of QMainWindow;
 * It is intended to be generic,
 * for use with other projects.
 */
class QToolBar;
class QDockWidget;

class MainWindowX : public QMainWindow{
    Q_OBJECT
public:
    explicit MainWindowX(QWidget* parent = nullptr);
    void sortWidgetsByTitle(QList<QDockWidget*>& list);
    void sortWidgetsByTitle(QList<QToolBar*>& list);
    void sortWidgetsByGroupAndTitle(QList<QToolBar*>& list);
public slots:
    void toggleRightDockArea(bool state);
    void toggleLeftDockArea(bool state);
    void toggleTopDockArea(bool state);
    void toggleBottomDockArea(bool state);
    void toggleFloatingDockwidgets(bool state);
private:
    void toggleDockArea(Qt::DockWidgetArea area, bool state);
    /// What each area toggle hid, so switching the area back on restores just
    /// that set rather than every dock widget assigned to the area.
    QHash<int, QList<QPointer<QDockWidget>>> m_hiddenByAreaToggle;
};

#endif // MAINWINDOWX_H
