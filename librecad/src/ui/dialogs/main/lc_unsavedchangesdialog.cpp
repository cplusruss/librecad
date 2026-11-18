#include "lc_unsavedchangesdialog.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "qc_mdiwindow.h"

LC_UnsavedChangesDialog::LC_UnsavedChangesDialog(const QList<QC_MDIWindow*>& modified,
                                                  QWidget* parent)
    : LC_Dialog(parent, "DlgUnsavedChanges")
    , m_windows(modified) {
    setWindowTitle(tr("Unsaved Changes"));

    auto* header = new QLabel(
        tr("%n document(s) have unsaved changes. Check the ones you want to save,\n"
           "then choose an action below.", "", modified.size()),
        this);
    header->setWordWrap(true);

    m_list = new QListWidget(this);
    for (QC_MDIWindow* w : modified) {
        if (w == nullptr) continue;
        QString name = w->getFileName();
        if (name.isEmpty()) {
            name = w->windowTitle();
        } else {
            QFileInfo fi(name);
            name = QStringLiteral("%1   [%2]").arg(fi.fileName(), fi.absolutePath());
        }
        auto* item = new QListWidgetItem(name, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<qulonglong>(w)));
    }

    auto* checkRow = new QHBoxLayout;
    auto* checkAll   = new QPushButton(tr("Check All"),   this);
    auto* uncheckAll = new QPushButton(tr("Uncheck All"), this);
    checkRow->addWidget(checkAll);
    checkRow->addWidget(uncheckAll);
    checkRow->addStretch();
    connect(checkAll,   &QPushButton::clicked, this, &LC_UnsavedChangesDialog::onCheckAll);
    connect(uncheckAll, &QPushButton::clicked, this, &LC_UnsavedChangesDialog::onUncheckAll);

    auto* buttons = new QDialogButtonBox(this);
    auto* btnSaveAll   = buttons->addButton(tr("Save &All"),       QDialogButtonBox::AcceptRole);
    auto* btnSaveSel   = buttons->addButton(tr("Save &Selected"),  QDialogButtonBox::AcceptRole);
    auto* btnSaveNone  = buttons->addButton(tr("Save &None"),      QDialogButtonBox::DestructiveRole);
    auto* btnCancel    = buttons->addButton(QDialogButtonBox::Cancel);
    btnSaveSel->setDefault(true);
    btnSaveSel->setFocus();
    connect(btnSaveAll,  &QPushButton::clicked, this, &LC_UnsavedChangesDialog::onSaveAll);
    connect(btnSaveSel,  &QPushButton::clicked, this, &LC_UnsavedChangesDialog::onSaveSelected);
    connect(btnSaveNone, &QPushButton::clicked, this, &LC_UnsavedChangesDialog::onSaveNone);
    connect(btnCancel,   &QPushButton::clicked, this, &LC_UnsavedChangesDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(header);
    layout->addWidget(m_list, 1);
    layout->addLayout(checkRow);
    layout->addWidget(buttons);

    resize(560, 360);
}

LC_UnsavedChangesDialog::~LC_UnsavedChangesDialog() = default;

QList<QC_MDIWindow*> LC_UnsavedChangesDialog::getCheckedWindows() const {
    QList<QC_MDIWindow*> result;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (item->checkState() == Qt::Checked) {
            auto* w = reinterpret_cast<QC_MDIWindow*>(item->data(Qt::UserRole).toULongLong());
            if (w != nullptr) result.append(w);
        }
    }
    return result;
}

void LC_UnsavedChangesDialog::onCheckAll() {
    for (int i = 0; i < m_list->count(); ++i)
        m_list->item(i)->setCheckState(Qt::Checked);
}

void LC_UnsavedChangesDialog::onUncheckAll() {
    for (int i = 0; i < m_list->count(); ++i)
        m_list->item(i)->setCheckState(Qt::Unchecked);
}

void LC_UnsavedChangesDialog::onSaveAll() {
    m_outcome = SaveAll;
    accept();
}

void LC_UnsavedChangesDialog::onSaveSelected() {
    m_outcome = SaveSome;
    accept();
}

void LC_UnsavedChangesDialog::onSaveNone() {
    m_outcome = SaveNone;
    accept();
}
