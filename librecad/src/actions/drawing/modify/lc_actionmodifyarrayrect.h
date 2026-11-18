#ifndef LC_ACTIONMODIFYARRAYRECT_H
#define LC_ACTIONMODIFYARRAYRECT_H

#include "rs_actioninterface.h"

/**
 * Rectangular array of the current selection — AutoCAD ARRAY (rectangular)
 * equivalent. Pops a small dialog asking rows / columns / row spacing /
 * column spacing, then clones the selected entities at each grid offset.
 *
 * One-shot action (no mouse interaction): init() runs the dialog + apply
 * and immediately finishes.
 */
class LC_ActionModifyArrayRect : public RS_ActionInterface {
    Q_OBJECT
public:
    explicit LC_ActionModifyArrayRect(LC_ActionContext* actionContext);
    void init(int status) override;
    void trigger() override;
    bool mayBeTerminatedExternally() override { return !m_dialogVisible; }

private:
    bool m_dialogVisible{false};
};

#endif // LC_ACTIONMODIFYARRAYRECT_H
