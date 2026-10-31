#include "WidgetAccessible.hpp"

#if wxUSE_ACCESSIBILITY

#include <wx/control.h>
#include <wx/window.h>
#include <utility>

#include "ComboBox.hpp"

namespace {
// The name somebody actually gave the widget, empty when the tooltip has to stand in
// for one. Real wxControls (e.g. ScalableButton) route through this class because wx's
// stock button accessible ignores SetName() - keep their visible label working as the
// name, exactly as the stock accessible did.
wxString explicit_name(wxWindow* win)
{
    wxString title = win->GetName();
    if (title.empty())
        if (const wxControl* ctrl = wxDynamicCast(win, wxControl))
            title = ctrl->GetLabelText();
    return title;
}

#if wxUSE_HELP
// Hints attached with wxWindow::SetHelpText() - Preferences uses one to announce the F1
// wiki shortcut - normally reach MSAA as the accessible description, but only through
// wxWindowAccessible, which the accessibles here replace. Appended as a paragraph of its
// own so a screen reader pauses between the tooltip text and the hint, the way it already
// does between the paragraphs of a parameter tooltip.
void append_help_text(wxWindow* win, wxString& description)
{
    const wxString help = win->GetHelpText();
    if (help.empty())
        return;
    if (!description.empty())
        description += "\n\n";
    description += help;
}
#else
void append_help_text(wxWindow*, wxString&) {}
#endif // wxUSE_HELP
} // namespace

WidgetAccessible::WidgetAccessible(wxWindow*              win,
                                   wxAccRole              role,
                                   ExtraStateGetter       extra_state,
                                   ExtraDescriptionGetter extra_description)
    : wxWindowAccessible(win)
    , m_role(role)
    , m_extra_state(std::move(extra_state))
    , m_extra_description(std::move(extra_description))
{}

wxAccStatus WidgetAccessible::GetName(int childId, wxString* name)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    wxWindow* win = GetWindow();
    if (!win)
        return wxACC_FAIL;

    wxString title = explicit_name(win);
    if (title.empty()) {
        // Many icon-only buttons never call SetName() but already have a
        // tooltip with a perfectly good label - fall back to that rather
        // than exposing no name at all. Parameter tooltips in particular are
        // a full description plus "parameter name: ..." / "Default: ..."
        // metadata (see Field::get_tooltip_text) - much too long to read
        // back as a name every time the control gets focus, so only take the
        // first paragraph; the rest is still available via GetDescription().
        title = win->GetToolTipText();
        int para_break = title.Find("\n\n");
        if (para_break != wxNOT_FOUND)
            title = title.Left(para_break);
    }
    if (title.empty())
        return wxACC_NOT_IMPLEMENTED;

    *name = title;
    return wxACC_OK;
}

wxAccStatus WidgetAccessible::GetDescription(int childId, wxString* description)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    wxWindow* win = GetWindow();
    if (!win)
        return wxACC_FAIL;

    // Only worth exposing as a separate description when the tooltip has the
    // "name paragraph + metadata" shape and GetName() above truncated it -
    // otherwise the name already says everything the tooltip does.
    wxString       text;
    const wxString tip        = win->GetToolTipText();
    const int      para_break = tip.Find("\n\n");
    if (para_break != wxNOT_FOUND)
        // A widget nobody named is announced by that first paragraph, so repeating it here
        // has it spoken twice; a named one keeps the whole tooltip as its description.
        text = explicit_name(win).empty() ? tip.Mid(para_break + 2) : tip;

    append_help_text(win, text);

    // Painted state the widget reports for itself, after whatever the tooltip and the
    // help text already said.
    if (m_extra_description) {
        const wxString extra = m_extra_description();
        if (!extra.empty()) {
            if (!text.empty())
                text += "\n";
            text += extra;
        }
    }

    if (text.empty())
        return wxACC_NOT_IMPLEMENTED;

    *description = text;
    return wxACC_OK;
}

