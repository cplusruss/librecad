#include "lc_actionmodifydimscale.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

#include "lc_dimstyle.h"
#include "lc_dimstyleslist.h"
#include "lc_graphicviewport.h"
#include "qc_applicationwindow.h"
#include "rs_dimension.h"
#include "rs_entity.h"
#include "rs_entitycontainer.h"
#include "rs_graphic.h"
#include "rs_graphicview.h"

LC_ActionModifyDimScale::LC_ActionModifyDimScale(LC_ActionContext* actionContext)
    : RS_ActionInterface("DimScale", actionContext, RS2::ActionModifyDimScale) {}

void LC_ActionModifyDimScale::init(int status) {
    RS_ActionInterface::init(status);
    trigger();
}

void LC_ActionModifyDimScale::applyScaleToAllStyles(double scale) {
    if (m_graphic == nullptr) return;
    LC_DimStylesList* styles = m_graphic->getDimStyleList();
    if (styles != nullptr) {
        const QList<LC_DimStyle*>* list = styles->getStylesList();
        if (list != nullptr) {
            for (LC_DimStyle* s : *list) {
                if (s == nullptr || s->scaling() == nullptr) continue;
                s->scaling()->setScale(scale);
                // Force the modify bit on. setScale's internal checkModified
                // only sets the bit when new != current; that's fine the first
                // time but breaks the round-trip: on reload the flag is clean,
                // and a subsequent user-driven 1.0 -> 1.0 confirmation doesn't
                // flip it, so DIMSCALE never gets written and the saved value
                // is silently dropped. Unconditional setFlag is harmless when
                // the value actually changed and necessary when it didn't.
                s->scaling()->setFlag(LC_DimStyle::Scaling::$DIMSCALE);
            }
        }
        // Dimensions with no matching named/default style resolve to the
        // vars-derived fallback style (getFallbackDimStyleFromVars). That style
        // lives OUTSIDE getStylesList() and is cached — the loop above never
        // reaches it, and the $DIMSCALE header var written below does not
        // refresh the cached copy at runtime. So those dimensions kept reading
        // the old scale and never resized. Update the fallback directly.
        LC_DimStyle* fallback = styles->getFallbackDimStyleFromVars();
        if (fallback != nullptr && fallback->scaling() != nullptr) {
            fallback->scaling()->setScale(scale);
            fallback->scaling()->setFlag(LC_DimStyle::Scaling::$DIMSCALE);
        }
        styles->setModified(true);
    }
    // Header $DIMSCALE is also written so external readers / pre-table-aware
    // pipelines pick up the scale even when they don't honor the DIMSTYLE
    // table entries. Belt and braces.
    m_graphic->addVariable(QStringLiteral("$DIMSCALE"), scale, 40);
    m_graphic->setModified(true);
}

void LC_ActionModifyDimScale::refreshAllDimensions() {
    if (m_container == nullptr) return;
    for (RS_Entity* e : *m_container) {
        if (e == nullptr) continue;
        if (RS2::isDimensionalEntity(e->rtti())) {
            e->update();
        }
    }
    if (m_viewport != nullptr) {
        m_viewport->notifyChanged();
    }
    redrawDrawing();
}

