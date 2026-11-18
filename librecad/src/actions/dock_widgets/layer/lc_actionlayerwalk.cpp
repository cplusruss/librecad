/*******************************************************************************
 *
 This file is part of the LibreCAD project, a 2D CAD program

 Copyright (C) 2026 LibreCAD.org

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ******************************************************************************/
#include "lc_actionlayerwalk.h"

#include <QByteArray>
#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QStringList>
#include <QVBoxLayout>

#include "lc_dialog.h"
#include "qc_applicationwindow.h"
#include "rs_graphic.h"
#include "rs_layer.h"
#include "rs_layerlist.h"
#include "rs_settings.h"

LC_ActionLayerWalk::LC_ActionLayerWalk(LC_ActionContext* actionContext)
    : RS_ActionInterface("Layer Walk", actionContext, RS2::ActionLayerWalk) {}

namespace {
// Subsequence match: every char of pattern appears in target in order,
// case-insensitive. Empty pattern matches anything.
bool fuzzyMatch(const QString& pattern, const QString& target) {
    if (pattern.isEmpty()) return true;
    int pi = 0;
    const QString p = pattern.toLower();
    const QString t = target.toLower();
    for (int ti = 0; ti < t.size() && pi < p.size(); ++ti) {
        if (t.at(ti) == p.at(pi)) ++pi;
    }
    return pi == p.size();
}

// Forwards Up/Down/PageUp/PageDown/Home/End/Enter from the search box to the
// list so the user can drive selection without leaving the search field.
class ListNavFilter : public QObject {
public:
    ListNavFilter(QListWidget* list, QObject* parent) : QObject(parent), m_list(list) {}
protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            switch (ke->key()) {
                case Qt::Key_Up: case Qt::Key_Down:
                case Qt::Key_PageUp: case Qt::Key_PageDown:
                case Qt::Key_Home: case Qt::Key_End:
                    QCoreApplication::sendEvent(m_list, ke);
                    return true;
                default: break;
            }
        }
        return QObject::eventFilter(obj, ev);
    }
private:
    QListWidget* m_list;
};
}

