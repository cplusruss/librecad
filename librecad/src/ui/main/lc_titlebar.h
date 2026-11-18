#ifndef LC_TITLEBAR_H
#define LC_TITLEBAR_H

#include <QAbstractButton>
#include <QImage>
#include <QWidget>

class QLabel;
class QMainWindow;
class QMenuBar;

// Windows only. The main window's caption row (icon, menu bar, title, caption buttons) on a frameless window:
// Windows does the move, snap, resize and double-click maximize through WM_NCHITTEST; the 3D border is drawn in the
// window's contents margin. Port of rdoc's app\ui\titlebar.py (TitleBar + FramelessMixin).
class LC_CaptionButton : public QAbstractButton {
public:
    explicit LC_CaptionButton(unsigned kind);
    unsigned kind;
protected:
    void paintEvent(QPaintEvent* event) override;
};

class LC_TitleBar : public QWidget {
public:
    static constexpr int BORDER = 4;
    static constexpr int TITLE_MIN = 48;

    explicit LC_TitleBar(QMainWindow* win);
    // the window's menu widget, the window frameless; the window must already forward nativeEvent here, since
    // applying the style sends WM_NCCALCSIZE at once
    void install();
    QMenuBar* menuBar() const { return m_menuBar; }
    void zoom(bool on);
    bool restoreFrameGeometry(const QByteArray& geometry);
    void applyFrameStyle();

    // forwarded from the window's own handlers
    bool nativeEvent(void* message, qintptr* result);
    void windowChanged(QEvent* event);
    void windowResized();
    void paintBorder();

    // the caption button exactly as Windows draws it (DrawFrameControl), w x h device pixels
    static QImage frameControl(unsigned kind, bool pushed, int w, int h);
    // the resize-edge hit code for pos in a window of the given size, 0 when pos is inside the border
    static int edgeHit(const QSize& size, const QPoint& pos);
    // the window title as Windows shows it: Qt's "[*]" placeholder becomes "*" when modified
    static QString captionText(const QString& title, bool modified);

protected:
    bool event(QEvent* event) override;

private:
    void syncFrame();
    void syncTitle();
    void syncIcon();
    int hitTest(const QPoint& pos) const;

    QMainWindow* m_win;
    QLabel* m_icon;
    QMenuBar* m_menuBar;
    QLabel* m_title;
    LC_CaptionButton* m_max;
};

#endif
