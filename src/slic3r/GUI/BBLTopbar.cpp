#include "BBLTopbar.hpp"
#include "wx/artprov.h"
#include "wx/aui/framemanager.h"
#include "wx/display.h"
#include <wx/gdicmn.h>
#include <wx/event.h>
#include <wx/dcbuffer.h>
#include <wx/dc.h>
#include <wx/aui/auibar.h>
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include <wx/settings.h>
#include "slic3r/GUI/Widgets/Label.hpp"
#include <wx/image.h>
#include <wx/string.h>
#include <cstddef>
#include <wx/utils.h>
#include <wx/window.h>
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "GUI.hpp"
#ifdef __WXGTK__
#include "LinuxDisplayBackend.hpp"
#endif
#include "wxExtensions.hpp"
#include "Plater.hpp"
#include "MainFrame.hpp"
#include "Notebook.hpp"
#ifdef SLIC3R_CAD
#include "CAD/DesignPanel.hpp"
#endif
#include "WebViewDialog.hpp"
#include "PartPlate.hpp"
#include "Widgets/WidgetAccessible.hpp"

#include <utility>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>

#include <boost/log/trivial.hpp>
#include "libslic3r/AppConfig.hpp"

#ifdef __WXGTK__
#include <gtk/gtk.h>
#endif

#define TOPBAR_ICON_SIZE  18
#define TOPBAR_TITLE_WIDTH  300

using namespace Slic3r;
using namespace Slic3r::GUI;

enum CUSTOM_ID
{
    ID_TOP_MENU_TOOL = 3100,
    ID_LOGO,
    ID_TOP_FILE_MENU,
    ID_TOP_DROPDOWN_MENU,
    ID_TITLE,
    ID_MODEL_STORE,
    ID_CALIB,
    ID_TOOL_BAR = 3200,
    ID_AMS_NOTEBOOK,
};

// ORCA: name and role of one interactive top bar tool. The tools are icon-only and
// carry no short help, so there is nothing to derive an accessible name from; menu
// openers are told apart from plain buttons so they can be given the menu button role.
struct TopbarToolInfo
{
    wxString name;
    bool     menu_opener { false };
};

// ORCA: false for anything that should not become a keyboard stop of its own.
static bool get_topbar_tool_info(const wxAuiToolBarItem& item, TopbarToolInfo& info)
{
    switch (item.GetId()) {
    case ID_TOP_FILE_MENU:     info = {_L("File"), true}; return true;
    case ID_TOP_DROPDOWN_MENU: info = {_L("Menu"), true}; return true;
    case ID_CALIB:             info = {_L("Calibration"), true}; return true;
    case wxID_OPEN:            info = {_L("Open Project"), false}; return true;
    case wxID_SAVE:            info = {_L("Save Project"), false}; return true;
    case wxID_UNDO:            info = {_L("Undo"), false}; return true;
    case wxID_REDO:            info = {_L("Redo"), false}; return true;
    // The window buttons are already reachable from the keyboard through the native
    // window menu (Alt+Space), which also names them, so they stay out of the ring.
    case wxID_ICONIZE_FRAME:
    case wxID_MAXIMIZE_FRAME:
    case wxID_CLOSE_FRAME:     return false;
    default:
        // Anything added later still gets a stop as long as it has a visible label.
        info = {item.GetLabel(), false};
        return !info.name.empty();
    }
}

#if wxUSE_ACCESSIBILITY
// ORCA: the proxy covers its tool, so the window rect MSAA would report by itself is
// already the right one; reporting the tool's rect explicitly keeps the two in step even
// in the moment between a re-layout of the bar and the proxies following it. The rest
// (name, focus and enabled state) is the generic WidgetAccessible behaviour.
class TopbarToolAccessible : public WidgetAccessible
{
public:
    TopbarToolAccessible(TopbarToolProxy* proxy, wxAccRole role, ExtraStateGetter extra_state);

    wxAccStatus GetLocation(wxRect& rect, int elementId) override;
    wxAccStatus GetDefaultAction(int childId, wxString* actionName) override;
    wxAccStatus DoDefaultAction(int childId) override;
};
#endif // wxUSE_ACCESSIBILITY

// ORCA: a wxAuiToolBar tool is not a window. It cannot take the keyboard focus and no
// screen reader can see it, which made the whole top bar mouse-only and invisible to
// assistive technology. Give every interactive tool a child window laid over it that
// stands in for it: wx traversal focuses the proxy, the arrow keys move between
// proxies, Enter/Space runs the tool's own handler, and the proxy carries the tool's
// accessible name, role, state and screen rectangle. The proxy takes over drawing the
// tool it covers, and returns HTTRANSPARENT for every mouse message, so nothing about
// the toolbar's own appearance or mouse handling, including its double duty as the
// window title bar, changes.
class TopbarToolProxy : public wxControl
{
public:
    TopbarToolProxy(BBLTopbar* topbar, int tool_id, const TopbarToolInfo& info)
        : m_topbar(topbar)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        // wxWANTS_CHARS: this window handles the arrow, Home/End, Enter/Space and Tab
        // keys itself, so they must be delivered here instead of being eaten by the
        // dialog navigation code.
        // The real position and size arrive with the first LayoutToolProxies().
        Create(topbar, wxID_ANY, wxPoint(0, 0), wxSize(1, 1), wxWANTS_CHARS | wxBORDER_NONE);
        SetTool(tool_id, info);
        // A proxy covers its tool exactly (see BBLTopbar::LayoutToolProxies), and a child
        // window is clipped out of its parent's painting, so the pixels of the tool are
        // the proxy's to draw. It draws them through the bar's own art provider, so this
        // is the same drawing as before, moved to the window that now owns the area.
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(this);
            m_topbar->PaintTool(dc, m_tool_id, GetClientRect());
        });
        Bind(wxEVT_KEY_DOWN, &TopbarToolProxy::OnKeyDown, this);
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& evt) {
            m_topbar->SetFocusedTool(m_tool_id);
            evt.Skip();
        });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& evt) {
            m_topbar->SetFocusedTool(wxID_NONE);
            evt.Skip();
        });
    }

    // Re-point an existing proxy at a tool, so re-syncing never destroys windows that
    // may hold the focus.
    void SetTool(int tool_id, const TopbarToolInfo& info)
    {
        m_tool_id     = tool_id;
        m_menu_opener = info.menu_opener;
        SetName(info.name);
    }

    BBLTopbar* GetTopbar() const { return m_topbar; }
    int        GetToolId() const { return m_tool_id; }

