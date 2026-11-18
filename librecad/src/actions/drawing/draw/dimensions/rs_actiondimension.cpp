/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
** This copyright notice MUST APPEAR in all copies of the script!
**
**********************************************************************/
#include "rs_actiondimension.h"

#include <algorithm>
#include <cmath>

#include "lc_dimstyle.h"
#include "lc_dimstyleslist.h"
#include "lc_graphicviewport.h"
#include "qg_dimoptions.h"
#include "rs_debug.h"
#include "rs_dialogfactory.h"
#include "rs_dialogfactoryinterface.h"
#include "rs_dimension.h"
#include "rs_entity.h"
#include "rs_entitycontainer.h"
#include "rs_graphic.h"
#include "rs_mtext.h"
#include "rs_settings.h"

namespace {
    const QString g_radialPrefix=QObject::tr("R", "Radial dimension prefix");
}

RS_ActionDimension::RS_ActionDimension(const char *name,LC_ActionContext *actionContext, RS2::EntityType dimType,  RS2::ActionType actionType)
    :RS_PreviewActionInterface(name,actionContext, actionType),  m_dimTypeToCreate {dimType}{
    reset();
    readSettings();
}

RS_ActionDimension::~RS_ActionDimension() = default;

void RS_ActionDimension::reset(){
    // auto dimStyleName = "Standard";
    auto dimStyleName = m_graphic->getDefaultDimStyleName();
    m_dimensionData = std::make_unique<RS_DimensionData>(RS_Vector(false),
                                                         RS_Vector(false),
                                                         RS_MTextData::VAMiddle,
                                                         RS_MTextData::HACenter,
                                                         RS_MTextData::Exact,
                                                         1.0,"",dimStyleName,0.0,0.0,
                                                         true,nullptr,false,
                                                         false);
    setDimStyleName(dimStyleName);
    m_diameter = false;
}

void RS_ActionDimension::init(int status){
    RS_PreviewActionInterface::init(status);
    autoGuessDimScaleIfFirstDim();
//reset();
}

void RS_ActionDimension::autoGuessDimScaleIfFirstDim() {
    // Required inputs. If anything is missing, bail silently — the user gets
    // whatever DIMSCALE the drawing already has, which is the safe default.
    if (m_container == nullptr || m_graphic == nullptr) return;

    // Skip if there's already a dimension on the drawing. Honors any value
    // the user (or an imported DXF) has already set up.
    for (RS_Entity* e : *m_container) {
        if (e != nullptr && RS2::isDimensionalEntity(e->rtti())) return;
    }

    // Unscaled dim-text height in drawing units. Header $DIMTXT is always
    // populated by rs_graphic.cpp's defaults (0.1 inch / 2.5 mm).
    const double dimtxt = m_graphic->getVariableDouble(QStringLiteral("$DIMTXT"), 2.5);
    if (!(dimtxt > 0.0)) return;

    // Heuristic: aim for dim text ~0.08% of the drawing's longest extent.
    // Drafters often dim small features (notches, holes, fillets) that are
    // a fraction of a percent of the overall geometry, so text/arrows need
    // to be small in absolute terms even when the overall extent is huge.
    //
    // Zoom-invariant — same answer whether you fit-to-view or zoomed in to
    // the feature you're dimensioning.
    //
    // Empty / degenerate drawings (no geometry) fall back to a viewport-based
    // target so the on-screen size at least makes sense.
    constexpr double kTextFractionOfExtent = 0.0008;
    constexpr double kFallbackTargetPixels = 8.0;

    double raw = 0.0;
    const RS_Vector size = m_container->getSize();
    const double extent = std::max(std::abs(size.x), std::abs(size.y));
    if (extent > 0.0 && std::isfinite(extent)) {
        raw = (extent * kTextFractionOfExtent) / dimtxt;
    } else if (m_viewport != nullptr) {
        const RS_Vector factor = m_viewport->getFactor();
        if (!(factor.x > 0.0) || !std::isfinite(factor.x)) return;
        raw = kFallbackTargetPixels / (dimtxt * factor.x);
    } else {
        return;
    }
    if (!(raw > 0.0) || !std::isfinite(raw)) return;

    // Snap DOWN to the 1-2-5 series. Biases smaller; text overflowing the dim
    // line is uglier than text that's a touch small.
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double n   = raw / mag;
    const double nice = (n < 2.0) ? 1.0 : (n < 5.0) ? 2.0 : 5.0;
    const double scale = std::clamp(nice * mag, 0.0001, 100000.0);

    // Write to every style + the header variable — same pattern as
    // LC_ActionModifyDimScale::applyScaleToAllStyles.
    LC_DimStylesList* styles = m_graphic->getDimStyleList();
    if (styles != nullptr) {
        const QList<LC_DimStyle*>* list = styles->getStylesList();
        if (list != nullptr) {
            for (LC_DimStyle* s : *list) {
                if (s == nullptr || s->scaling() == nullptr) continue;
                s->scaling()->setScale(scale);
                s->scaling()->setFlag(LC_DimStyle::Scaling::$DIMSCALE);
            }
        }
        styles->setModified(true);
    }
    m_graphic->addVariable(QStringLiteral("$DIMSCALE"), scale, 40);
    m_graphic->setModified(true);

    // Tell the user what happened so a too-big/too-small guess is debuggable
    // without spelunking. Goes to the command line; ignored if no dialog
    // factory is wired up (e.g. tests).
    if (RS_DIALOGFACTORY != nullptr) {
        RS_DIALOGFACTORY->commandMessage(
            tr("Auto-set DIMSCALE to %1 (drawing extent %2; first dimension). "
               "Use 'dimscale' to override.")
                .arg(scale, 0, 'g', 4)
                .arg(extent, 0, 'g', 4));
    }
}


