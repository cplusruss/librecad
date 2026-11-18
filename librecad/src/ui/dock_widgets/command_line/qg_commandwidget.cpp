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

#include "qg_commandwidget.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPalette>

#include "lc_dynamicinput.h"
#include "qc_applicationwindow.h"
#include "qg_actionhandler.h"
#include "rs_commands.h"
#include "rs_settings.h"
/*
 *  Constructs a QG_CommandWidget as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
QG_CommandWidget::QG_CommandWidget(QG_ActionHandler* action_handler, QWidget* parent, const char* name,
                                   Qt::WindowFlags fl)
    : QWidget(parent, fl)
      , m_actionHandler(action_handler) {
    setObjectName(name);
    setupUi(this);
    connect(leCommand, &QG_CommandEdit::command, this, &QG_CommandWidget::handleCommand);
    connect(leCommand, &QG_CommandEdit::escape, this, &QG_CommandWidget::escape);
    connect(leCommand, &QG_CommandEdit::focusOut, this, &QG_CommandWidget::setNormalMode);
    connect(leCommand, &QG_CommandEdit::focusIn, this, &QG_CommandWidget::setCommandMode);
    connect(leCommand, &QG_CommandEdit::tabPressed, this, &QG_CommandWidget::tabPressed);
    // User-driven edits invalidate any in-progress Tab cycle so the next Tab
    // starts a fresh prefix scan. textEdited fires only on real keystrokes,
    // not on the programmatic setText() the cycle issues itself.
    connect(leCommand, &QLineEdit::textEdited, this, [this](const QString&) {
        m_tabCycle.clear();
        m_tabCycleIdx = -1;
        m_tabCycleStem.clear();
    });
    connect(leCommand, &QG_CommandEdit::clearCommandsHistory, teHistory, &QG_CommandHistory::clear);
    connect(leCommand, &QG_CommandEdit::message, this, &QG_CommandWidget::appendHistory);
    connect(leCommand, &QG_CommandEdit::keycode, this, &QG_CommandWidget::handleKeycode);

    // Mirror live typing into the cursor-side dynamic-input overlay. Uses the
    // QLineEdit base signal so we see every keystroke, not just on Enter.
    connect(leCommand, &QLineEdit::textChanged,
            this, [](const QString& t) {
        LC_DynamicInput::instance().setTyped(t);
    });

    auto a1 = new QAction(QObject::tr("Keycode mode"), this);
    a1->setObjectName("keycode_action");
    a1->setCheckable(true);
    connect(a1, &QAction::toggled, this, &QG_CommandWidget::setKeycodeMode);
    options_button->addAction(a1);

    if (LC_GET_ONE_BOOL("Widgets", "KeycodeMode", false)) {
        leCommand->m_keycode_mode = true;
        a1->setChecked(true);
    }

    auto a2 = new QAction(QObject::tr("Load command file"), this);
    connect(a2, &QAction::triggered, this, &QG_CommandWidget::chooseCommandFile);
    options_button->addAction(a2);

    auto a3 = new QAction(QObject::tr("Paste multiple commands"), this);
    connect(a3, &QAction::triggered, leCommand, &QG_CommandEdit::modifiedPaste);
    options_button->addAction(a3);

    options_button->setStyleSheet("QToolButton::menu-indicator { image: none; }");

    // For convenience of re-docking a floating command widget. Without this button,
    // the title bar may not have a "dock" button.
    // The m_docking button allows user to re-dock the command widget
    m_docking = new QAction(tr("Dock"), this);
    addAction(m_docking);
    connect(m_docking, &QAction::triggered, this, &QG_CommandWidget::dockingButtonTriggered);

    options_button->addAction(m_docking);

    // Apply persisted appearance now, and re-apply whenever the settings
    // dialog hits OK / Apply (RS_Settings emits optionsChanged on save).
    applyAppearance();
    connect(RS_SETTINGS, &RS_Settings::optionsChanged,
            this, &QG_CommandWidget::applyAppearance);
}

/**
 * Re-read command-line appearance settings (Appearance group) and push them
 * into the input line edit AND the history pane so both stay visually in sync.
 *
 * Colors are applied via per-widget stylesheets, NOT via QPalette - LibreCAD
 * loads a global QSS through LC_CustomStyleHelper, and Qt rule is "stylesheet
 * beats palette." A widget-level stylesheet has the highest specificity, so
 * it overrides both the global QSS and the system theme. An empty color
 * string clears the stylesheet on that widget, falling back to the global /
 * system look.
 */