protected:
#ifdef __WXMSW__
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif
#if wxUSE_ACCESSIBILITY
    wxAccessible* CreateAccessible() override;
#endif

private:
    void OnKeyDown(wxKeyEvent& evt);

    BBLTopbar* m_topbar;
    int        m_tool_id { wxID_NONE };
    bool       m_menu_opener { false };
};

void TopbarToolProxy::OnKeyDown(wxKeyEvent& evt)
{
    switch (evt.GetKeyCode()) {
    // Arrows move within the bar, the toolbar idiom the tab strips already follow.
    case WXK_LEFT:  m_topbar->FocusAdjacentTool(this, -1); return;
    case WXK_RIGHT: m_topbar->FocusAdjacentTool(this, 1); return;
    // Swallowed rather than passed on: this bar is horizontal, so up and down have no
    // meaning in it and must not fall through to the frame.
    case WXK_UP:
    case WXK_DOWN:  return;
    case WXK_HOME:  m_topbar->FocusAdjacentTool(nullptr, 1); return;
    case WXK_END:   m_topbar->FocusAdjacentTool(nullptr, -1); return;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
    case WXK_SPACE: m_topbar->ActivateTool(m_tool_id); return;
    case WXK_TAB: {
        // Tab steps to the next tool and leaves the bar at its ends. MainFrame is a
        // wxFrame, not a TAB-traversal container, so Navigate() has nothing to hand the
        // focus to; FocusNextTabStop() does the leaving itself rather than wrapping,
        // which is what made the bar a TAB trap.
        const bool forward = !evt.ShiftDown();
        const int  flags   = wxNavigationKeyEvent::FromTab |
                             (forward ? wxNavigationKeyEvent::IsForward : wxNavigationKeyEvent::IsBackward);
        if (!m_topbar->Navigate(flags))
            m_topbar->FocusNextTabStop(this, forward);
        return;
    }
    default: evt.Skip(); return;
    }
}

#if wxUSE_ACCESSIBILITY
wxAccessible* TopbarToolProxy::CreateAccessible()
{
    return new TopbarToolAccessible(this, m_menu_opener ? wxROLE_SYSTEM_BUTTONMENU : wxROLE_SYSTEM_PUSHBUTTON,
                                    [this] {
                                        // The proxy window is always enabled, so the states that
                                        // matter - a disabled tool, or one whose menu is open -
                                        // have to be read off the tool itself. A disabled tool
                                        // stays focusable and simply announces as unavailable,
                                        // rather than disappearing from the ring.
                                        long state = 0;
                                        if (!m_topbar->GetToolEnabled(m_tool_id))
                                            state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
                                        if (m_topbar->GetToolSticky(m_tool_id))
                                            state |= wxACC_STATE_SYSTEM_PRESSED;
                                        return state;
                                    });
}

TopbarToolAccessible::TopbarToolAccessible(TopbarToolProxy* proxy, wxAccRole role, ExtraStateGetter extra_state)
    : WidgetAccessible(proxy, role, std::move(extra_state))
{}

wxAccStatus TopbarToolAccessible::GetLocation(wxRect& rect, int elementId)
{
    if (elementId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    TopbarToolProxy* proxy = static_cast<TopbarToolProxy*>(GetWindow());
    if (!proxy)
        return wxACC_FAIL;

    BBLTopbar*   topbar    = proxy->GetTopbar();
    const wxRect tool_rect = topbar->GetToolRect(proxy->GetToolId());
    if (tool_rect.IsEmpty())
        return wxACC_NOT_IMPLEMENTED;

    rect = wxRect(topbar->ClientToScreen(tool_rect.GetTopLeft()), tool_rect.GetSize());
    return wxACC_OK;
}

wxAccStatus TopbarToolAccessible::GetDefaultAction(int childId, wxString* actionName)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    *actionName = _L("Press");
    return wxACC_OK;
}

wxAccStatus TopbarToolAccessible::DoDefaultAction(int childId)
{
    if (childId != wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    TopbarToolProxy* proxy = static_cast<TopbarToolProxy*>(GetWindow());
    if (!proxy)
        return wxACC_FAIL;

    proxy->GetTopbar()->ActivateTool(proxy->GetToolId());
    return wxACC_OK;
}
#endif // wxUSE_ACCESSIBILITY

CenteredTitle::CenteredTitle(wxWindow* parent)
    : wxControl()
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    // ORCA: wx defaults an unnamed window's GetName() to "control", which a screen
    // reader would read out until the first SetTitle() replaces it.
    SetName(wxEmptyString);
    Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(38, 46, 48)));
        dc.Clear();

        dc.SetTextForeground(*wxWHITE);

        wxFontMetrics fm = dc.GetFontMetrics();
        int textHeight = fm.ascent + fm.descent;

        wxRect   rect       = GetClientRect();
        wxString ellipsized = wxControl::Ellipsize(m_title, dc, wxELLIPSIZE_END, wxMax(0, rect.GetWidth() - FromDIP(8)));

        int y = rect.y + (rect.height - textHeight) / 2 + 1;
        int x = rect.x + ((ellipsized != m_title)                      // is ellipsized
            ? FromDIP(4)                                               // align to left when clipped
            : (rect.width - dc.GetTextExtent(m_title).GetWidth()) / 2); // centered when has available space

        dc.DrawText(ellipsized, x, y);
    });

    // repaint for Ellipsize
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        Refresh();
        e.Skip();
    });

    auto forwardMouseEvent = [this](wxMouseEvent& e) {
        if (e.LeftDown() && e.GetClickCount() > 1) return; // prevent duplicate event
        e.SetPosition(GetParent()->ScreenToClient(ClientToScreen(e.GetPosition())));
        GetParent()->GetEventHandler()->ProcessEvent(e);
    };

    Bind(wxEVT_LEFT_DOWN,   forwardMouseEvent);
    Bind(wxEVT_LEFT_DCLICK, forwardMouseEvent);
}

void CenteredTitle::SetTitle(const wxString& title) {
    m_title = title;
    Refresh();
}

#if wxUSE_ACCESSIBILITY
wxAccessible* CenteredTitle::CreateAccessible()
{
    // ORCA: custom-drawn text, so nothing exposes it to a screen reader by itself. The
    // name is the project title, set by BBLTopbar::SetTitle().
    return new WidgetAccessible(this, wxROLE_SYSTEM_STATICTEXT);
}
#endif

// required for proper height
wxSize CenteredTitle::DoGetBestSize() const
{
    return wxSize(10, FromDIP(30));
}


