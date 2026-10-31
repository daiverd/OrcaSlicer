#ifndef slic3r_GUI_WidgetAccessible_hpp_
#define slic3r_GUI_WidgetAccessible_hpp_

#include <wx/access.h>
#include <wx/window.h>

#if wxUSE_ACCESSIBILITY

#include <functional>

// Generic accessible implementation shared by Orca's custom-drawn widgets
// (StaticBox and its subclasses, and other one-off custom wxWindows), so
// each widget only has to supply a role and, optionally, extra state bits
// instead of writing its own wxAccessible subclass.
//
// Name comes from wxWindow::SetName(); focusable/focused/enabled/visible
// state is derived generically from the window itself.
//
// The base class must be wxWindowAccessible, not wxAccessible: only
// wxWindowAccessible implements GetChildCount()/GetChild()/GetParent()/
// Navigate() in terms of the window's child windows. wxAccessible answers
// wxACC_NOT_IMPLEMENTED to all of them, which makes wxIAccessible fall back
// to the control's standard MSAA object - and that object reports a child
// count taken from the child HWNDs while failing every get_accChild() for
// them, so a screen reader sees a strip of nameless, role-less phantom
// children and cannot reach the real ones (the tabs of a page tab list, for
// instance).
//
// The overrides below answer for wxACC_SELF only and return
// wxACC_NOT_IMPLEMENTED for a child id, which is wx's own convention (see
// wxWindowAccessible::GetName): wxIAccessible then resolves the child id
// through GetChild() and asks the child's own accessible instead.
class WidgetAccessible : public wxWindowAccessible
{
public:
    using ExtraStateGetter = std::function<long()>;
    // Runs the widget's default action - what MSAA's accDoDefaultAction performs.
    // Windows' UIA proxy builds its ExpandCollapse and Invoke patterns on top of it,
    // so a widget that reports an expandable state needs one to be operable at all.
    using DefaultActionHandler = std::function<void()>;
    // Extra sentence appended to the description, for painted state that has no
    // MSAA state bit of its own. Queried on demand so it follows the widget.
    using ExtraDescriptionGetter = std::function<wxString()>;

    WidgetAccessible(wxWindow*              win,
                     wxAccRole              role,
                     ExtraStateGetter       extra_state       = nullptr,
                     ExtraDescriptionGetter extra_description = nullptr);

    wxAccStatus GetName(int childId, wxString* name) override;
    wxAccStatus GetRole(int childId, wxAccRole* role) override;
    wxAccStatus GetState(int childId, long* state) override;
    wxAccStatus GetDescription(int childId, wxString* description) override;
    wxAccStatus GetValue(int childId, wxString* strValue) override;
    wxAccStatus GetKeyboardShortcut(int childId, wxString* shortcut) override;
    wxAccStatus GetDefaultAction(int childId, wxString* actionName) override;
    wxAccStatus DoDefaultAction(int childId) override;

    void SetRole(wxAccRole role) { m_role = role; }
    void SetExtraStateGetter(ExtraStateGetter getter) { m_extra_state = std::move(getter); }
    // name may be empty, which leaves the action's name to the standard accessible.
    void SetDefaultAction(wxString name, DefaultActionHandler handler)
    {
        m_default_action_name = std::move(name);
        m_default_action      = std::move(handler);
    }

private:
    wxAccRole              m_role;
    ExtraStateGetter       m_extra_state;
    ExtraDescriptionGetter m_extra_description;
    wxString               m_default_action_name;
    DefaultActionHandler   m_default_action;
};

// Accessible for a *native* control (a real Win32 window such as the EDIT behind
// wxTextCtrl) that only needs a name. wxWindow::SetName() is not plumbed into MSAA
// at all: with no wxAccessible attached, wx lets DefWindowProc answer WM_GETOBJECT
// and the OS proxy names the control after a preceding static label - which Orca's
// custom-drawn parameter rows do not have, so the control ends up nameless.
//
// The same holds for a help text hint (wxWindow::SetHelpText), which only reaches MSAA
// through wxWindowAccessible - a native control does not have one.
//
// Every method other than those two is left to the base class, which for a wxControl
// answers wxACC_NOT_IMPLEMENTED for role, state, value and the default action, so
// wxIAccessible falls back to the control's own standard accessible object (see
// wxAccessible::GetIAccessibleStd) and role, value, caret, selection and the
// read-only state keep coming from the native control exactly as before.
//
// The base is wxWindowAccessible rather than wxAccessible because
// wxAccessible's GetChildCount()/GetChild()/GetParent()/Navigate() are no-ops, and
// wxIAccessible answers that by taking the child count from the child HWNDs while failing
// get_accChild() for every one of them, so the window advertises children no client can
// resolve. Latent on these controls today - a native EDIT has no child HWNDs - but the
// class is generic and the next window given one may.
class NativeCtrlAccessible : public wxWindowAccessible
{
public:
    explicit NativeCtrlAccessible(wxWindow* win) : wxWindowAccessible(win) {}

    wxAccStatus GetName(int childId, wxString* name) override;
    wxAccStatus GetDescription(int childId, wxString* description) override;
};

#endif // wxUSE_ACCESSIBILITY

#endif // slic3r_GUI_WidgetAccessible_hpp_
