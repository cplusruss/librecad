#include "lc_actionmodifyarrayrect.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>
#include <vector>

#include "lc_graphicviewport.h"
#include "lc_undosection.h"
#include "qc_applicationwindow.h"
#include "rs_document.h"
#include "rs_entity.h"
#include "rs_entitycontainer.h"
#include "rs_settings.h"

LC_ActionModifyArrayRect::LC_ActionModifyArrayRect(LC_ActionContext* actionContext)
    : RS_ActionInterface("Array", actionContext, RS2::ActionModifyArrayRect) {}

void LC_ActionModifyArrayRect::init(int status) {
    RS_ActionInterface::init(status);
    trigger();
}

namespace {

// Polar method enum mirrors the AutoCAD dialog ordering.
enum class PolarMethod {
    ItemsAndFill   = 0,  // Total items + Angle to fill
    ItemsAndBetween = 1, // Total items + Angle between
    FillAndBetween = 2   // Angle to fill + Angle between
};

constexpr double kDegToRad = M_PI / 180.0;

// Manual clone helpers — adds the clones to `container` (raw, no undo cycle)
// and returns them in a vector so they can be removed later (Cancel) or
// registered as undoables in one batch (OK).

std::vector<RS_Entity*> generateRectangularClones(
    RS_EntityContainer& container,
    const std::vector<RS_Entity*>& originals,
    int rows, int cols,
    double rowGap, double colGap) {
    std::vector<RS_Entity*> added;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (r == 0 && c == 0) continue;
            for (RS_Entity* original : originals) {
                if (original == nullptr) continue;
                RS_Entity* clone = original->clone();
                clone->move(RS_Vector(c * colGap, r * rowGap));
                container.addEntity(clone);
                added.push_back(clone);
            }
        }
    }
    return added;
}

std::vector<RS_Entity*> generatePolarClones(
    RS_EntityContainer& container,
    const std::vector<RS_Entity*>& originals,
    const RS_Vector& center,
    int totalItems,
    double angleBetweenDeg,
    bool rotateItems) {
    std::vector<RS_Entity*> added;
    const int copies = totalItems - 1;
    if (copies < 1) return added;
    const double angleBetweenRad = angleBetweenDeg * kDegToRad;
    for (int num = 1; num <= copies; ++num) {
        const double theta = angleBetweenRad * num;
        const double cosT = std::cos(theta);
        const double sinT = std::sin(theta);
        for (RS_Entity* original : originals) {
            if (original == nullptr) continue;
            RS_Entity* clone = original->clone();
            if (rotateItems) {
                clone->rotate(center, theta);
            } else {
                RS_Vector p = (original->getMin() + original->getMax()) * 0.5;
                RS_Vector d = p - center;
                RS_Vector rotated(center.x + d.x * cosT - d.y * sinT,
                                  center.y + d.x * sinT + d.y * cosT);
                clone->move(rotated - p);
            }
            container.addEntity(clone);
            added.push_back(clone);
        }
    }
    return added;
}

// Strips a set of preview clones out of the container without going through
// the undo system. Each removed entity is also deleted. Caller clears their
// own tracking vector afterwards.
void clearPreviewClones(RS_EntityContainer& container,
                         std::vector<RS_Entity*>& clones) {
    for (RS_Entity* e : clones) {
        if (e == nullptr) continue;
        container.removeEntity(e);  // removeEntity owns the delete
    }
    clones.clear();
}

} // namespace