class BBLTopbarArt : public wxAuiDefaultToolBarArt
{
public:
    virtual void DrawBackground(wxDC& dc, wxWindow* wnd, const wxRect& rect) wxOVERRIDE;
    virtual void DrawButton(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect) wxOVERRIDE;

    // ORCA: id of the tool whose focus proxy currently holds the keyboard focus
    void SetFocusedTool(int tool_id) { m_focused_tool = tool_id; }

private:
    int m_focused_tool { wxID_NONE };
};

void BBLTopbarArt::DrawBackground(wxDC& dc, wxWindow* wnd, const wxRect& rect)
{
    // ORCA: stroke the edge in the fill colour, the way DrawButton does. The default pen
    // is black, and now that each tool is painted into a proxy window of its own the
    // rectangle's edges land inside the visible area instead of off the ends of the bar,
    // so an unset pen outlines every tool.
    const wxColour background(38, 46, 48);
    dc.SetPen(wxPen(background));
    dc.SetBrush(wxBrush(background));
    wxRect clipRect = rect;
    clipRect.y -= 8;
    clipRect.height += 8;
    dc.SetClippingRegion(clipRect);
    dc.DrawRectangle(rect);
    dc.DestroyClippingRegion();
}

void BBLTopbarArt::DrawButton(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect)
{
    int textWidth = 0, textHeight = 0;
    wxFontMetrics fm;

    if (m_flags & wxAUI_TB_TEXT)
    {
        dc.SetFont(m_font);
        fm = dc.GetFontMetrics();
        textHeight = fm.ascent + fm.descent;
        dc.GetTextExtent(item.GetLabel(), &textWidth, nullptr);
    }

    int bmpX = 0, bmpY = 0;
    int textX = 0, textY = 0;

    // ORCA resolves the toolbar item bitmap using the actual window DPI context used for painting.
    // GetBitmap() / GetDisabledBitmap() was using internal window pointer (m_window), not the paint-time wnd
    // m_window was created before final DPI context was known so items not scales properly
    const wxBitmap bmp = item.GetCurrentBitmapFor(wnd);

    const wxSize bmpSize = bmp.IsOk() ? bmp.GetScaledSize() : wxSize(0, 0);

    if (m_textOrientation == wxAUI_TBTOOL_TEXT_BOTTOM)
    {
        bmpX = rect.x +
            (rect.width / 2) -
            (bmpSize.x / 2);

        bmpY = rect.y +
            ((rect.height - textHeight) / 2) -
            (bmpSize.y / 2);

        textX = rect.x + (rect.width / 2) - (textWidth / 2) + 1;
        textY = rect.y + rect.height - textHeight - 1;
    }
    else if (m_textOrientation == wxAUI_TBTOOL_TEXT_RIGHT)
    {
        bmpX = rect.x + wnd->FromDIP(3);

        bmpY = rect.y +
            (rect.height / 2) -
            (bmpSize.y / 2);

        textX = bmpX + wnd->FromDIP(3) + bmpSize.x;
        textY = bmpY + bmpSize.y / 2 - textHeight / 2 - fm.externalLeading + 1;
    }


    if (!(item.GetState() & wxAUI_BUTTON_STATE_DISABLED))
    {
        if (item.GetState() & wxAUI_BUTTON_STATE_PRESSED)
        {
            dc.SetPen(wxPen(StateColor::darkModeColorFor("#009688"))); // ORCA
            dc.SetBrush(wxBrush(StateColor::darkModeColorFor("#009688"))); // ORCA
            dc.DrawRectangle(rect);
        }
        else if ((item.GetState() & wxAUI_BUTTON_STATE_HOVER) || item.IsSticky())
        {
            dc.SetPen(wxPen(StateColor::darkModeColorFor("#009688"))); // ORCA
            dc.SetBrush(wxBrush(StateColor::darkModeColorFor("#009688"))); // ORCA

            // draw an even lighter background for checked item hovers (since
            // the hover background is the same color as the check background)
            if (item.GetState() & wxAUI_BUTTON_STATE_CHECKED)
                dc.SetBrush(wxBrush(StateColor::darkModeColorFor("#009688"))); // ORCA

            dc.DrawRectangle(rect);
        }
        else if (item.GetState() & wxAUI_BUTTON_STATE_CHECKED)
        {
            // it's important to put this code in an else statement after the
            // hover, otherwise hovers won't draw properly for checked items
            dc.SetPen(wxPen(StateColor::darkModeColorFor("#009688"))); // ORCA
            dc.SetBrush(wxBrush(StateColor::darkModeColorFor("#009688"))); // ORCA
            dc.DrawRectangle(rect);
        }
    }

    if (bmp.IsOk())
        dc.DrawBitmap(bmp, bmpX, bmpY, true);

    // set the item's text color based on if it is disabled
#ifdef __WINDOWS__
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
#else
    dc.SetTextForeground(*wxWHITE);
#endif
    if (item.GetState() & wxAUI_BUTTON_STATE_DISABLED)
    {
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    }

    if ((m_flags & wxAUI_TB_TEXT) && !item.GetLabel().empty())
    {
        dc.DrawText(item.GetLabel(), textX, textY);
    }

    // ORCA: keyboard focus ring. Every state of these items is drawn as the same teal
    // fill, so focus needs an indicator of its own: the near-white of the label text,
    // which clears 3:1 against both the fill and the bar background, and which
    // StateColor does not remap for dark mode.
    if (m_focused_tool != wxID_NONE && item.GetId() == m_focused_tool)
    {
        wxRect focus_rect = rect;
        focus_rect.Deflate(wnd->FromDIP(1));
        dc.SetPen(wxPen(wxColour(254, 254, 254), wnd->FromDIP(2)));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(focus_rect);
    }
}

// ORCA: wxTAB_TRAVERSAL is what makes the tool proxies reachable - it is the style both
// the traversal container and the platform's dialog navigation key off.
static constexpr long TOPBAR_STYLE = wxAUI_TB_TEXT | wxAUI_TB_HORZ_TEXT | wxTAB_TRAVERSAL;

BBLTopbar::BBLTopbar(wxFrame* parent)
{
    // ORCA: wxNavigationEnabled<> has only a default constructor, so the window has to
    // be created in two phases rather than through a base-class initializer.
    wxAuiToolBar::Create(parent, ID_TOOL_BAR, wxDefaultPosition, wxDefaultSize, TOPBAR_STYLE);
    Init(parent);
    // The proxies stay out of Init(): they are a separate, idempotent pass over the
    // finished item list, and Init() is public and may be called again.
    SyncToolProxies();
}

