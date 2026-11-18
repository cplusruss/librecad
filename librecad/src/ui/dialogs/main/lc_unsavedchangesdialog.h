#ifndef LC_UNSAVEDCHANGESDIALOG_H
#define LC_UNSAVEDCHANGESDIALOG_H

#include <QList>
#include "lc_dialog.h"

class QListWidget;
class QC_MDIWindow;

/**
 * Single dialog presented at app exit / Close All when multiple documents have
 * unsaved changes. Lists every modified document with a checkbox per row plus
 * four exit actions: Save All, Save Selected (checked rows), Save None, Cancel.
 *
 * The result is conveyed via result() (DialogCode) and getCheckedWindows().
 */
class LC_UnsavedChangesDialog : public LC_Dialog {
    Q_OBJECT
public:
    enum Result {
        Cancel    = 0,
        SaveAll   = 1,
        SaveNone  = 2,
        SaveSome  = 3
    };

    explicit LC_UnsavedChangesDialog(const QList<QC_MDIWindow*>& modified,
                                      QWidget* parent = nullptr);
    ~LC_UnsavedChangesDialog() override;

    Result outcome() const { return m_outcome; }
    QList<QC_MDIWindow*> getCheckedWindows() const;

private slots:
    void onSaveAll();
    void onSaveSelected();
    void onSaveNone();
    void onCheckAll();
    void onUncheckAll();

private:
    QList<QC_MDIWindow*> m_windows;
    QListWidget* m_list{nullptr};
    Result m_outcome{Cancel};
};

#endif
