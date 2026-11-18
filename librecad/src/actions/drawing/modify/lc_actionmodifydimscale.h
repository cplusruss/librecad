#ifndef LC_ACTIONMODIFYDIMSCALE_H
#define LC_ACTIONMODIFYDIMSCALE_H

#include "rs_actioninterface.h"

/**
 * AutoCAD DIMSCALE equivalent. Pops a modal dialog with a spin box bound to
 * the document's dim styles' DIMSCALE field. valueChanged() pushes the new
 * value to every dim style and re-runs update() on every dimension entity so
 * the user sees the change in realtime. Cancel restores the original value.
 *
 * One-shot action: init() runs the dialog and immediately finishes.
 */
class LC_ActionModifyDimScale : public RS_ActionInterface {
    Q_OBJECT
public:
    explicit LC_ActionModifyDimScale(LC_ActionContext* actionContext);
    void init(int status) override;
    void trigger() override;
    bool mayBeTerminatedExternally() override { return !m_dialogVisible; }

private:
    void applyScaleToAllStyles(double scale);
    void refreshAllDimensions();

    bool m_dialogVisible{false};
};

#endif