BBLTopbar::BBLTopbar(wxWindow* pwin, wxFrame* parent)
{
    wxAuiToolBar::Create(pwin, ID_TOOL_BAR, wxDefaultPosition, wxDefaultSize, TOPBAR_STYLE);
    Init(parent);
    SyncToolProxies();
}

void BBLTopbar::Init(wxFrame* parent)
{
    auto* art = new BBLTopbarArt();
    art->SetFont(Label::Body_12);
    SetArtProvider(art);
    m_topbar_art = art;
    m_frame = parent;
    m_skip_popup_file_menu = false;
    m_skip_popup_dropdown_menu = false;
    m_skip_popup_calib_menu    = false;

    wxInitAllImageHandlers();

    bool window_btns_on_left = false;

#ifdef __linux__
    window_btns_on_left = wxGetApp().app_config->get("window_buttons_on_left")  == "true";

    if(window_btns_on_left){
        wxBitmap close_bitmap = create_scaled_bitmap("topbar_close", nullptr, TOPBAR_ICON_SIZE);
        wxAuiToolBarItem* close_btn = this->AddTool(wxID_CLOSE_FRAME, "", close_bitmap);

        this->AddSpacer(FromDIP(4));

        maximize_bitmap = create_scaled_bitmap("topbar_max", nullptr, TOPBAR_ICON_SIZE);
        window_bitmap = create_scaled_bitmap("topbar_win", nullptr, TOPBAR_ICON_SIZE);
        if (m_frame->IsMaximized()) {
            maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", window_bitmap);
        }
        else {
            maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", maximize_bitmap);
        }

        this->AddSpacer(FromDIP(4));

        wxBitmap iconize_bitmap = create_scaled_bitmap("topbar_min", nullptr, TOPBAR_ICON_SIZE);
        wxAuiToolBarItem* iconize_btn = this->AddTool(wxID_ICONIZE_FRAME, "", iconize_bitmap);

        this->AddSpacer(15);
    }
#endif

    if(!window_btns_on_left)
        this->AddSpacer(5);

    /*wxBitmap logo_bitmap = create_scaled_bitmap("topbar_logo", nullptr, TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* logo_item = this->AddTool(ID_LOGO, "", logo_bitmap);
    logo_item->SetHoverBitmap(logo_bitmap);
    logo_item->SetActive(false);*/

    wxBitmap file_bitmap = create_scaled_bitmap("topbar_file", nullptr, TOPBAR_ICON_SIZE);
    m_file_menu_item = this->AddTool(ID_TOP_FILE_MENU, _L("File"), file_bitmap, wxEmptyString, wxITEM_NORMAL);

    this->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));

    this->AddSpacer(FromDIP(5));

    wxBitmap dropdown_bitmap = create_scaled_bitmap("topbar_dropdown", nullptr, TOPBAR_ICON_SIZE);
    m_dropdown_menu_item = this->AddTool(ID_TOP_DROPDOWN_MENU, "",
        dropdown_bitmap, wxEmptyString);

    this->AddSpacer(FromDIP(5));
    this->AddSeparator();
    this->AddSpacer(FromDIP(5));

    //wxBitmap open_bitmap = create_scaled_bitmap("topbar_open", nullptr, TOPBAR_ICON_SIZE);
    //wxAuiToolBarItem* tool_item = this->AddTool(wxID_OPEN, "", open_bitmap);

    this->AddSpacer(FromDIP(10));

    wxBitmap save_bitmap = create_scaled_bitmap("topbar_save", nullptr, TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* save_btn = this->AddTool(wxID_SAVE, "", save_bitmap);

    this->AddSpacer(FromDIP(10));

    wxBitmap undo_bitmap = create_scaled_bitmap("topbar_undo", nullptr, TOPBAR_ICON_SIZE);
    m_undo_item = this->AddTool(wxID_UNDO, "", undo_bitmap);
    wxBitmap undo_inactive_bitmap = create_scaled_bitmap("topbar_undo_inactive", nullptr, TOPBAR_ICON_SIZE);
    m_undo_item->SetDisabledBitmap(undo_inactive_bitmap);

    this->AddSpacer(FromDIP(10));

    wxBitmap redo_bitmap = create_scaled_bitmap("topbar_redo", nullptr, TOPBAR_ICON_SIZE);
    m_redo_item = this->AddTool(wxID_REDO, "", redo_bitmap);
    wxBitmap redo_inactive_bitmap = create_scaled_bitmap("topbar_redo_inactive", nullptr, TOPBAR_ICON_SIZE);
    m_redo_item->SetDisabledBitmap(redo_inactive_bitmap);

    this->AddSpacer(FromDIP(10));

    wxBitmap calib_bitmap          = create_scaled_bitmap("calib_sf", nullptr, TOPBAR_ICON_SIZE);
    wxBitmap calib_bitmap_inactive = create_scaled_bitmap("calib_sf_inactive", nullptr, TOPBAR_ICON_SIZE);
    m_calib_item                   = this->AddTool(ID_CALIB, _L("Calibration"), calib_bitmap);
    m_calib_item->SetDisabledBitmap(calib_bitmap_inactive);

    this->AddSpacer(FromDIP(25));
    //this->AddStretchSpacer(1);

    m_title_ctrl = new CenteredTitle(this);
    m_title_ctrl->SetFont(Label::Body_12);
    wxAuiToolBarItem* title_item = this->AddControl(m_title_ctrl, "");
    title_item->SetProportion(1); 

    this->AddSpacer(FromDIP(25));
    //this->AddStretchSpacer(1);

    //this->EnableTool(m_publish_item->GetId(), false);
    //this->AddSpacer(FromDIP(4));

    /*wxBitmap model_store_bitmap = create_scaled_bitmap("topbar_store", nullptr, TOPBAR_ICON_SIZE);
    m_model_store_item = this->AddTool(ID_MODEL_STORE, "", model_store_bitmap);
    this->AddSpacer(12);
    */

    //this->AddSeparator();
    //this->AddSpacer(FromDIP(4));

    if(!window_btns_on_left){
    wxBitmap iconize_bitmap = create_scaled_bitmap("topbar_min", nullptr, TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* iconize_btn = this->AddTool(wxID_ICONIZE_FRAME, "", iconize_bitmap);

    this->AddSpacer(FromDIP(4));

    maximize_bitmap = create_scaled_bitmap("topbar_max", nullptr, TOPBAR_ICON_SIZE);
    window_bitmap = create_scaled_bitmap("topbar_win", nullptr, TOPBAR_ICON_SIZE);
    if (m_frame->IsMaximized()) {
        maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", window_bitmap);
    }
    else {
        maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", maximize_bitmap);
    }

    this->AddSpacer(FromDIP(4));

    wxBitmap close_bitmap = create_scaled_bitmap("topbar_close", nullptr, TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* close_btn = this->AddTool(wxID_CLOSE_FRAME, "", close_bitmap);
    }

    Realize();
    // m_toolbar_h = this->GetSize().GetHeight();
    m_toolbar_h = FromDIP(30);

    int client_w = parent->GetClientSize().GetWidth();
    this->SetSize(client_w, m_toolbar_h);

    // ORCA: the proxies follow the tools they cover. wxAuiToolBar lays its items out in
    // its own wxEVT_SIZE handler, which runs after this one, so read the new rects on the
    // next turn of the event loop rather than the stale ones now.
    this->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        event.Skip();
        CallAfter([this] { LayoutToolProxies(); });
    });
    this->Bind(wxEVT_MOTION, &BBLTopbar::OnMouseMotion, this);
    this->Bind(wxEVT_MOUSE_CAPTURE_LOST, &BBLTopbar::OnMouseCaptureLost, this);
    this->Bind(wxEVT_MENU_CLOSE, &BBLTopbar::OnMenuClose, this);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnFileToolItem, this, ID_TOP_FILE_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnDropdownToolItem, this, ID_TOP_DROPDOWN_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnCalibToolItem, this, ID_CALIB);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnIconize, this, wxID_ICONIZE_FRAME);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnFullScreen, this, wxID_MAXIMIZE_FRAME);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnCloseFrame, this, wxID_CLOSE_FRAME);
    this->Bind(wxEVT_LEFT_DCLICK, &BBLTopbar::OnMouseLeftDClock, this);
    this->Bind(wxEVT_LEFT_DOWN, &BBLTopbar::OnMouseLeftDown, this);
    this->Bind(wxEVT_LEFT_UP, &BBLTopbar::OnMouseLeftUp, this);
    this->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) {
        m_last_mouse_position = wxDefaultPosition;
        event.Skip();
    });
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnOpenProject, this, wxID_OPEN);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnSaveProject, this, wxID_SAVE);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnRedo, this, wxID_REDO);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnUndo, this, wxID_UNDO);
    //this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnModelStoreClicked, this, ID_MODEL_STORE);
}

