#ifndef LC_DIALOGTABKEYS_H
#define LC_DIALOGTABKEYS_H

#include <QObject>

// Application-wide filter: in any QDialog, numpad + / - select the next /
// previous enabled tab of the focused (or first visible) tab widget.
class LC_DialogTabKeys : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* watched, QEvent* event) override;
};

#endif
