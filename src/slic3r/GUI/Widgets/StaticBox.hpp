#ifndef slic3r_GUI_StaticBox_hpp_
#define slic3r_GUI_StaticBox_hpp_

#include "../wxExtensions.hpp"
#include "StateHandler.hpp"
#include "WidgetAccessible.hpp"

#include <wx/anybutton.h>
#include <wx/checklst.h>
#include <wx/gdicmn.h>
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include <wx/colour.h>
#include <wx/peninfobase.h>
#include <wx/event.h>
#include <wx/window.h>
#include <wx/containr.h>

#include <functional>

// ORCA: wxWindow alone has no TAB-traversal container logic (that's
// wxPanel's job via wxNavigationEnabled<wxWindow>). Every StaticBox-based
// grouping panel app-wide was previously a Tab trap for anything past its
// own direct children - Tab could never escalate to a sibling StaticBox.
class StaticBox : public wxNavigationEnabled<wxWindow>
{
public:
    StaticBox();

    StaticBox(wxWindow* parent,
             wxWindowID      id        = wxID_ANY,
             const wxPoint & pos       = wxDefaultPosition,
             const wxSize &  size      = wxDefaultSize,
             long style = 0);

    bool Create(wxWindow* parent,
        wxWindowID      id        = wxID_ANY,
        const wxPoint & pos       = wxDefaultPosition,
        const wxSize &  size      = wxDefaultSize,
        long style = 0);

    void SetCornerRadius(double radius);

    void SetBorderWidth(int width);

    void SetTopMargin(int margin); // ORCA

    void SetBorderColor(StateColor const & color);

    void SetBorderColorNormal(wxColor const &color);

    void SetBorderStyle(wxPenStyle style);

    // ORCA: roving tabindex hook, for the WAI-ARIA tabs pattern - a tab strip answers
    // "am I the tab Tab should stop on?" and only that tab is offered to traversal.
    // Asked on demand rather than toggled with SetCanFocus() as the selection changes:
    // SetCanFocus(false) also clears the window's GTK can-focus flag, which turns a
    // later SetFocus() into a no-op there, so arrow keys could not move focus within
    // the strip on Linux. This withholds the window from keyboard traversal only, and
    // leaves code, the mouse and the arrow keys able to focus it on every platform.
    void SetKeyboardTabStop(std::function<bool()> is_tab_stop) { m_is_tab_stop = std::move(is_tab_stop); }

    void SetBackgroundColor(StateColor const &color);

    void SetBackgroundColorNormal(wxColor const &color);

    void SetBackgroundColor2(StateColor const &color);

    static wxColor GetParentBackgroundColor(wxWindow * parent);

    void ShowBadge(bool show);

    // ORCA: give this widget a real MSAA/IAccessible role instead of the
    // generic fallback custom-drawn windows get by default. extra_state is
    // an optional callback for widget-specific state bits (e.g. selected,
    // checked, expanded) on top of the focus/enabled/visible state that's
    // always derived automatically.
#if wxUSE_ACCESSIBILITY
    void SetAccessibleRole(wxAccRole role, WidgetAccessible::ExtraStateGetter extra_state = nullptr);

    // ORCA: let assistive technology operate this widget, not just read it. name is the
    // verb MSAA reports for the action (e.g. "Open"); handler runs it in process, on the
    // UI thread, and must go through the widget's normal code path.
    void SetAccessibleDefaultAction(wxString name, WidgetAccessible::DefaultActionHandler handler);
#endif

protected:
#if wxUSE_ACCESSIBILITY
    wxAccessible* CreateAccessible() override;
#endif

    void eraseEvent(wxEraseEvent& evt);

    void sizeEvent(wxSizeEvent& evt);

    void paintEvent(wxPaintEvent& evt);

    void render(wxDC& dc);

    virtual void doRender(wxDC& dc);

    // ORCA: what SetKeyboardTabStop() above set, for a subclass whose
    // AcceptsFocusFromKeyboard() override is the only route left to it.
    bool KeyboardTabStopAllows() const { return !m_is_tab_stop || m_is_tab_stop(); }
    std::function<bool()> m_is_tab_stop;

protected:
    double radius;
    int border_width = 1;
    int top_margin = 0;
    wxPenStyle border_style = wxPENSTYLE_SOLID;
    StateHandler state_handler;
    StateColor   border_color;
    StateColor   background_color;
    StateColor   background_color2;
    ScalableBitmap badge;

#if wxUSE_ACCESSIBILITY
    wxAccRole                              m_a11y_role = wxROLE_SYSTEM_CLIENT;
    WidgetAccessible::ExtraStateGetter     m_a11y_extra_state;
    wxString                               m_a11y_default_action_name;
    WidgetAccessible::DefaultActionHandler m_a11y_default_action;
#endif

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_StaticBox_hpp_