BBLTopbar::~BBLTopbar()
{
    m_file_menu_item = nullptr;
    m_dropdown_menu_item = nullptr;
    m_file_menu = nullptr;
    // ORCA: take the focus proxies down here rather than leaving them to the base class
    // destructor: losing the focus calls back into the toolbar, which has to be whole,
    // and its art provider alive, when that happens. Drop each one from the list before
    // destroying it, because that call back reaches Refresh(), which walks the list.
    while (!m_tool_proxies.empty()) {
        TopbarToolProxy* proxy = m_tool_proxies.back();
        m_tool_proxies.pop_back();
        proxy->Destroy();
    }
    m_topbar_art = nullptr; // owned and deleted by the base class
}

// ORCA: bring the focus proxies in line with the current tool list. Idempotent: it
// re-points the proxies it already has instead of recreating them, so calling it again
// never destroys the window that holds the keyboard focus. Call it after any change to
// the set of tools or to their visibility; enabling and disabling a tool needs no call,
// because the proxies read that state live.
void BBLTopbar::SyncToolProxies()
{
    size_t count = 0;
    for (size_t i = 0; i < GetToolCount(); ++i) {
        wxAuiToolBarItem* item = FindToolByIndex(int(i));
        // Skip separators, spacers, labels and the title control: only real tools.
        if (!item || item->GetKind() != wxITEM_NORMAL)
            continue;

        TopbarToolInfo info;
        if (!get_topbar_tool_info(*item, info))
            continue;

        if (count < m_tool_proxies.size())
            m_tool_proxies[count]->SetTool(item->GetId(), info);
        else
            m_tool_proxies.push_back(new TopbarToolProxy(this, item->GetId(), info));

        // A hidden tool must not be a keyboard stop, and hiding its proxy is also what
        // makes a screen reader report it as invisible.
        const wxSizerItem* sizer_item = item->GetSizerItem();
        m_tool_proxies[count]->Show(!sizer_item || sizer_item->IsShown());
        ++count;
    }

    while (m_tool_proxies.size() > count) {
        TopbarToolProxy* proxy = m_tool_proxies.back();
        m_tool_proxies.pop_back();
        proxy->Destroy();
    }

    LayoutToolProxies();
}

// ORCA: put every proxy exactly over the tool it stands for. It is the proxy window that
// assistive technology sees, and UI Automation takes an element's bounding rectangle from
// the window rather than from the MSAA location, so parked at 1x1 in the corner all six
// reported as one-pixel elements at the screen origin. Covering the tool also makes the
// proxy the window that owns those pixels - see the paint handler in TopbarToolProxy.
// Cheap and idempotent: nothing moves unless the bar was laid out again.
void BBLTopbar::LayoutToolProxies()
{
    for (TopbarToolProxy* proxy : m_tool_proxies) {
        const wxRect tool_rect = GetToolRect(proxy->GetToolId());
        // A hidden tool is parked off to the left at zero size; its proxy is hidden too
        // and has nothing to cover, so leave it where it is.
        if (tool_rect.IsEmpty() || proxy->GetRect() == tool_rect)
            continue;
        proxy->SetSize(tool_rect);
    }
}

// ORCA: draw one tool into a proxy that covers it, through the bar's own art provider, so
// the pixels are the ones the bar drew before the proxy was there - focus ring included,
// and drawn once, because the toolbar's own copy of this tool is clipped away by the
// child window. rect is the proxy's client rect; the art provider lays every element out
// relative to it, so the drawing does not care that it is no longer at the bar's origin.
void BBLTopbar::PaintTool(wxDC& dc, int tool_id, const wxRect& rect)
{
    wxAuiToolBarItem* item = FindTool(tool_id);
    if (!m_topbar_art || !item)
        return;

    m_topbar_art->DrawBackground(dc, this, rect);
    m_topbar_art->DrawButton(dc, this, *item, rect);
}

// ORCA: the proxies paint their own tools, and Refresh() does not reach child windows, so
// every repaint the bar asks for - hover, sticky, enabled state, the focus ring - has to
// be handed on or the tools under the proxies would go stale. Repaints Windows starts
// itself need no help: the invalid region covers the child windows as well.
void BBLTopbar::Refresh(bool eraseBackground, const wxRect* rect)
{
    wxAuiToolBar::Refresh(eraseBackground, rect);

    for (TopbarToolProxy* proxy : m_tool_proxies)
        proxy->Refresh();
}