void LC_ActionModifyDimScale::trigger() {
    if (m_container == nullptr || m_graphic == nullptr) {
        finish(false);
        return;
    }

    // Read the current DIMSCALE from the first available style so the dialog
    // opens with a sensible starting value. If no styles exist we default to 1.
    double currentScale = 1.0;
    LC_DimStylesList* styles = m_graphic->getDimStyleList();
    if (styles != nullptr) {
        const QList<LC_DimStyle*>* list = styles->getStylesList();
        if (list != nullptr) {
            for (LC_DimStyle* s : *list) {
                if (s != nullptr && s->scaling() != nullptr) {
                    currentScale = s->scaling()->scale();
                    break;
                }
            }
        }
    }
    const double originalScale = currentScale;

    QDialog dlg(QC_ApplicationWindow::getAppWindow().get());
    dlg.setWindowTitle(tr("DIMSCALE"));

    auto* spin = new QDoubleSpinBox(&dlg);
    spin->setDecimals(4);
    // Wide range to cover anything from micron-scale electronics drawings up
    // to building-scale plans. The dim-text floor/ceiling clamp at draw time
    // keeps text visible even at extreme values, so an aggressive DIMSCALE
    // no longer traps you into an invisible drawing.
    spin->setRange(0.0001, 100000.0);
    spin->setSingleStep(0.1);
    spin->setValue(currentScale);
    spin->setKeyboardTracking(true);
    spin->setMinimumWidth(140);

    auto* currentLabel = new QLabel(
        tr("Current value in the drawing: <b>%1</b>").arg(currentScale, 0, 'f', 3),
        &dlg);

    auto* hint = new QLabel(
        tr("Global dimension scale factor. Applies to every dimension —\n"
           "arrowhead size, text height, gaps. Live preview as you change\n"
           "the value. If your dim text has gone invisible, click Reset."),
        &dlg);
    hint->setWordWrap(true);

    auto* resetBtn = new QPushButton(tr("Reset to 1.0"), &dlg);
    QObject::connect(resetBtn, &QPushButton::clicked, &dlg, [spin]() {
        spin->setValue(1.0);
    });

    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("Scale:"), &dlg));
    row->addWidget(spin);
    row->addWidget(resetBtn);
    row->addStretch();

    // Log-scaled slider spanning the full dialog range. Linear would give all
    // of [0.0001, 1] one pixel and all of [1, 100000] the rest — useless. Log
    // gives even pixel coverage per decade.
    //   slider value (0..N) ↔ log10(scale): pos / N = (log10(s) − lo) / (hi − lo)
    const int    kSliderSteps = 1000;
    const double kLogLo = -4.0;   // log10(0.0001)
    const double kLogHi =  5.0;   // log10(100000)
    auto* slider = new QSlider(Qt::Horizontal, &dlg);
    slider->setRange(0, kSliderSteps);
    auto valueToSliderPos = [kSliderSteps, kLogLo, kLogHi](double v) {
        if (v <= 0.0) return 0;
        const double logv = std::log10(v);
        const double t    = (logv - kLogLo) / (kLogHi - kLogLo);
        return std::clamp(int(std::round(t * kSliderSteps)), 0, kSliderSteps);
    };
    auto sliderPosToValue = [kSliderSteps, kLogLo, kLogHi](int pos) {
        const double t    = double(pos) / double(kSliderSteps);
        const double logv = kLogLo + t * (kLogHi - kLogLo);
        return std::pow(10.0, logv);
    };
    slider->setValue(valueToSliderPos(currentScale));

    // Show the range at each end of the slider so the user knows what's
    // there at the extremes.
    auto* sliderRow = new QHBoxLayout;
    sliderRow->addWidget(new QLabel(
        tr("min %1").arg(std::pow(10.0, kLogLo), 0, 'g', 3), &dlg));
    sliderRow->addWidget(slider, /*stretch=*/1);
    sliderRow->addWidget(new QLabel(
        tr("max %1").arg(std::pow(10.0, kLogHi), 0, 'g', 3), &dlg));

    // Bidirectional sync with reentrancy guards: slider → spin → live update,
    // and spin (or Reset) → slider. blockSignals avoids the feedback loop.
    QObject::connect(slider, &QSlider::valueChanged, &dlg, [&, sliderPosToValue](int pos) {
        const double v = sliderPosToValue(pos);
        if (std::abs(v - spin->value()) > 1e-9) {
            spin->blockSignals(true);
            spin->setValue(v);
            spin->blockSignals(false);
            applyScaleToAllStyles(v);
            refreshAllDimensions();
        }
    });
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [&, valueToSliderPos](double v) {
        const int pos = valueToSliderPos(v);
        if (pos != slider->value()) {
            slider->blockSignals(true);
            slider->setValue(pos);
            slider->blockSignals(false);
        }
    });

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     &dlg, [this](double v) {
        applyScaleToAllStyles(v);
        refreshAllDimensions();
    });

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(hint);
    layout->addWidget(currentLabel);
    layout->addLayout(row);
    layout->addLayout(sliderRow);
    layout->addWidget(buttons);
    dlg.setMinimumWidth(460);

    m_dialogVisible = true;
    int result = dlg.exec();
    m_dialogVisible = false;

    if (result != QDialog::Accepted) {
        // Revert to whatever scale we read on entry. The spin box may have
        // pushed several intermediate values; this one snap brings them all
        // back atomically.
        applyScaleToAllStyles(originalScale);
        refreshAllDimensions();
    }

    finish(false);
}
