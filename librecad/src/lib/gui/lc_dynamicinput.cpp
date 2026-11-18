#include "lc_dynamicinput.h"

LC_DynamicInput& LC_DynamicInput::instance() {
    static LC_DynamicInput s;
    return s;
}

void LC_DynamicInput::setEnabled(bool on) {
    if (m_enabled == on) return;
    m_enabled = on;
    emit changed();
}

void LC_DynamicInput::setPrompt(const QString& s) {
    if (m_prompt == s) return;
    m_prompt = s;
    emit changed();
}

void LC_DynamicInput::setTyped(const QString& s) {
    if (m_typed == s) return;
    m_typed = s;
    emit changed();
}