// ORCA: run a tool from the keyboard or from assistive technology, through the very
// same event the toolbar sends on a mouse press, so there is one activation path and
// no synthesized mouse input. The menu openers anchor their menus to the tool's own
// rectangle already, so a menu opened this way appears in the right place.
void BBLTopbar::ActivateTool(int tool_id)
{
    if (!GetToolEnabled(tool_id) || !FindTool(tool_id))
        return;

    // The skip flags only exist to let a second mouse press on an open menu's tool
    // close it instead of reopening it; a keypress must always open the menu.
    m_skip_popup_file_menu     = false;
    m_skip_popup_dropdown_menu = false;
    m_skip_popup_calib_menu    = false;

    wxAuiToolBarEvent evt(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, tool_id);
    evt.SetEventObject(this);
    evt.SetToolId(tool_id);
    evt.SetItemRect(GetToolRect(tool_id));
    GetEventHandler()->ProcessEvent(evt);
}

// ORCA: move the keyboard focus between tool proxies, skipping hidden tools. from ==
// nullptr starts outside the bar, so step 1 lands on the first tool and step -1 on the
// last: that is Home and End. The arrow keys wrap; TAB does not, and reads the false
// return as "the bar ends here".
bool BBLTopbar::FocusAdjacentTool(const TopbarToolProxy* from, int step, bool wrap)
{
    const int count = int(m_tool_proxies.size());
    if (count == 0)
        return false;

    int index = step > 0 ? -1 : count;
    for (int i = 0; i < count; ++i)
        if (m_tool_proxies[i] == from) {
            index = i;
            break;
        }

    for (int i = 0; i < count; ++i) {
        index += step;
        if (index < 0 || index >= count) {
            if (!wrap)
                return false;
            index = (index + count) % count;
        }
        if (m_tool_proxies[index]->IsShown()) {
            m_tool_proxies[index]->SetFocus();
            return true;
        }
    }
    return false;
}

// ORCA: first shown and enabled descendant that can take the keyboard focus itself.
// IsFocusable(), not CanAcceptFocus(): the latter also returns container windows, where
// SetFocus() does nothing perceptible. Recurse before testing the child so a focusable
// container never wins over the real control inside it.
static wxWindow* find_focusable_descendant(wxWindow* win)
{
    if (!win)
        return nullptr;

    for (wxWindow* child : win->GetChildren()) {
        // IsShown() is not recursive, so hidden subtrees have to be cut off here.
        if (!child->IsShown() || !child->IsEnabled())
            continue;
        if (wxWindow* descendant = find_focusable_descendant(child))
            return descendant;
        if (child->IsFocusable())
            return child;
    }
    return nullptr;
}

// ORCA: the selected tab of the main tab strip, where the focus goes when TAB walks off
// either end of the bar. Everything on the way there can legitimately be missing - the
// bar is built before the tab panel exists, and it is not always a MainFrame's bar - so a
// null return just means there is nowhere to hand the focus to.
static wxWindow* find_main_tab_stop(wxFrame* frame)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(frame);
    if (!main_frame || !main_frame->m_tabpanel)
        return nullptr;

    ButtonsListCtrl* tab_strip = main_frame->m_tabpanel->GetBtnsListCtrl();
    if (!tab_strip || !tab_strip->IsShown())
        return nullptr;

    const int selected = main_frame->m_tabpanel->GetSelection();
    wxWindow* tab      = selected == wxNOT_FOUND ? nullptr : tab_strip->GetPageButton(size_t(selected));
    if (tab && tab->IsFocusable())
        return tab;
    return find_focusable_descendant(tab_strip);
}

// ORCA: the TAB step. It moves within the bar like the arrow keys, but at either end it
// hands the focus out instead of wrapping - MainFrame is a wxFrame, not a TAB-traversal
// container, so nothing else will, and wrapping is what made the bar a trap that only F6
// could escape. The main tab strip is the target in both directions: the bar sits above
// every other pane, so there is nothing before it, and landing on the selected tab is
// where F6 puts the focus too. Wrapping stays as the last resort, for the case where
// there is no strip to leave to, so the key is never simply dead.
void BBLTopbar::FocusNextTabStop(const TopbarToolProxy* from, bool forward)
{
    const int step = forward ? 1 : -1;
    if (FocusAdjacentTool(from, step, /*wrap*/ false))
        return;

    if (wxWindow* target = find_main_tab_stop(m_frame)) {
        target->SetFocus();
        return;
    }

    FocusAdjacentTool(from, step);
}

void BBLTopbar::SetFocusedTool(int tool_id)
{
    if (!m_topbar_art)
        return;

    m_topbar_art->SetFocusedTool(tool_id);
    Refresh();
}

#if wxUSE_ACCESSIBILITY
wxAccessible* BBLTopbar::CreateAccessible()
{
    return new WidgetAccessible(this, wxROLE_SYSTEM_TOOLBAR);
}
#endif

void BBLTopbar::OnOpenProject(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->load_project();
}

void BBLTopbar::OnSaveProject(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->save_project();
}

void BBLTopbar::OnUndo(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
#ifdef SLIC3R_CAD
    if (DesignPanel* design = main_frame->shown_design_panel()) { design->menu_undo_redo(false); return; }
#endif
    Plater* plater = main_frame->plater();
    plater->undo();
}

void BBLTopbar::OnRedo(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
#ifdef SLIC3R_CAD
    if (DesignPanel* design = main_frame->shown_design_panel()) { design->menu_undo_redo(true); return; }
#endif
    Plater* plater = main_frame->plater();
    plater->redo();
}

void BBLTopbar::EnableUndoRedoItems()
{
    this->EnableTool(m_undo_item->GetId(), true);
    this->EnableTool(m_redo_item->GetId(), true);
    this->EnableTool(m_calib_item->GetId(), true);
    Refresh();
}

void BBLTopbar::EnableUndoRedo(bool undo, bool redo)
{
    this->EnableTool(m_undo_item->GetId(), undo);
    this->EnableTool(m_redo_item->GetId(), redo);
    Refresh();
}

void BBLTopbar::DisableUndoRedoItems()
{
    this->EnableTool(m_undo_item->GetId(), false);
    this->EnableTool(m_redo_item->GetId(), false);
    this->EnableTool(m_calib_item->GetId(), false);
    Refresh();
}

void BBLTopbar::SaveNormalRect()
{
    m_normalRect = m_frame->GetRect();
}

