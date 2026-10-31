#ifndef slic3r_GUI_StaticBox_hpp_
#define slic3r_GUI_StaticBox_hpp_

#include "../wxExtensions.hpp"
#include "StateHandler.hpp"

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

protected:
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

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_StaticBox_hpp_