void QG_CommandWidget::applyAppearance() {
    LC_GROUP_GUARD("Appearance");
    const QString bgStr = LC_GET_STR("CommandLineBackgroundColor", QString{});
    const QString fgStr = LC_GET_STR("CommandLineForegroundColor", QString{});
    const QString family = LC_GET_STR("CommandLineFontName",
                                      QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    const int  size   = LC_GET_INT("CommandLineFontSize", 9);
    const bool bold   = LC_GET_BOOL("CommandLineFontBold",   false);
    const bool italic = LC_GET_BOOL("CommandLineFontItalic", false);

    QFont font(family);
    font.setPointSize(size);
    font.setBold(bold);
    font.setItalic(italic);
    if (leCommand != nullptr) leCommand->setFont(font);
    if (teHistory != nullptr) teHistory->setFont(font);

    // Build a SCOPED stylesheet so we only restyle the text area, not the
    // child widgets (notably QScrollBar in the QTextEdit). A naked
    // `background-color: ...;` cascades to all descendants; using class
    // selectors limits the match to the text widget itself. We additionally
    // reset QScrollBar back to the system palette as a belt-and-braces
    // defense against Qt style implementations that propagate background
    // through frames.
    const bool bgValid = QColor::fromString(bgStr).isValid();
    const bool fgValid = QColor::fromString(fgStr).isValid();

    auto buildCss = [&](const QString& selector) {
        QStringList decls;
        if (bgValid) decls << QStringLiteral("background-color: %1;").arg(bgStr);
        if (fgValid) decls << QStringLiteral("color: %1;").arg(fgStr);
        if (decls.isEmpty()) return QString{};
        QString css = QStringLiteral("%1 { %2 }\n")
            .arg(selector, decls.join(QChar(' ')));
        // Keep scrollbars + the corner widget on the system theme.
        css += QStringLiteral(
            "QScrollBar, QScrollBar::add-line, QScrollBar::sub-line, "
            "QScrollBar::handle, QAbstractScrollArea::corner "
            "{ background: palette(window); color: palette(window-text); }\n");
        return css;
    };

    // Empty CSS clears the widget stylesheet, restoring the global look.
    if (leCommand != nullptr) leCommand->setStyleSheet(buildCss(QStringLiteral("QLineEdit")));
    if (teHistory != nullptr) teHistory->setStyleSheet(buildCss(QStringLiteral("QTextEdit")));
}

/*
 *  Destroys the object and frees any allocated resources
 */
QG_CommandWidget::~QG_CommandWidget() {
    auto action = findChild<QAction*>("keycode_action");
    LC_SET_ONE("Widgets", "KeycodeMode", action->isChecked());
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void QG_CommandWidget::languageChange() {
    retranslateUi(this);
}

bool QG_CommandWidget::eventFilter(QObject*/*obj*/, QEvent* event) {
    if (event != nullptr && event->type() == QEvent::KeyPress) {
        auto e = static_cast<QKeyEvent*>(event);

        int key{e->key()};
        switch (key) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (!leCommand->text().size())
                    return false;
                else
                    break;
            case Qt::Key_Escape:
                return false;
            case Qt::Key_Space:
                // AutoCAD: space is Enter. While the command line is empty it must
                // reach the running action instead of being typed as a character.
                if (!leCommand->text().size())
                    return false;
                break;
            default:
                break;
        }

        //detect Ctl- Alt- modifier, but not Shift
        //This should avoid filtering shortcuts, such as Ctl-C
        Qt::KeyboardModifiers modifiers{e->modifiers()};
        if (!(Qt::GroupSwitchModifier == modifiers && Qt::Key_At == key) // let '@' key pass for relative coords
            && modifiers != Qt::KeypadModifier && modifiers & (Qt::KeyboardModifierMask ^ Qt::ShiftModifier) & (Qt::KeyboardModifierMask ^ Qt::ControlModifier)) {
            return false;
        }
        bool isGraphicViewEvent = (key != Qt::Key_Shift) &&
            (key != Qt::Key_Control) &&
            (key != Qt::Key_Up) &&
            (key != Qt::Key_Left) &&
            (key != Qt::Key_Right) &&
            (key != Qt::Key_Down) &&
            (key != Qt::Key_Plus) &&
            (key != Qt::Key_Minus);
        // prevent focus for graphic-view specific keys
        if (isGraphicViewEvent) {
            setFocus();
            QApplication::postEvent(leCommand, e->clone());
            e->accept();
            return true;
        }
        else {
            auto eventClone = e->clone();
            QApplication::postEvent(QC_ApplicationWindow::getAppWindow().get(), eventClone);
            if (!eventClone->isAccepted()) {
                QApplication::postEvent(leCommand, e->clone());
            }
            e->accept();
            return true;
        }
    }

    return false;
}

void QG_CommandWidget::setFocus() {
    if (!isActiveWindow())
        activateWindow();

    auto newEvent = new QFocusEvent(QEvent::FocusIn);
    QApplication::postEvent(leCommand, newEvent);
    leCommand->setFocus();
}

void QG_CommandWidget::setCommand(const QString& cmd) {
    // The "Command:" context label was removed; the command line's own text
    // (mirrored next to the cursor via LC_DynamicInput) is the feedback now.
    Q_UNUSED(cmd)
}

void QG_CommandWidget::setInput(const QString& cmd) {
    leCommand->setText(cmd);
    leCommand->setFocus();
}

void QG_CommandWidget::appendHistory(const QString& msg) {
    teHistory->append(msg);
}

void QG_CommandWidget::handleCommand(QString cmd) {
    cmd = cmd.simplified();
    bool isAction = false;
    if (!cmd.isEmpty()) {
        appendHistory(cmd);
    }

    if (m_actionHandler) {
        isAction = m_actionHandler->command(cmd);
    }

    if (!isAction && !(cmd.contains(',') || cmd.at(0) == '@')) {
        appendHistory(tr("Unknown command: %1").arg(cmd));
    }

    leCommand->setText("");
}

// fixme - review ouptput to command widget
//fixme - add generic help command (as TAB for empy)

void QG_CommandWidget::tabPressed() {
    if (m_actionHandler == nullptr) return;

    const QString currentText = leCommand->text();

    // Two modes:
    //  1. CYCLE continuation - line edit holds one of the entries from the
    //     previously-computed match list; advance to the next entry and
    //     replace the line edit text in place. Wraps around at end.
    //  2. NEW cycle - either no cycle is active or the user has edited the
    //     line since the last Tab. Compute matches for the current prefix
    //     and seed the cycle. If there's exactly one match, just commit it
    //     and let the cycle stay armed for re-triggering.
    const bool continuing = !m_tabCycle.isEmpty()
                            && m_tabCycleIdx >= 0
                            && m_tabCycleIdx < m_tabCycle.size()
                            && currentText.compare(m_tabCycle.at(m_tabCycleIdx),
                                                   Qt::CaseInsensitive) == 0;

    if (continuing) {
        m_tabCycleIdx = (m_tabCycleIdx + 1) % m_tabCycle.size();
        leCommand->blockSignals(true);
        leCommand->setText(m_tabCycle.at(m_tabCycleIdx));
        leCommand->blockSignals(false);
        return;
    }

    // ---- new cycle ----
    m_tabCycleStem = currentText;
    QStringList choices = m_actionHandler->getAvailableCommands();
    if (choices.empty()) {
        choices = RS_COMMANDS->complete(m_tabCycleStem);
    }

    QStringList matches;
    std::copy_if(choices.cbegin(), choices.cend(), std::back_inserter(matches),
                 [this](const QString& cmd) {
                     return m_tabCycleStem.isEmpty()
                         || cmd.startsWith(m_tabCycleStem, Qt::CaseInsensitive);
                 });

    if (matches.isEmpty()) {
        m_tabCycle.clear();
        m_tabCycleIdx = -1;
        return;
    }

    if (matches.count() == 1) {
        // Single match: commit it AND arm the cycle so the next Tab from
        // the same prefix wouldn't surprise the user (no-op cycle of size 1).
        m_tabCycle = matches;
        m_tabCycleIdx = 0;
        leCommand->blockSignals(true);
        leCommand->setText(matches.first());
        leCommand->blockSignals(false);
        return;
    }

    // Multiple matches: show the list once (for visibility) and put the
    // FIRST match into the line edit. Subsequent Tab presses cycle.
    appendHistory(matches.join(QStringLiteral(", ")));
    const QString aliasFile = RS_Commands::getAliasFile();
    if (!aliasFile.isEmpty())
        appendHistory(tr("Command Alias File: %1").arg(aliasFile));

    m_tabCycle    = matches;
    m_tabCycleIdx = 0;
    leCommand->blockSignals(true);
    leCommand->setText(matches.first());
    leCommand->blockSignals(false);
}

void QG_CommandWidget::escape() {
    //leCommand->clearFocus();
    if (m_actionHandler) {
        m_actionHandler->command(QString(tr("escape", "escape, go back from action steps")));
    }
}

void QG_CommandWidget::setActionHandler(QG_ActionHandler* ah) {
    m_actionHandler = ah;
}

void QG_CommandWidget::setCommandMode() {
    // No-op: the colored "Command:" context label was removed.
}

void QG_CommandWidget::setNormalMode() {
    // No-op: the colored "Command:" context label was removed.
}

QString QG_CommandWidget::getRootCommand(const QStringList& cmdList, const QString& typed) {
    //do we have to check for empty cmdList?
    if (cmdList.empty())
        return QString();

    //find the shortest string in cmdList
    auto const& shortestString = *std::min_element(cmdList.begin(), cmdList.end(),
                                                   [](QString const& a, QString const& b) -> bool {
                                                       return a.size() < b.size();
                                                   }
        );
    int const lengthShortestString = shortestString.size();

    // Now we parse the cmdList list, character of each item by character.
    int low = typed.length();
    int high = lengthShortestString + 1;

    while (high > low + 1) {
        int mid = (high + low) / 2;
        bool common = true;

        QString const& proposal = shortestString.left(mid);
        for (auto const& substring : cmdList) {
            if (!substring.startsWith(proposal)) {
                common = false;
                break;
            }
        }
        if (common) {
            low = mid;
        }
        else {
            high = mid;
        }
    }

    // As we assign just before mid value to low (if strings are common), we can use it as parameter for left.
    // If not common -> low value does not changes, even if escaping from the while. This avoids weird behaviors like continuing completion when pressing tab.
    return shortestString.left(low);
}

void QG_CommandWidget::chooseCommandFile() {
    QString path = QFileDialog::getOpenFileName(this);
    if (!path.isEmpty()) {
        leCommand->readCommandFile(path);
    }
}

void QG_CommandWidget::handleKeycode(QString code) {
    if (m_actionHandler->keycode(code)) {
        leCommand->clear();
    }
}

void QG_CommandWidget::setKeycodeMode(bool state) {
    leCommand->m_keycode_mode = state;
}

void QG_CommandWidget::dockingButtonTriggered(bool /*docked*/) {
    auto* cmd_dockwidget = QC_ApplicationWindow::getAppWindow()->findChild<QDockWidget*>("command_dockwidget");
    cmd_dockwidget->setFloating(!cmd_dockwidget->isFloating());
    m_docking->setText(cmd_dockwidget->isFloating() ? tr("Dock") : tr("Float"));
    setWindowTitle(cmd_dockwidget->isFloating() ? tr("Command Line") : tr("Cmd"));
}
