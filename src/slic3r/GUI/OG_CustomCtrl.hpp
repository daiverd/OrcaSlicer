#ifndef slic3r_OG_CustomCtrl_hpp_
#define slic3r_OG_CustomCtrl_hpp_

#include <wx/panel.h>
#include <wx/gdicmn.h>
#include <wx/dc.h>
#include <wx/colour.h>
#include <cstddef>
#include <vector>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/settings.h>

#include <map>
#include <functional>
#include <wx/types.h>
#include <wx/string.h>
#include <wx/validate.h>

#include "libslic3r/Config.hpp"

#include "slic3r/GUI/wxExtensions.hpp"

#include "OptionsGroup.hpp"
#include "Widgets/WidgetAccessible.hpp"

class wxColour;
class wxDC;
class wxMouseEvent;
class wxPaintEvent;
class wxSizer;
class wxWindow;
namespace Slic3r::GUI { class Field; }

// Translate the ifdef 
#ifdef __WXOSX__
    #define wxOSX true
#else
    #define wxOSX false
#endif

namespace Slic3r { namespace GUI {

class OptionStateRegistry;

//  Static text shown among the options.
class OG_CustomCtrl :public wxPanel
{
    wxFont  m_font;
    int     m_v_gap;
    int     m_v_gap2;
    int     m_h_gap;
    int     m_em_unit;

    wxSize  m_bmp_mode_sz;
    wxSize  m_bmp_blinking_sz;

    ScalableBitmap m_bmp_multi_extruder; // ORCA multi extruder icon drawn on left margin for variant options

    int     m_max_win_width{0};

    struct CtrlLine {
        wxCoord           width{ wxDefaultCoord };
        wxCoord           height{ wxDefaultCoord };
        OG_CustomCtrl*    ctrl    { nullptr };
        const Line&       og_line;

        bool draw_just_act_buttons  { false };
        bool draw_mode_bitmap       { true };
        bool is_visible             { true };
        bool is_focused             { false };

        CtrlLine(   wxCoord         height,
                    OG_CustomCtrl*  ctrl,
                    const Line&     og_line,
                    bool            draw_just_act_buttons = false,
                    bool            draw_mode_bitmap = true);
        ~CtrlLine() { ctrl = nullptr; }

        int     get_max_win_width();
        void    correct_items_positions();
        void    msw_rescale();
        void    update_visibility(ConfigOptionMode mode);

        void render_separator(wxDC& dc, wxCoord v_pos);

        void    render(wxDC& dc, wxCoord h_pos, wxCoord v_pos);
        wxCoord draw_text      (wxDC& dc, wxPoint pos, const wxString& text, const wxColour* color, int width, bool is_url = false, bool is_main = false);
        wxPoint draw_blinking_bmp(wxDC& dc, wxPoint pos, bool is_blinking);
        wxPoint draw_act_bmps(wxDC& dc, wxPoint pos, const wxBitmap& bmp_undo_to_sys, const wxBitmap& bmp_undo, bool is_blinking, size_t rect_id = 0, bool is_main = false);
        wxCoord draw_edit_bmp(wxDC& dc, wxPoint pos, const wxBitmap& bmp_edit);
        bool    launch_browser() const;
        bool    is_separator() const { return og_line.is_separator(); }

        std::vector<wxRect> rects_undo_icon;
        std::vector<wxRect> rects_undo_to_sys_icon;
        std::vector<wxRect> rects_edit_icon;
        wxRect              rect_label;
    };

    std::vector<CtrlLine> ctrl_lines;
    const OptionStateRegistry* m_option_state_registry{nullptr};

    int  m_icon_focus { -1 }; // index into icon_actions() while an icon bar has the focus
    const CtrlLine* icon_line() const;
    Field*          icon_field() const;
    void            notify_icon_focus();
    void            OnIconNavigation(wxNavigationKeyEvent& event);
    void            OnIconKey(wxKeyEvent& event);

public:
    OG_CustomCtrl(  wxWindow* parent,
                    OptionsGroup* og,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    const wxValidator& val = wxDefaultValidator,
                    const wxString& name = wxEmptyString);
    ~OG_CustomCtrl() {}

    void set_option_state_registry(const OptionStateRegistry* registry) { m_option_state_registry = registry; }

    void    OnPaint(wxPaintEvent&);
    void    OnMotion(wxMouseEvent& event);
    void    OnLeftDown(wxMouseEvent& event);
    void    OnLeaveWin(wxMouseEvent& event);

    void    init_ctrl_lines();
    bool    update_visibility(ConfigOptionMode mode);
    void    correct_window_position(wxWindow* win, const Line& line, Field* field = nullptr);
    void    correct_widgets_position(wxSizer* widget, const Line& line, Field* field = nullptr);
    void    init_max_win_width();
    void    set_max_win_width(int max_win_width);
    int     get_max_win_width() { return m_max_win_width; }

    //BBS
    int    get_title_width();
    // BBS
    void fixup_items_positions();

    void    msw_rescale();
    void    sys_color_changed();

#if wxUSE_ACCESSIBILITY
    // ORCA: this panel holds every row of one options group, so it is the natural
    // place to expose the group itself - see the constructor.
    wxAccessible* CreateAccessible() override;
#endif

    // ORCA: a group whose only field sits outside this panel (the full-width G-code and
    // notes boxes) leaves it painting nothing but that field's buttons. The keyboard then
    // reaches each button that would do something as a Tab stop of its own. (The reset
    // to system value button is compiled out, see DISABLE_UNDO_SYS.)
    enum class IconAction { Undo, Edit };
    bool                    is_icon_bar() const { return GetChildren().empty(); }
    std::vector<IconAction> icon_actions() const;
    wxString                icon_name(IconAction action) const;
    wxString                icon_description(IconAction action) const;
    wxRect                  icon_rect(IconAction action) const;
    int                     focused_icon() const { return m_icon_focus; }
    void                    activate_icon(IconAction action);

    bool AcceptsFocus() const override { return is_icon_bar() ? !icon_actions().empty() : wxPanel::AcceptsFocus(); }
    bool AcceptsFocusFromKeyboard() const override { return is_icon_bar() ? !icon_actions().empty() : wxPanel::AcceptsFocusFromKeyboard(); }

    wxPoint get_pos(const Line& line, Field* field = nullptr);
    int     get_height(const Line& line);

    OptionsGroup*  opt_group;

private:
    void activate_icon(const CtrlLine& line, const std::string& opt_key, IconAction action);
};

}}

#endif /* slic3r_OG_CustomCtrl_hpp_ */