void LC_ActionLayerWalk::trigger() {
    if (m_graphic == nullptr) { finish(false); return; }
    auto* layers = m_graphic->getLayerList();
    if (layers == nullptr || layers->count() == 0) { finish(false); return; }

    // Snapshot original frozen-state so we can restore on Cancel.
    QHash<QString, bool> originalFrozen;
    QStringList allNames;
    for (RS_Layer* l : *layers) {
        if (l != nullptr) {
            originalFrozen.insert(l->getName(), l->isFrozen());
            allNames << l->getName();
        }
    }

    auto& appWin = QC_ApplicationWindow::getAppWindow();
    QDialog dlg(appWin ? appWin.get() : nullptr);
    dlg.setWindowTitle(QObject::tr("Layer Walk"));
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(
        QObject::tr("Click to isolate. Ctrl+Click adds, Shift+Click selects a range.\n"
                    "Type to filter. 'Restore' undoes, 'Show all' un-freezes everything."), &dlg));

    auto* search = new QLineEdit(&dlg);
    search->setPlaceholderText(QObject::tr("Filter layers..."));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);

    auto* list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setUniformItemSizes(true);
    for (const QString& name : allNames) list->addItem(name);
    layout->addWidget(list, /*stretch=*/1);

    auto* btnRow = new QHBoxLayout;
    auto* btnRestore = new QPushButton(QObject::tr("&Restore"), &dlg);
    auto* btnAllOn   = new QPushButton(QObject::tr("Show &all"), &dlg);
    auto* btnClose   = new QPushButton(QObject::tr("&Close"), &dlg);
    btnClose->setDefault(true);
    btnRow->addWidget(btnRestore);
    btnRow->addWidget(btnAllOn);
    btnRow->addStretch(1);
    btnRow->addWidget(btnClose);
    layout->addLayout(btnRow);

    auto restoreOriginal = [layers, &originalFrozen]() {
        for (RS_Layer* l : *layers) {
            if (l == nullptr) continue;
            const auto it = originalFrozen.constFind(l->getName());
            if (it != originalFrozen.cend()) l->freeze(it.value());
        }
        layers->fireLayerToggled();
    };
    auto unfreezeAll = [layers]() {
        for (RS_Layer* l : *layers) {
            if (l != nullptr) l->freeze(false);
        }
        layers->fireLayerToggled();
    };
    auto isolateSet = [layers, restoreOriginal](const QSet<QString>& shown) {
        if (shown.isEmpty()) { restoreOriginal(); return; }
        for (RS_Layer* l : *layers) {
            if (l == nullptr) continue;
            l->freeze(!shown.contains(l->getName()));
        }
        layers->fireLayerToggled();
    };

    QObject::connect(list, &QListWidget::itemSelectionChanged, &dlg,
        [list, &isolateSet]() {
            QSet<QString> shown;
            const auto sel = list->selectedItems();
            for (auto* it : sel) shown.insert(it->text());
            isolateSet(shown);
        });

    // Filter on type. Hidden items would otherwise stay selected and still
    // drive isolation, so on every keystroke we reseat selection on the first
    // visible match. That gives an immediate preview as the user types.
    QObject::connect(search, &QLineEdit::textChanged, &dlg,
        [list](const QString& pattern) {
            int firstVisible = -1;
            for (int i = 0; i < list->count(); ++i) {
                auto* it = list->item(i);
                const bool match = fuzzyMatch(pattern, it->text());
                it->setHidden(!match);
                if (match && firstVisible < 0) firstVisible = i;
            }
            if (firstVisible >= 0) {
                list->setCurrentRow(firstVisible, QItemSelectionModel::ClearAndSelect);
                list->scrollToItem(list->item(firstVisible));
            } else {
                list->clearSelection();
            }
        });

    // Enter from search box commits — same as Close button.
    QObject::connect(search, &QLineEdit::returnPressed, &dlg, &QDialog::accept);

    // Let Up/Down etc. drive the list while focus stays in the search box.
    // QListView::moveCursor skips hidden rows automatically, so arrow nav
    // walks the filtered result set without us doing anything extra.
    search->installEventFilter(new ListNavFilter(list, &dlg));
    search->setFocus();

    // Pre-select every currently-visible layer so the dialog opens reflecting
    // the drawing's existing state, not an empty isolation. Signals are blocked
    // because the resulting isolateSet() would be a no-op (every layer freeze
    // state is already correct) but would still fire layerToggled redraws.
    {
        list->blockSignals(true);
        int firstShownRow = -1;
        for (int i = 0; i < list->count(); ++i) {
            const auto it = originalFrozen.constFind(list->item(i)->text());
            const bool visible = it != originalFrozen.cend() && !it.value();
            if (visible) {
                list->item(i)->setSelected(true);
                if (firstShownRow < 0) firstShownRow = i;
            }
        }
        if (firstShownRow >= 0) list->setCurrentRow(firstShownRow, QItemSelectionModel::NoUpdate);
        list->blockSignals(false);
    }

    QObject::connect(btnRestore, &QPushButton::clicked, &dlg, [&]() {
        restoreOriginal();
        list->clearSelection();
    });
    QObject::connect(btnAllOn,   &QPushButton::clicked, &dlg, unfreezeAll);
    QObject::connect(btnClose,   &QPushButton::clicked, &dlg, &QDialog::accept);

    // Double-click an item: isolate just that layer and dismiss the dialog.
    // Single-click already isolates (via itemSelectionChanged); double-click
    // is the "I'm done, commit this pick" shortcut.
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dlg,
        [list, &dlg](QListWidgetItem* item) {
            if (item == nullptr) return;
            list->clearSelection();
            item->setSelected(true);   // fires isolateSet via itemSelectionChanged
            dlg.accept();
        });

    // Ctrl+A: select every currently-visible (non-filtered-out) layer. Same
    // effect as the "Show all" button when the filter is empty, but respects
    // the filter — useful for selecting a fuzzy-matched subset.
    {
        auto* selectAllSc = new QShortcut(QKeySequence::SelectAll, &dlg);
        selectAllSc->setContext(Qt::WindowShortcut);
        QObject::connect(selectAllSc, &QShortcut::activated, &dlg, [list]() {
            list->blockSignals(true);
            list->clearSelection();
            for (int i = 0; i < list->count(); ++i) {
                auto* it = list->item(i);
                if (it != nullptr && !it->isHidden()) {
                    it->setSelected(true);
                }
            }
            list->blockSignals(false);
            // One emit at the end so isolateSet runs once with the full pick.
            emit list->itemSelectionChanged();
        });
    }

    // Anchored to the main window so LayerWalk follows it across monitors.
    LC_Dialog::loadRelativeGeometry(&dlg, "LayerWalk", QSize(320, 440));

    dlg.exec();

    LC_Dialog::saveRelativeGeometry(&dlg, "LayerWalk");

    finish(false);
}

void LC_ActionLayerWalk::init(int status) {
    RS_ActionInterface::init(status);
    trigger();
}