void BBLTopbar::ShowCalibrationButton(bool show)
{
    m_calib_item->GetSizerItem()->Show(show);
    m_sizer->Layout();
    if (!show)
        m_calib_item->GetSizerItem()->SetDimension({-1000, 0}, {0, 0});
    SyncToolProxies(); // ORCA: a hidden tool must not stay a keyboard stop
    Refresh();
}

void BBLTopbar::OnModelStoreClicked(wxAuiToolBarEvent& event)
{
    //GUI::wxGetApp().load_url(wxString(wxGetApp().app_config->get_web_host_url() + MODEL_STORE_URL));
}

void BBLTopbar::SetFileMenu(wxMenu* file_menu)
{
    m_file_menu = file_menu;
}

void BBLTopbar::AddDropDownSubMenu(wxMenu* sub_menu, const wxString& title)
{
    m_top_menu.AppendSubMenu(sub_menu, title);
}

void BBLTopbar::AddDropDownMenuItem(wxMenuItem* menu_item)
{
    m_top_menu.Append(menu_item);
}

wxMenu* BBLTopbar::GetTopMenu()
{
    return &m_top_menu;
}

wxMenu* BBLTopbar::GetCalibMenu()
{
    return &m_calib_menu;
}

void BBLTopbar::SetTitle(wxString title)
{
    m_titleText = title;
    if (m_title_ctrl) {
        m_title_ctrl->SetTitle(title);
        m_title_ctrl->SetName(title); // ORCA: the project name a screen reader reads out
    }
}

void BBLTopbar::SetMaximizedSize()
{
    maximize_btn->SetBitmap(maximize_bitmap);
}

void BBLTopbar::SetWindowSize()
{
    maximize_btn->SetBitmap(window_bitmap);
}

void BBLTopbar::UpdateToolbarWidth(int width)
{
    this->SetSize(width, m_toolbar_h);
}