void LC_ActionModifyArrayRect::trigger() {
    if (m_container == nullptr || m_graphic == nullptr) {
        finish(false);
        return;
    }

    std::vector<RS_Entity*> selected;
    for (RS_Entity* e : *m_container) {
        if (e != nullptr && e->isSelected()) {
            selected.push_back(e);
        }
    }
    if (selected.empty()) {
        m_dialogVisible = true;
        QMessageBox::information(QC_ApplicationWindow::getAppWindow().get(),
                                 tr("Array"),
                                 tr("Select objects before running ARRAY."));
        m_dialogVisible = false;
        finish(false);
        return;
    }

    LC_GROUP_GUARD("Array");

    bool isPolar         = LC_GET_INT("Mode", 0) == 1;
    int  rows            = LC_GET_INT("Rows", 3);
    int  cols            = LC_GET_INT("Cols", 4);
    double rowGap        = LC_GET_INT("RowSpacing", 10);
    double colGap        = LC_GET_INT("ColSpacing", 10);
    double centerX       = LC_GET_INT("PolarCenterX", 0);
    double centerY       = LC_GET_INT("PolarCenterY", 0);
    int  polarMethodIdx  = LC_GET_INT("PolarMethod", 0);
    int  polarItems      = LC_GET_INT("PolarItems", 4);
    double polarFill     = LC_GET_INT("PolarAngleFill", 360);
    double polarBetween  = LC_GET_INT("PolarAngleBetween", 90);
    bool rotateItems     = LC_GET_INT("PolarRotateItems", 1) != 0;
    bool livePreview     = LC_GET_INT("LivePreview", 1) != 0;

    QDialog dlg(QC_ApplicationWindow::getAppWindow().get());
    dlg.setWindowTitle(tr("Array"));

    auto* rbRect  = new QRadioButton(tr("&Rectangular Array"), &dlg);
    auto* rbPolar = new QRadioButton(tr("&Polar Array"), &dlg);
    rbRect->setChecked(!isPolar);
    rbPolar->setChecked(isPolar);
    auto* radioLayout = new QHBoxLayout;
    radioLayout->addWidget(rbRect);
    radioLayout->addWidget(rbPolar);
    radioLayout->addStretch();

    auto* rectGroup = new QGroupBox(tr("Rectangular"), &dlg);
    auto* rowSpin = new QSpinBox(rectGroup);
    rowSpin->setRange(1, 1000);
    rowSpin->setValue(rows);
    auto* colSpin = new QSpinBox(rectGroup);
    colSpin->setRange(1, 1000);
    colSpin->setValue(cols);
    auto* rowGapSpin = new QDoubleSpinBox(rectGroup);
    rowGapSpin->setDecimals(4);
    rowGapSpin->setRange(-1e6, 1e6);
    rowGapSpin->setValue(rowGap);
    auto* colGapSpin = new QDoubleSpinBox(rectGroup);
    colGapSpin->setDecimals(4);
    colGapSpin->setRange(-1e6, 1e6);
    colGapSpin->setValue(colGap);
    auto* rectForm = new QFormLayout(rectGroup);
    rectForm->addRow(tr("Rows:"),               rowSpin);
    rectForm->addRow(tr("Columns:"),            colSpin);
    rectForm->addRow(tr("Row spacing (Y):"),    rowGapSpin);
    rectForm->addRow(tr("Column spacing (X):"), colGapSpin);

    auto* polarGroup = new QGroupBox(tr("Polar"), &dlg);
    auto* centerXSpin = new QDoubleSpinBox(polarGroup);
    centerXSpin->setDecimals(4);
    centerXSpin->setRange(-1e9, 1e9);
    centerXSpin->setValue(centerX);
    auto* centerYSpin = new QDoubleSpinBox(polarGroup);
    centerYSpin->setDecimals(4);
    centerYSpin->setRange(-1e9, 1e9);
    centerYSpin->setValue(centerY);
    auto* centerLayout = new QHBoxLayout;
    centerLayout->addWidget(new QLabel(tr("X:"), polarGroup));
    centerLayout->addWidget(centerXSpin);
    centerLayout->addSpacing(8);
    centerLayout->addWidget(new QLabel(tr("Y:"), polarGroup));
    centerLayout->addWidget(centerYSpin);

    auto* methodCombo = new QComboBox(polarGroup);
    methodCombo->addItem(tr("Total number of items && Angle to fill"));
    methodCombo->addItem(tr("Total number of items && Angle between items"));
    methodCombo->addItem(tr("Angle to fill && Angle between items"));
    methodCombo->setCurrentIndex(polarMethodIdx);

    auto* itemsSpin = new QSpinBox(polarGroup);
    itemsSpin->setRange(1, 10000);
    itemsSpin->setValue(polarItems);

    auto* fillSpin = new QDoubleSpinBox(polarGroup);
    fillSpin->setDecimals(2);
    fillSpin->setRange(-3600.0, 3600.0);
    fillSpin->setValue(polarFill);
    fillSpin->setSuffix(QStringLiteral(" °"));

    auto* betweenSpin = new QDoubleSpinBox(polarGroup);
    betweenSpin->setDecimals(4);
    betweenSpin->setRange(-360.0, 360.0);
    betweenSpin->setValue(polarBetween);
    betweenSpin->setSuffix(QStringLiteral(" °"));

    auto* rotateCheck = new QCheckBox(tr("Rotate items as copied"), polarGroup);
    rotateCheck->setChecked(rotateItems);

    auto* polarForm = new QFormLayout(polarGroup);
    polarForm->addRow(tr("Center point:"),     centerLayout);
    polarForm->addRow(tr("Method:"),           methodCombo);
    polarForm->addRow(tr("Total items:"),      itemsSpin);
    polarForm->addRow(tr("Angle to fill:"),    fillSpin);
    polarForm->addRow(tr("Angle between:"),    betweenSpin);
    polarForm->addRow(rotateCheck);

    auto applyMethodEnable = [&]() {
        const int idx = methodCombo->currentIndex();
        itemsSpin->setEnabled(idx != static_cast<int>(PolarMethod::FillAndBetween));
        fillSpin->setEnabled(idx != static_cast<int>(PolarMethod::ItemsAndBetween));
        betweenSpin->setEnabled(idx != static_cast<int>(PolarMethod::ItemsAndFill));
    };
    applyMethodEnable();
    QObject::connect(methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     &dlg, [&](int){ applyMethodEnable(); });

    auto applyMode = [&]() {
        rectGroup->setVisible(rbRect->isChecked());
        polarGroup->setVisible(rbPolar->isChecked());
        dlg.adjustSize();
    };
    applyMode();
    QObject::connect(rbRect,  &QRadioButton::toggled, &dlg, [&](bool){ applyMode(); });
    QObject::connect(rbPolar, &QRadioButton::toggled, &dlg, [&](bool){ applyMode(); });

    auto* previewCheck = new QCheckBox(tr("Live preview"), &dlg);
    previewCheck->setChecked(livePreview);
    previewCheck->setToolTip(tr(
        "Update the drawing live as you change parameters. The preview is\n"
        "removed if you click Cancel and committed to the undo history if\n"
        "you click OK."));

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* topLayout = new QVBoxLayout(&dlg);
    topLayout->addLayout(radioLayout);
    topLayout->addWidget(rectGroup);
    topLayout->addWidget(polarGroup);
    topLayout->addWidget(previewCheck);
    topLayout->addWidget(buttons);

    // -- Preview machinery ----------------------------------------------------
    //
    // Entities added during the live preview are tracked here. They're added
    // to the container directly (no undo registration) so they appear and can
    // be torn down cheaply on every parameter change. On Cancel we remove
    // them; on OK we register them as undoables in a single batch so one
    // Ctrl+Z reverts the whole array.
    std::vector<RS_Entity*> previewClones;

    auto resolvePolarParams = [&](int& outItems, double& outBetween) -> bool {
        int    items   = itemsSpin->value();
        double fill    = fillSpin->value();
        double between = betweenSpin->value();
        switch (static_cast<PolarMethod>(methodCombo->currentIndex())) {
            case PolarMethod::ItemsAndFill:
                if (items < 1) return false;
                between = fill / static_cast<double>(items);
                break;
            case PolarMethod::ItemsAndBetween:
                if (items < 1) return false;
                break;
            case PolarMethod::FillAndBetween:
                if (std::abs(between) < 1e-9) return false;
                items = static_cast<int>(std::ceil(std::abs(fill / between)));
                if (items < 1) items = 1;
                break;
        }
        if (items < 2) return false;
        outItems   = items;
        outBetween = between;
        return true;
    };

    auto refreshPreview = [&]() {
        clearPreviewClones(*m_container, previewClones);
        if (!previewCheck->isChecked()) {
            m_viewport->notifyChanged();
            redrawDrawing();
            return;
        }
        if (rbPolar->isChecked()) {
            int items; double between;
            if (!resolvePolarParams(items, between)) {
                m_viewport->notifyChanged();
                redrawDrawing();
                return;
            }
            const RS_Vector center(centerXSpin->value(), centerYSpin->value());
            previewClones = generatePolarClones(*m_container, selected, center,
                                                  items, between, rotateCheck->isChecked());
        } else {
            const int    r  = rowSpin->value();
            const int    c  = colSpin->value();
            const double rg = rowGapSpin->value();
            const double cg = colGapSpin->value();
            if (r < 1 || c < 1 || (r == 1 && c == 1)) {
                m_viewport->notifyChanged();
                redrawDrawing();
                return;
            }
            previewClones = generateRectangularClones(*m_container, selected, r, c, rg, cg);
        }
        m_viewport->notifyChanged();
        redrawDrawing();
    };

    // Connect everything that affects the array to the preview refresh. The
    // earlier handlers (applyMode / applyMethodEnable) still run first via
    // the toggled / currentIndexChanged signals; we just chain a refresh.
    QObject::connect(previewCheck, &QCheckBox::toggled, &dlg, [&](bool) { refreshPreview(); });
    QObject::connect(rbRect,       &QRadioButton::toggled, &dlg, [&](bool) { refreshPreview(); });
    QObject::connect(rbPolar,      &QRadioButton::toggled, &dlg, [&](bool) { refreshPreview(); });
    QObject::connect(rowSpin,      QOverload<int>::of(&QSpinBox::valueChanged),
                     &dlg, [&](int){ refreshPreview(); });
    QObject::connect(colSpin,      QOverload<int>::of(&QSpinBox::valueChanged),
                     &dlg, [&](int){ refreshPreview(); });
    QObject::connect(rowGapSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [&](double){ refreshPreview(); });
    QObject::connect(colGapSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [&](double){ refreshPreview(); });
    QObject::connect(centerXSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [&](double){ refreshPreview(); });
    QObject::connect(centerYSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [&](double){ refreshPreview(); });
    QObject::connect(methodCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
                     &dlg, [&](int){ refreshPreview(); });
    QObject::connect(itemsSpin,    QOverload<int>::of(&QSpinBox::valueChanged),
                     &dlg, [&](int){ refreshPreview(); });
    QObject::connect(fillSpin,     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [&](double){ refreshPreview(); });
    QObject::connect(betweenSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [&](double){ refreshPreview(); });
    QObject::connect(rotateCheck,  &QCheckBox::toggled, &dlg, [&](bool){ refreshPreview(); });

    // Seed the preview with the persisted last-used values.
    refreshPreview();

    m_dialogVisible = true;
    // Show non-modally and wait on a local event loop instead of exec(): exec()
    // is application-modal, which blocks pan/zoom on the drawing so the user
    // can't navigate to the points they need while the dialog is open.
    dlg.setModal(false);
    dlg.show();
    dlg.raise();
    QEventLoop dlgLoop;
    QObject::connect(&dlg, &QDialog::finished, &dlgLoop, &QEventLoop::quit);
    dlgLoop.exec();
    const int result = dlg.result();
    m_dialogVisible = false;

    // Persist every field for next time, regardless of outcome.
    const bool polarSelected = rbPolar->isChecked();
    LC_SET("Mode", polarSelected ? 1 : 0);
    LC_SET("Rows", rowSpin->value());
    LC_SET("Cols", colSpin->value());
    LC_SET("RowSpacing", rowGapSpin->value());
    LC_SET("ColSpacing", colGapSpin->value());
    LC_SET("PolarCenterX", centerXSpin->value());
    LC_SET("PolarCenterY", centerYSpin->value());
    LC_SET("PolarMethod", methodCombo->currentIndex());
    LC_SET("PolarItems", itemsSpin->value());
    LC_SET("PolarAngleFill", fillSpin->value());
    LC_SET("PolarAngleBetween", betweenSpin->value());
    LC_SET("PolarRotateItems", rotateCheck->isChecked() ? 1 : 0);
    LC_SET("LivePreview", previewCheck->isChecked() ? 1 : 0);

    if (result != QDialog::Accepted) {
        // Cancel: tear down whatever preview is on screen and exit.
        clearPreviewClones(*m_container, previewClones);
        m_viewport->notifyChanged();
        redrawDrawing();
        finish(false);
        return;
    }

    // OK path. If live preview was on, the entities are already in the
    // container; we just need to register them as undoables so one Ctrl+Z
    // reverts the whole array. If live preview was off, generate them now.
    if (previewClones.empty()) {
        if (polarSelected) {
            int items; double between;
            if (!resolvePolarParams(items, between)) {
                finish(false);
                return;
            }
            const RS_Vector center(centerXSpin->value(), centerYSpin->value());
            previewClones = generatePolarClones(*m_container, selected, center,
                                                  items, between, rotateCheck->isChecked());
        } else {
            const int    r  = rowSpin->value();
            const int    c  = colSpin->value();
            const double rg = rowGapSpin->value();
            const double cg = colGapSpin->value();
            if (r < 1 || c < 1 || (r == 1 && c == 1)) {
                finish(false);
                return;
            }
            previewClones = generateRectangularClones(*m_container, selected, r, c, rg, cg);
        }
    }

    // Single batched undo cycle. addUndoable() on each clone is what lets
    // RS_Undo find them on Ctrl+Z. m_document is non-null at this point
    // because m_container has a graphic.
    if (!previewClones.empty()) {
        LC_UndoSection undo(m_document, m_viewport, /*handleUndo=*/true);
        for (RS_Entity* e : previewClones) {
            if (e != nullptr) undo.addUndoable(e);
        }
    }

    m_viewport->notifyChanged();
    redrawDrawing();
    finish(false);
}