wxAccStatus WidgetAccessible::GetValue(int childId, wxString* strValue)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    // A button or a tab carries no value. Left to the standard accessible, the value
    // repeats the label, and NVDA reads a tab as "Prepare Prepare".
    if (m_role == wxROLE_SYSTEM_PUSHBUTTON || m_role == wxROLE_SYSTEM_PAGETAB) {
        strValue->clear();
        return wxACC_OK;
    }

    // A combo box is announced as label plus current value, and its name is the label,
    // so the selection is only ever spoken if it comes through here.
    if (m_role != wxROLE_SYSTEM_COMBOBOX)
        return wxACC_NOT_IMPLEMENTED;

    const ComboBox* combo = dynamic_cast<const ComboBox*>(GetWindow());
    if (!combo)
        return wxACC_NOT_IMPLEMENTED;

    *strValue = combo->GetValue();
    return wxACC_OK;
}

wxAccStatus WidgetAccessible::GetKeyboardShortcut(int childId, wxString* shortcut)
{
    // None of these widgets has an accelerator of its own. Left unimplemented, the
    // standard accessible parses the window text for a mnemonic instead and announces
    // a bogus one for any value holding an '&' (see wxIAccessible::get_accKeyboardShortcut).
    return wxACC_FALSE;
}

wxAccStatus WidgetAccessible::GetRole(int childId, wxAccRole* role)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    *role = m_role;
    return wxACC_OK;
}

wxAccStatus WidgetAccessible::GetDefaultAction(int childId, wxString* actionName)
{
    if (childId != wxACC_SELF || !m_default_action || m_default_action_name.empty())
        return wxACC_NOT_IMPLEMENTED;

    *actionName = m_default_action_name;
    return wxACC_OK;
}

wxAccStatus WidgetAccessible::DoDefaultAction(int childId)
{
    if (childId != wxACC_SELF || !m_default_action)
        return wxACC_NOT_IMPLEMENTED;

    wxWindow* win = GetWindow();
    if (!win)
        return wxACC_FAIL;
    // Assistive technology is not aware of a disabled widget being unclickable, and
    // this runs in process on the UI thread, straight into the widget's own code.
    if (!win->IsEnabled())
        return wxACC_FAIL;

    m_default_action();
    return wxACC_OK;
}

wxAccStatus WidgetAccessible::GetState(int childId, long* state)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    wxWindow* win = GetWindow();
    if (!win)
        return wxACC_FAIL;

    long s = 0;
    if (!win->IsShown())
        s |= wxACC_STATE_SYSTEM_INVISIBLE;
    if (!win->IsEnabled())
        s |= wxACC_STATE_SYSTEM_UNAVAILABLE;
    if (win->AcceptsFocus())
        s |= wxACC_STATE_SYSTEM_FOCUSABLE;
    if (win->HasFocus())
        s |= wxACC_STATE_SYSTEM_FOCUSED;
    if (m_extra_state)
        s |= m_extra_state();

    *state = s;
    return wxACC_OK;
}

wxAccStatus NativeCtrlAccessible::GetName(int childId, wxString* name)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    wxWindow* win = GetWindow();
    if (!win)
        return wxACC_FAIL;

    const wxString title = win->GetName();
    if (title.empty())
        // Nothing has named this control - leave the native accessible to its own
        // guess rather than overriding it with nothing.
        return wxACC_NOT_IMPLEMENTED;

    *name = title;
    return wxACC_OK;
}

wxAccStatus NativeCtrlAccessible::GetDescription(int childId, wxString* description)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    wxWindow* win = GetWindow();
    if (!win)
        return wxACC_FAIL;

    // Focus lands on this inner control rather than on the TextInput/SpinInput around it,
    // so the help text its container carries has to be announced from here as well.
    wxString text;
    append_help_text(win, text);
    if (text.empty())
        return wxACC_NOT_IMPLEMENTED;

    *description = text;
    return wxACC_OK;
}

#endif // wxUSE_ACCESSIBILITY