RS2::CursorType RS_ActionDimension::doGetMouseCursor([[maybe_unused]] int status){
    return RS2::SelectCursor;
}

bool RS_ActionDimension::isDimensionAction(RS2::ActionType type){
    switch (type) {
        case RS2::ActionDimAligned:
        case RS2::ActionDimLinear:
        case RS2::ActionDimOrdinate:
        case RS2::ActionDimLinearVer:
        case RS2::ActionDimLinearHor:
        case RS2::ActionDimLinearAuto:
        case RS2::ActionDimAngular:
        case RS2::ActionDimDiametric:
        case RS2::ActionDimRadial:
        case RS2::ActionDimArc:
        case RS2::ActionDimBaseline:
        case RS2::ActionDimContinue:
            return true;
        default:
            return false;
    }
}

QString RS_ActionDimension::getText() const{
    if (!m_dimensionData->text.isEmpty()) {
        return m_dimensionData->text;
    }

    QString l = m_label;

    if (l.isEmpty() && (m_diameter == true || !m_tol1.isEmpty() || !m_tol2.isEmpty())) {
        l = "<>";
    }

    if (m_diameter) {
        if (rtti() == RS2::ActionDimRadial && !l.startsWith(g_radialPrefix)) {
            l = g_radialPrefix + l;
        }
        else if (l.at(0) != QChar(0x2205)) {
            l = QChar(0x2205) + l;
        }
    }
    else if (l.startsWith({QChar(0x2205)})) {
        l = l.mid(1);
    }
    else if (l.startsWith(g_radialPrefix)) {
        l = l.mid(g_radialPrefix.length());
    }

    if (!m_tol1.isEmpty() || !m_tol2.isEmpty()) {
        l += QString("\\S%1\\%2;").arg(m_tol1, m_tol2);
    }

    return l;
}

void RS_ActionDimension::setText(const QString &t){
    m_dimensionData->text = t;
}

const QString &RS_ActionDimension::getLabel() const{
    return m_label;
}

void RS_ActionDimension::setLabel(const QString &t){
//data->text = t;
    m_label = t;
}

const QString &RS_ActionDimension::getTol1() const{
    return m_tol1;
}

void RS_ActionDimension::setTol1(const QString &t){
    m_tol1 = t;
}

const QString &RS_ActionDimension::getTol2() const{
    return m_tol2;
}

void RS_ActionDimension::setTol2(const QString &t){
    m_tol2 = t;
}

bool RS_ActionDimension::getDiameter() const{
    return m_diameter;
}

void RS_ActionDimension::setDiameter(bool d){
    m_diameter = d;
}

LC_ActionOptionsWidget* RS_ActionDimension::createOptionsWidget(){
    return new QG_DimOptions(m_graphic);
}

// FIXME - sand - REWORK
void RS_ActionDimension::readSettings() {
    m_previewShowsFullDimension = LC_GET_ONE_BOOL("Appearance", "PreviewFullDimOnExt2", true);
}

void RS_ActionDimension::resume() {
    RS_PreviewActionInterface::resume();
    readSettings();
}

void RS_ActionDimension::setDimStyleName(const QString& styleName) {
    LC_DimStylesList* stylesList = m_graphic->getDimStyleList();
    auto typeSpecificDimStyle = stylesList->findByBaseNameAndType(styleName, m_dimTypeToCreate);
    if (typeSpecificDimStyle != nullptr) {
        m_dimensionData->style = typeSpecificDimStyle->getName();
    }
    else {
        m_dimensionData->style = styleName;
    }
}

QString RS_ActionDimension::getDimStyleName() {
    return m_dimensionData->style;
}
