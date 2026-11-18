#ifndef LC_DYNAMICINPUT_H
#define LC_DYNAMICINPUT_H

#include <QObject>
#include <QString>

/**
 * Process-wide state holder for the AutoCAD-style "dynamic input" cursor
 * overlay: a small floating label rendered next to the mouse cursor that
 * shows two things at once —
 *   - the prompt the current action is asking for ("Specify first point"),
 *   - the live text the user is typing in the command line.
 *
 * Singleton because both producers (RS_ActionInterface::updateMouseWidget*
 * and QG_CommandEdit::textChanged) and the consumer (QG_GraphicView::paint)
 * live in different subsystems and we'd otherwise have to thread a pointer
 * through three or four layers.
 *
 * Emits changed() whenever prompt or typed text changes so consumers can
 * trigger a repaint without polling.
 */
class LC_DynamicInput : public QObject {
    Q_OBJECT
public:
    static LC_DynamicInput& instance();

    bool    isEnabled() const   { return m_enabled; }
    void    setEnabled(bool on);

    QString prompt() const      { return m_prompt; }
    void    setPrompt(const QString& s);

    QString typed() const       { return m_typed; }
    void    setTyped(const QString& s);

    LC_DynamicInput(const LC_DynamicInput&) = delete;
    LC_DynamicInput& operator=(const LC_DynamicInput&) = delete;

signals:
    void changed();

private:
    LC_DynamicInput() = default;

    bool    m_enabled{false};
    QString m_prompt;
    QString m_typed;
};

#endif