void BBLTopbar::Rescale() {
    int em = em_unit(this);
    wxAuiToolBarItem* item;

    /*item = this->FindTool(ID_LOGO);
    item->SetBitmap(create_scaled_bitmap("topbar_logo", nullptr, TOPBAR_ICON_SIZE));*/

    item = this->FindTool(ID_TOP_FILE_MENU);
    item->SetBitmap(create_scaled_bitmap("topbar_file", this, TOPBAR_ICON_SIZE));

    item = this->FindTool(ID_TOP_DROPDOWN_MENU);
    item->SetBitmap(create_scaled_bitmap("topbar_dropdown", this, TOPBAR_ICON_SIZE));

    //item = this->FindTool(wxID_OPEN);
    //item->SetBitmap(create_scaled_bitmap("topbar_open", nullptr, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_SAVE);
    item->SetBitmap(create_scaled_bitmap("topbar_save", this, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_UNDO);
    item->SetBitmap(create_scaled_bitmap("topbar_undo", this, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("topbar_undo_inactive", this, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_REDO);
    item->SetBitmap(create_scaled_bitmap("topbar_redo", this, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("topbar_redo_inactive", this, TOPBAR_ICON_SIZE));

    item = this->FindTool(ID_CALIB);
    item->SetBitmap(create_scaled_bitmap("calib_sf", this, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("calib_sf_inactive", this, TOPBAR_ICON_SIZE));

    if (m_title_ctrl)
        m_title_ctrl->SetTitle(m_titleText);

    /*item = this->FindTool(ID_MODEL_STORE);
    item->SetBitmap(create_scaled_bitmap("topbar_store", this, TOPBAR_ICON_SIZE));
    */

    item = this->FindTool(wxID_ICONIZE_FRAME);
    item->SetBitmap(create_scaled_bitmap("topbar_min", this, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_MAXIMIZE_FRAME);
    maximize_bitmap = create_scaled_bitmap("topbar_max", this, TOPBAR_ICON_SIZE);
    window_bitmap   = create_scaled_bitmap("topbar_win", this, TOPBAR_ICON_SIZE);
    if (m_frame->IsMaximized()) {
        item->SetBitmap(window_bitmap);
    }
    else {
        item->SetBitmap(maximize_bitmap);
    }

    item = this->FindTool(wxID_CLOSE_FRAME);
    item->SetBitmap(create_scaled_bitmap("topbar_close", this, TOPBAR_ICON_SIZE));

    Realize();
    LayoutToolProxies(); // ORCA: the tools are a different size now, so the proxies are too
}

void BBLTopbar::OnIconize(wxAuiToolBarEvent& event)
{
    m_frame->Iconize();
}

void BBLTopbar::OnFullScreen(wxAuiToolBarEvent& event)
{
#ifdef __WXGTK__
    GtkWindow* gtk_window = GTK_WINDOW(m_frame->m_widget);
    if (gtk_window_is_maximized(gtk_window)) {
        gtk_window_unmaximize(gtk_window);
    }
    else {
        m_normalRect = m_frame->GetRect();
        gtk_window_maximize(gtk_window);
    }
    return;
#endif

    if (m_frame->IsMaximized()) {
        m_frame->Restore();
    }
    else {
        m_normalRect = m_frame->GetRect();
        m_frame->Maximize();
    }
}

void BBLTopbar::OnCloseFrame(wxAuiToolBarEvent& event)
{
    m_frame->Close();
}

void BBLTopbar::OnMouseLeftDClock(wxMouseEvent& mouse)
{
    m_last_mouse_position = mouse.GetPosition();
    wxPoint mouse_pos = this->ClientToScreen(mouse.GetPosition());
    wxAuiToolBarItem* item = this->FindToolByPosition(mouse.GetX(), mouse.GetY());
    // check whether mouse is not on any tool item
    if (item != NULL && item->GetWindow() != m_title_ctrl) {
        mouse.Skip();
        return;
    }
#ifdef __W1XMSW__
    ::PostMessage((HWND) m_frame->GetHandle(), WM_NCLBUTTONDBLCLK, HTCAPTION, MAKELPARAM(mouse_pos.x, mouse_pos.y));
    return;
#endif //  __WXMSW__

    wxAuiToolBarEvent evt;
    OnFullScreen(evt);
}

void BBLTopbar::OnFileToolItem(wxAuiToolBarEvent& evt)
{
    wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(evt.GetEventObject());

    tb->SetToolSticky(evt.GetId(), true);

    if (!m_skip_popup_file_menu) {
        auto rec = this->GetToolRect(ID_TOP_FILE_MENU);
        GetParent()->PopupMenu(m_file_menu, wxPoint(rec.GetLeft(), this->GetSize().GetHeight() - 2));
    }
    else {
        m_skip_popup_file_menu = false;
    }

    // make sure the button is "un-stuck"
    tb->SetToolSticky(evt.GetId(), false);
}

void BBLTopbar::OnDropdownToolItem(wxAuiToolBarEvent& evt)
{
    wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(evt.GetEventObject());

    tb->SetToolSticky(evt.GetId(), true);

    if (!m_skip_popup_dropdown_menu) {
        auto rec = this->GetToolRect(ID_TOP_DROPDOWN_MENU);
        GetParent()->PopupMenu(&m_top_menu, wxPoint(rec.GetLeft(), this->GetSize().GetHeight() - 2));
    }
    else {
        m_skip_popup_dropdown_menu = false;
    }

    // make sure the button is "un-stuck"
    tb->SetToolSticky(evt.GetId(), false);
}

void BBLTopbar::OnCalibToolItem(wxAuiToolBarEvent &evt)
{
    wxAuiToolBar *tb = static_cast<wxAuiToolBar *>(evt.GetEventObject());

    tb->SetToolSticky(evt.GetId(), true);

    if (!m_skip_popup_calib_menu) {
        auto rec = this->GetToolRect(ID_CALIB);
        GetParent()->PopupMenu(&m_calib_menu, wxPoint(rec.GetLeft(), this->GetSize().GetHeight() - 2));
    } else {
        m_skip_popup_calib_menu = false;
    }

    // make sure the button is "un-stuck"
    tb->SetToolSticky(evt.GetId(), false);
}

void BBLTopbar::OnMouseLeftDown(wxMouseEvent& event)
{
    m_last_mouse_position = event.GetPosition();
    // Use event-relative coords converted to screen, instead of wxGetMousePosition()
    // which returns (0,0) on Wayland for global screen coordinates.
    wxPoint mouse_pos = this->ClientToScreen(event.GetPosition());
    wxPoint frame_pos = m_frame->GetScreenPosition();
    wxAuiToolBarItem* item = this->FindToolByPosition(event.GetX(), event.GetY());
    m_delta = mouse_pos - frame_pos;

    if (item == NULL || item->GetWindow() == m_title_ctrl)
    {
#ifdef __WXMSW__
        CaptureMouse();
        ReleaseMouse();
        ::PostMessage((HWND) m_frame->GetHandle(), WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(mouse_pos.x, mouse_pos.y));
        return;
#elif defined(__WXGTK__)
        // Use WM-integrated drag for smoother window movement on Linux.
        gtk_window_begin_move_drag(
            GTK_WINDOW(m_frame->m_widget),
            1,  // left mouse button
            mouse_pos.x, mouse_pos.y,
            gtk_get_current_event_time());
        return;
#else
        CaptureMouse();
#endif
    }

    event.Skip();
}

void BBLTopbar::OnMouseLeftUp(wxMouseEvent& event)
{
    m_last_mouse_position = event.GetPosition();
    wxPoint mouse_pos = this->ClientToScreen(event.GetPosition());
    if (HasCapture())
    {
        ReleaseMouse();
    }

    event.Skip();
}

void BBLTopbar::OnMouseMotion(wxMouseEvent& event)
{
    m_last_mouse_position = event.GetPosition();
    // Use event-relative coords converted to screen, instead of wxGetMousePosition()
    // which returns (0,0) on Wayland for global screen coordinates.
    wxPoint mouse_pos = this->ClientToScreen(event.GetPosition());

    if (!HasCapture()) {
        //m_frame->OnMouseMotion(event);

        event.Skip();
        return;
    }

    if (event.Dragging() && event.LeftIsDown())
    {
        // leave max state and adjust position 
        if (m_frame->IsMaximized()) {
            wxRect rect = m_frame->GetRect();
            // Filter unexcept mouse move
            if (m_delta + rect.GetLeftTop() != mouse_pos) {
                m_delta = mouse_pos - rect.GetLeftTop();
                m_delta.x = m_delta.x * m_normalRect.width / rect.width;
                m_delta.y = m_delta.y * m_normalRect.height / rect.height;
                m_frame->Restore();
            }
        }
        m_frame->Move(mouse_pos - m_delta);
    }
    event.Skip();
}

void BBLTopbar::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
}

void BBLTopbar::OnMenuClose(wxMenuEvent& event)
{
    wxAuiToolBarItem* item = this->FindToolByCurrentPosition();
    if (item == m_file_menu_item) {
        m_skip_popup_file_menu = true;
    }
    else if (item == m_dropdown_menu_item) {
        m_skip_popup_dropdown_menu = true;
    }
}

wxAuiToolBarItem* BBLTopbar::FindToolByCurrentPosition()
{
    if (m_last_mouse_position != wxDefaultPosition && GetClientRect().Contains(m_last_mouse_position))
        return this->FindToolByPosition(m_last_mouse_position.x, m_last_mouse_position.y);

#ifdef __WXGTK__
    if (Slic3r::GUI::is_running_on_wayland())
        return nullptr;
#endif

    wxPoint mouse_pos = ::wxGetMousePosition();
    wxPoint client_pos = this->ScreenToClient(mouse_pos);
    return this->FindToolByPosition(client_pos.x, client_pos.y);
}

#ifdef __WXMSW__
#include <windowsx.h>

WXLRESULT CenteredTitle::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    switch (nMsg) {
    case WM_NCHITTEST: {
        // Pass all mouse event to parent
        return HTTRANSPARENT;
    }
    }

    return wxControl::MSWWindowProc(nMsg, wParam, lParam);
}

WXLRESULT TopbarToolProxy::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    switch (nMsg) {
    case WM_NCHITTEST: {
        // ORCA: the proxy exists for the keyboard only, and it now covers its tool, so
        // this is what keeps the bar usable with a mouse: HTTRANSPARENT makes the hit test
        // carry on to the window underneath, which is the toolbar itself, and it receives
        // every press, release, double click and motion in its own coordinates, exactly as
        // it did before the proxies were there. Hover highlighting, tooltips and dragging
        // the window by the bar all run off those messages.
        return HTTRANSPARENT;
    }
    }

    return wxControl::MSWWindowProc(nMsg, wParam, lParam);
}

WXLRESULT BBLTopbar::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    switch (nMsg) {
    case WM_NCHITTEST: {
        m_last_mouse_position = ScreenToClient({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});

        wxAuiToolBarItem* item = this->FindToolByCurrentPosition();
        if (item != NULL && item->GetWindow() != m_title_ctrl) {
            break;
        }

        // Pass the event to main window if mouse is on the top bar and not on any of the buttons
        return HTTRANSPARENT;
    }
    }

    return wxAuiToolBar::MSWWindowProc(nMsg, wParam, lParam);
}
#endif
