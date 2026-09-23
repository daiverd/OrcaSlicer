#include "DropDown.hpp"
#include "Label.hpp"
#include "../I18N.hpp"

#include <cassert>
#include <algorithm>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <cstdio>
#include "slic3r/GUI/Widgets/PopupWindow.hpp"
#include <vector>
#include <utility>
#include <iterator>
#include <wx/dc.h>
#include <wx/dcclient.h>
#include <wx/display.h>
#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>
#include <wx/event.h>
#include <wx/popupwin.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/window.h>
#include <wx/utils.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

#ifdef __WXGTK__
#include <gtk/gtk.h>
#endif

#include <set>
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/StateHandler.hpp"
#include "slic3r/GUI/wxExtensions.hpp"

wxDEFINE_EVENT(EVT_DISMISS, wxCommandEvent);
wxDEFINE_EVENT(EVT_DROPDOWN_ITEM_ACTION, wxCommandEvent);

BEGIN_EVENT_TABLE(DropDown, PopupWindow)

EVT_LEFT_DOWN(DropDown::mouseDown)
EVT_LEFT_UP(DropDown::mouseReleased)
EVT_MOUSE_CAPTURE_LOST(DropDown::mouseCaptureLost)
EVT_MOTION(DropDown::mouseMove)
EVT_MOUSEWHEEL(DropDown::mouseWheelMoved)
// CHAR_HOOK rather than KEY_DOWN: it reaches the focused popup before the keys
// are offered to an enclosing dialog, which would otherwise cancel on Escape.
EVT_CHAR_HOOK(DropDown::charHook)

// catch paint events
EVT_PAINT(DropDown::paintEvent)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

DropDown::DropDown(std::vector<Item> &items)
    : items(items)
    , state_handler(this)
    , border_color(0xDBDBDB)
    , text_color(std::make_pair(0x909090, (int) StateColor::Disabled),
        std::make_pair(0x363636, (int) StateColor::Normal))
    , selector_border_color(std::make_pair(0x009688, (int) StateColor::Hovered),
        std::make_pair(*wxWHITE, (int) StateColor::Normal))
    , selector_background_color(std::make_pair(0xBFE1DE, (int) StateColor::Checked), // ORCA updated background color for checked item
        std::make_pair(*wxWHITE, (int) StateColor::Normal))
{
}

DropDown::DropDown(wxWindow *parent, std::vector<Item> &items, long style)
    : DropDown(items)
{
    Create(parent, style);
}

void DropDown::Create(wxWindow *parent, long style)
{
    PopupWindow::Create(parent, wxPU_CONTAINS_CONTROLS);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(*wxWHITE);
    state_handler.attach({&border_color, &text_color, &selector_border_color, &selector_background_color});
    state_handler.update_binds();
    if ((style & DD_NO_CHECK_ICON) == 0)
        check_bitmap = ScalableBitmap(this, "checked", 16);
    arrow_bitmap = ScalableBitmap(this, "hms_arrow", 16);
    text_off = style & DD_NO_TEXT;

    // BBS set default font
    SetFont(Label::Body_14);
#ifdef __WXOSX__
    // PopupWindow releases mouse on idle, which may cause various problems,
    //  such as losting mouse move, and dismissing soon on first LEFT_DOWN event.
    Bind(wxEVT_IDLE, [] (wxIdleEvent & evt) {});
#endif
}

void DropDown::Invalidate(bool clear)
{
    if (clear) {
        selection = hover_item = -1;
        offset = wxPoint();
    }
    assert(selection < (int) items.size());
    need_sync = true;
}

void DropDown::SetSelection(int n)
{
    if (n >= (int) items.size())
        n = -1;
    if (selection == n) return;
    selection = n;
    if (need_sync) { // for icon Size
        messureSize();
        need_sync = true;
    }
    if (subDropDown)
        subDropDown->SetSelection(n);
    paintNow();
}

wxString DropDown::GetValue() const
{
    return selection >= 0 ? items[selection].text : wxString();
}

void DropDown::SetValue(const wxString &value)
{
    auto i    = std::find_if(items.begin(), items.end(), [&value](Item & item) { return item.text == value; });
    selection = i == items.end() ? -1 : std::distance(items.begin(), i);
}

void DropDown::SetCornerRadius(double radius)
{
    this->radius = radius;
    paintNow();
}

void DropDown::SetBorderColor(StateColor const &color)
{
    border_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetSelectorBorderColor(StateColor const &color)
{
    selector_border_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetTextColor(StateColor const &color)
{
    text_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetSelectorBackgroundColor(StateColor const &color)
{
    selector_background_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetUseContentWidth(bool use, bool limit_max_content_width)
{
    if (use_content_width == use)
        return;
    use_content_width = use;
    this->limit_max_content_width = limit_max_content_width;
    need_sync = true;
    messureSize();
}

void DropDown::SetAlignIcon(bool align) { align_icon = align; }

void DropDown::Rescale()
{
    need_sync = true;
}

bool DropDown::HasDismissLongTime()
{
    auto now = boost::posix_time::microsec_clock::universal_time();
    return !IsShown() &&
        (now - dismissTime).total_milliseconds() >= 20;
}

void DropDown::Popup(wxWindow *focus)
{
#ifdef __WXGTK__
    if (!mainDropDown && m_widget) {
        // Data-view cell editors can receive focus before wxGTK infers a
        // native popup parent, so provide the current toplevel explicitly.
        GtkWindow *transient_parent = nullptr;
        for (wxWindow *win = GetParent(); win; win = win->GetParent()) {
            GtkWidget *widget = static_cast<GtkWidget *>(win->GetHandle());
            if (!widget)
                continue;
            GtkWidget *top = gtk_widget_get_toplevel(widget);
            if (GTK_IS_WINDOW(top)) {
                transient_parent = GTK_WINDOW(top);
                break;
            }
        }
        if (transient_parent)
            gtk_window_set_transient_for(GTK_WINDOW(m_widget), transient_parent);
    }
#endif
    PopupWindow::Popup(focus);
    // ORCA: the native focus event only names the window; follow it with the row
    if (focus == this)
        CallAfter([this] { notifyFocusedRow(); });
}

void DropDown::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxBufferedPaintDC dc(this);
    render(dc);
}

/*
 * Alternatively, you can use a clientDC to paint on the panel
 * at any time. Using this generally does not free you from
 * catching paint events, since it is possible that e.g. the window
 * manager throws away your drawing when the window comes to the
 * background, and expects you will redraw it when the window comes
 * back (by sending a paint event).
 */
void DropDown::paintNow()
{
    // depending on your system you may need to look at double-buffered dcs
    //wxClientDC dc(this);
    //render(dc);
    Refresh();
}

static wxSize GetBmpSize(wxBitmap & bmp)
{
    if (!bmp.IsOk())
        return wxSize(0, 0);
#ifdef __APPLE__
    return bmp.GetScaledSize();
#else
    return bmp.GetSize();
#endif
}

// The x-range [first, second) of the column at the right of the rows that holds the items' action
// icons, empty when no item has one. render() draws the icons in it and mouseReleased() hit-tests
// clicks against it, so the drawn icon and its clickable area always match.
std::pair<int, int> DropDown::actionColumn() const
{
    if (actionIconWidth <= 0)
        return {0, 0};
    int right = rowSize.x;
    if (rowSize.y * count > GetSize().y) // position bar
        right -= 6;
    return {right - actionIconWidth - FromDIP(16), right};
}

static void _DrawSplitItem(const wxWindow* w, wxDC& dc, wxString split_text, wxPoint start_pt, int item_width, int item_height)
{
    // save dc
    auto pre_clr = dc.GetTextForeground();
    auto pre_pen = dc.GetPen();
    dc.SetTextForeground(wxColour(172, 172, 172));//GRAY 500
    dc.SetPen(wxColour(166, 169, 170));//GRAY 400
    // miner font
    auto font = w->GetFont();
    font.SetPointSize(font.GetPointSize() - 3);
    dc.SetFont(font);

    int spacing = w->FromDIP(8);

    if (!split_text.empty())// Paiting: text + spacing + line + spacing
    {
        int max_content_width = item_width - start_pt.x - 2 * spacing;
        wxSize tSize = dc.GetMultiLineTextExtent(split_text);
        if (tSize.x > max_content_width)
        {
            split_text = wxControl::Ellipsize(split_text, dc, wxELLIPSIZE_END, max_content_width);
            tSize = dc.GetMultiLineTextExtent(split_text);
        }

        dc.SetFont(font);
        dc.DrawText(split_text, start_pt);

        int line_width = item_width - start_pt.x - tSize.x - 2 * spacing;
        int line_y = start_pt.y + (tSize.GetHeight() / 2);
        dc.DrawLine(start_pt.x + tSize.x + spacing, line_y, start_pt.x + tSize.x + line_width + spacing, line_y);// draw right line
    }
    else// Paiting: line + spacing
    {
        int line_y = start_pt.y + (item_height / 2);
        int line_width = item_width - start_pt.x - spacing;
        dc.DrawLine(start_pt.x, line_y, start_pt.x + line_width, line_y);// draw line
    }

    // restore dc
    dc.SetTextForeground(pre_clr);
    dc.SetPen(pre_pen);
    dc.SetFont(w->GetFont());
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void DropDown::render(wxDC &dc)
{
    if (items.size() == 0) return;
    int states = state_handler.states();
    if (subDropDown)
        states |= subDropDown->state_handler.states();
    dc.SetPen(wxPen(border_color.colorForStates(states)));
    dc.SetBrush(wxBrush(StateColor::darkModeColorFor(GetBackgroundColour())));
    // if (GetWindowStyle() & wxBORDER_NONE)
    //    dc.SetPen(wxNullPen);

    // draw background
    wxSize size = GetSize();
    if (radius == 0)
        dc.DrawRectangle(0, 0, size.x, size.y);
    else
        dc.DrawRoundedRectangle(0, 0, size.x, size.y, radius);

    int selected_item = selectedItem();
    int hover_index   = hoverIndex();

    // draw hover rectangle
    wxRect rcContent = {{0, offset.y}, rowSize};
    if (hover_item >= 0 && (states & StateColor::Hovered) && (hover_index < 0 || !(items[hover_index].style & DD_ITEM_STYLE_SPLIT_ITEM))) {
        rcContent.y += rowSize.y * hover_item;
        if (rcContent.GetBottom() > 0 && rcContent.y < size.y) {
            if (selected_item == hover_item)
                dc.SetBrush(wxBrush(selector_background_color.colorForStates(states | StateColor::Checked)));
            dc.SetPen(wxPen(selector_border_color.colorForStates(states)));
            rcContent.Deflate(4, 1);
            dc.DrawRectangle(rcContent);
            rcContent.Inflate(4, 1);
        }
        rcContent.y = offset.y;
    }
    // draw checked rectangle
    if (selected_item >= 0 && (selected_item != hover_item || (states & StateColor::Hovered) == 0)) {
        rcContent.y += rowSize.y * selected_item;
        if (rcContent.GetBottom() > 0 && rcContent.y < size.y) {
            dc.SetBrush(wxBrush(selector_background_color.colorForStates(states | StateColor::Checked)));
            dc.SetPen(wxPen(selector_background_color.colorForStates(states)));
            rcContent.Deflate(4, 1);
            dc.DrawRectangle(rcContent);
            rcContent.Inflate(4, 1);
        }
        rcContent.y = offset.y;
    }
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    {
        wxSize offset = (rowSize - textSize) / 2;
        rcContent.Deflate(0, offset.y);
    }

    // draw position bar
    if (rowSize.y * count > size.y) {
        int    height = rowSize.y * count;
        wxRect rect = {size.x - 6, -offset.y * size.y / height, 4,
                       size.y * size.y / height};
        dc.SetPen(wxPen(border_color.defaultColor()));
        dc.SetBrush(wxBrush(*wxLIGHT_GREY));
        dc.DrawRoundedRectangle(rect, 2);
        rcContent.width -= 6;
    }

    // draw check icon
    rcContent.x += 5;
    rcContent.width -= 5;
    if (check_bitmap.bmp().IsOk()) {
        auto szBmp = check_bitmap.GetBmpSize();
        if (selected_item >= 0) {
            wxPoint pt = rcContent.GetLeftTop();
            pt.y += (rcContent.height - szBmp.y) / 2;
            pt.y += rowSize.y * selected_item;
            if (pt.y + szBmp.y > 0 && pt.y < size.y)
                dc.DrawBitmap(check_bitmap.bmp(), pt);
        }
        rcContent.x += szBmp.x + 5;
        rcContent.width -= szBmp.x + 5;
    }

    const auto action_column = actionColumn();
    if (action_column.second > action_column.first)
        rcContent.width = action_column.first - rcContent.x;

    std::set<wxString> groups;
    // draw texts & icons
    int index = 0;
    for (int i = 0; i < items.size(); ++i) {
        auto &item = items[i];
        int states2 = states;
        if ((item.style & DD_ITEM_STYLE_DISABLED) != 0)
            states2 &= ~StateColor::Enabled;
        // Skip by group
        if (group.IsEmpty()) {
            if (!item.group_key.IsEmpty()) {
                if (groups.find(item.group_key) != groups.end())
                    continue;
                groups.insert(item.group_key);
                if (!item.group_key.IsEmpty()) {
                    bool disabled = true;
                    for (int j = i + 1; j < items.size(); ++j) {
                        if (items[j].group_key == item.group_key && (items[j].style & DD_ITEM_STYLE_DISABLED) == 0) {
                            disabled = false;
                            break;
                        }
                    }
                    if (!disabled)
                        states2 |= StateColor::Enabled;
                }
            }
        } else {
            if (item.group_key != group)
                continue;
        }
        bool is_hover = index == hover_item;
        ++index;
        if (rcContent.GetBottom() < 0) {
            rcContent.y += rowSize.y;
            continue;
        }
        if (rcContent.y > size.y) break;
        wxPoint pt   = rcContent.GetLeftTop();

        if (item.style & DD_ITEM_STYLE_SPLIT_ITEM) {
            _DrawSplitItem(this, dc, item.text, pt, rowSize.GetWidth(), rowSize.GetHeight());
            rcContent.y += rowSize.GetHeight();
            continue;
        }

        auto &  icon  = item.icon;
        auto size2 = GetBmpSize(icon);
        if (iconSize.x > 0) {
            if (icon.IsOk()) {
                pt.y += (rcContent.height - size2.y) / 2;
                dc.DrawBitmap(icon, pt);
            }
            pt.x += iconSize.x + 5;
            pt.y = rcContent.y;
        } else if (icon.IsOk()) {
            pt.y += (rcContent.height - size2.y) / 2;
            dc.DrawBitmap(icon, pt);
            pt.x += size2.x + 5;
            pt.y = rcContent.y;
        }
        auto text = group.IsEmpty()
                        ? (item.group_key.IsEmpty() ? item.text : item.group_label)
                        : item.text;
        if (item.action_icon.IsOk()) {
            auto action_size = GetBmpSize(item.action_icon);
            wxPoint action_pt(action_column.second - action_size.x - FromDIP(8),
                              rcContent.y + (rcContent.height - action_size.y) / 2);
            dc.DrawBitmap(item.action_icon, action_pt);
        }

        if (!text_off && !text.IsEmpty()) {
            wxSize tSize = dc.GetMultiLineTextExtent(text);
            if (pt.x + tSize.x > rcContent.GetRight()) {
                int max_text_width = rcContent.GetRight() - pt.x;
                if (max_text_width <= 0) {
                    text.clear();
                } else {
                    if (is_hover && item.tip.IsEmpty())
                        SetToolTip(text);
                    text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, max_text_width);
                }
            }
            pt.y += (rcContent.height - textSize.y) / 2;
            dc.SetFont(GetFont());
            // Dimmed items stay selectable, so they only borrow the disabled text tone rather
            // than taking the disabled state itself.
            const int text_states = (item.style & DD_ITEM_STYLE_DIMMED) ? (states2 & ~StateColor::Enabled) : states2;
            dc.SetTextForeground(text_color.colorForStates(text_states));
            dc.DrawText(text, pt);
            if (group.IsEmpty() && !item.group_key.IsEmpty()) {
                auto szBmp = arrow_bitmap.GetBmpSize();
                pt.x = rcContent.GetRight() - szBmp.x - 5;
                pt.y = rcContent.y + (rcContent.height - szBmp.y) / 2;
                dc.DrawBitmap(arrow_bitmap.bmp(), pt);
            }
        }
        rcContent.y += rowSize.y;
    }
}

int DropDown::hoverIndex() { return itemIndex(hover_item); }

int DropDown::itemIndex(int hover) const
{
    if (hover < 0)
        return -1;
    // BUG FIX: Can't take the shortcut if subDropDown exists (which means there are groups)
    // because we need to detect group headers which return negative indices
    if (count == items.size() && subDropDown == nullptr)
        return hover;
    int index = -1;
    std::set<wxString> groups;
    for (int i = 0; i < items.size(); ++i) {
        auto &item = items[i];
        // Skip by group
        if (group.IsEmpty()) {
            if (!item.group_key.IsEmpty()) {
                if (groups.find(item.group_key) == groups.end())
                    groups.insert(item.group_key);
                else
                    continue;
            }
        } else {
            if (item.group_key != group)
                continue;
        }
        if (++index == hover)
            return (item.group_key.IsEmpty() || !group.IsEmpty()) ? i : -i - 2;
    }
    return -1;
}

int DropDown::selectedItem()
{
    if (selection < 0)
        return -1;
    // BUG FIX: Can't take the shortcut if subDropDown exists (which means there are groups)
    // because the visual position differs from the actual item index when groups are shown
    if (count == items.size() && subDropDown == nullptr)
        return selection;
    auto & sel = items[selection];
    if (group.IsEmpty() ? !sel.group_key.IsEmpty() : sel.group_key != group)
        return -1;
    if (selection == 0)
        return 0;
    int                index = 0;
    std::set<wxString> groups;
    for (size_t i = 0; i < selection; ++i) {
        auto &item = items[i];
        // Skip by group
        if (group.IsEmpty()) {
            if (!item.group_key.IsEmpty()) {
                if (groups.find(item.group_key) == groups.end())
                    groups.insert(item.group_key);
                else
                    continue;
            }
        } else {
            if (item.group_key != group)
                continue;
        }
        ++index;
    }
    return index;
}

void DropDown::messureSize()
{
    if (!need_sync) return;
    textSize = wxSize();
    iconSize = wxSize();
    actionIconWidth = 0;
    count = 0;
    wxClientDC dc(GetParent() ? GetParent() : this);
    dc.SetFont(GetFont());
    std::set<wxString> groups;
    for (size_t i = 0; i < items.size(); ++i) {
        auto &item = items[i];
        // Skip by group
        if (group.IsEmpty()) {
            if (!item.group_key.IsEmpty()) {
                if (groups.find(item.group_key) == groups.end())
                    groups.insert(item.group_key);
                else
                    continue;
            }
        } else {
            if (item.group_key != group)
                continue;
        }
        ++count;
        wxSize size1;
        if (!text_off) {
            auto text = group.IsEmpty()
                        ? (item.group_key.IsEmpty() ? item.text : item.group_label)
                        : item.text;
            size1 = dc.GetMultiLineTextExtent(text);
            if (group.IsEmpty() && !item.group_key.IsEmpty())
                size1.x += 5 + arrow_bitmap.GetBmpWidth();
        }
        if (item.icon.IsOk()) {
            wxSize size2 = GetBmpSize(item.icon);
            if (size2.x > iconSize.x)
                iconSize = size2;
            if (!align_icon) {
                size1.x += size2.x + (text_off ? 0 : 5);
            }
        }
        if (item.action_icon.IsOk())
            actionIconWidth = std::max(actionIconWidth, GetBmpSize(item.action_icon).x);
        if (size1.x > textSize.x) textSize = size1;
    }
    // Every row leaves room for the action column, as render() and actionColumn() lay it out.
    if (actionIconWidth > 0)
        textSize.x += actionIconWidth + FromDIP(16);
    if (!align_icon) iconSize.x = 0;
    wxSize szContent = textSize;
    if (szContent.x < FromDIP(120) && !use_content_width)
        szContent.x = FromDIP(120);
    szContent.x += 10;
    if (check_bitmap.bmp().IsOk()) {
        auto szBmp = check_bitmap.GetBmpSize();
        szContent.x += szBmp.x + 5;
    }
    if (iconSize.x > 0) szContent.x += iconSize.x + (text_off ? 0 : 5);
    if (iconSize.y > szContent.y) szContent.y = iconSize.y;
    szContent.y += 10;
    if (count > 15) szContent.x += 6;
    if (GetParent() && group.IsEmpty()) {
        auto x = GetParent()->GetSize().x;
        if (x > 0 && (!use_content_width || x > szContent.x))
            szContent.x = x;
    }
    rowSize = szContent;
    if (limit_max_content_width) {
        wxSize parent_size = GetParent()->GetSize();
        if (rowSize.x > parent_size.x * 2) {
            rowSize.x = 2 * parent_size.x;
            szContent = rowSize;
        }
    }
    szContent.y *= std::min((size_t) 15, std::max(count, (size_t) 1));
    szContent.y += items.size() > 15 ? rowSize.y / 2 : 0;
    wxWindow::SetSize(szContent);
#ifdef __WXGTK__
    // Gtk has a wrapper window for popup widget
    if (szContent.x > 0 && szContent.y > 0)
        gtk_window_resize (GTK_WINDOW (m_widget), szContent.x, szContent.y);
#endif
    if (!groups.empty() && subDropDown == nullptr) {
        subDropDown = new DropDown(items);
        subDropDown->mainDropDown = this;
        subDropDown->check_bitmap      = check_bitmap;
        subDropDown->text_off          = text_off;
        subDropDown->use_content_width = true;
        subDropDown->Create(GetParent());
#ifdef __WXGTK__
        // Orca: On Wayland, while the sub holds an xdg_popup grab, motion events for
        // the cursor over main may not be delivered (Mutter drops motion
        // outside the grabbing surface). Poll on idle and synthesize a
        // mouseMove on main so its hover highlight tracks and it can dismiss
        // the sub when the cursor leaves the parent (group) item.
        DropDown* sub = subDropDown;
        sub->Bind(wxEVT_IDLE, [sub](wxIdleEvent& e) {
            e.Skip();
            if (!sub->IsShown() || !sub->mainDropDown->IsShown())
                return;
            wxPoint screen_pt = wxGetMousePosition();
            if (sub->GetScreenRect().Contains(screen_pt) || !sub->mainDropDown->GetScreenRect().Contains(screen_pt))
                return;
            wxPoint main_pt = sub->mainDropDown->ScreenToClient(screen_pt);
            wxMouseEvent ev(wxEVT_MOTION);
            ev.SetEventObject(sub->mainDropDown);
            ev.m_x = main_pt.x;
            ev.m_y = main_pt.y;
            sub->mainDropDown->mouseMove(ev);
            e.RequestMore();
        });
        // Orca: the submenu holds the GTK grab, so GTK gives it the wheel events of the whole
        // chain. GDK names the window under the cursor, so re-emit main's events on main.
        g_signal_connect(sub->m_widget, "event", G_CALLBACK(+[](GtkWidget *, GdkEvent *event, gpointer data) -> gboolean {
            auto *sub = static_cast<DropDown *>(data);
            DropDown *main = sub->mainDropDown;
            if (event->type != GDK_SCROLL || !main || !main->IsShown() || !sub->IsShown())
                return FALSE;
            gpointer owner = nullptr;
            if (event->any.window)
                gdk_window_get_user_data(event->any.window, &owner);
            GtkWidget *widget   = (owner && GTK_IS_WIDGET(owner)) ? GTK_WIDGET(owner) : nullptr;
            bool       for_main = widget && (widget == main->m_widget || gtk_widget_is_ancestor(widget, main->m_widget));
            bool       for_sub  = widget && (widget == sub->m_widget || gtk_widget_is_ancestor(widget, sub->m_widget));
            if (!for_main && for_sub) {
                // The compositor may keep the pointer focus on the submenu while the cursor is over main.
                wxPoint pt((int) event->scroll.x, (int) event->scroll.y);
                if (sub->GetClientRect().Contains(pt))
                    return FALSE;
                wxPoint off = sub->GetScreenRect().GetTopLeft() - main->GetScreenRect().GetTopLeft();
                event->scroll.x += off.x;
                event->scroll.y += off.y;
                for_main = true;
            }
            if (!for_main)
                return FALSE;
            // wx connects its mouse handlers to GetConnectWidget(), the inner widget of the popup.
            gtk_propagate_event(main->GetConnectWidget(), event);
            return TRUE;
        }), sub);
#endif
        subDropDown->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &e) {
            e.SetEventObject(this);
            e.SetId(GetId());
            GetEventHandler()->ProcessEvent(e);
        });
        subDropDown->Bind(EVT_DROPDOWN_ITEM_ACTION, [this](wxCommandEvent &e) {
            e.SetEventObject(this);
            e.SetId(GetId());
            GetEventHandler()->ProcessEvent(e);
        });
    }
    need_sync = false;
}

void DropDown::autoPosition()
{
    messureSize();
    wxPoint pos;
    wxSize  off;
    if (mainDropDown) {
        pos = mainDropDown->ClientToScreen(wxPoint(0, 0));
        off = mainDropDown->GetSize();
        pos.x += 6;
        pos.y += mainDropDown->hover_item * rowSize.y + rowSize.y + mainDropDown->offset.y;
        off.x -= 12;
        off.y = -rowSize.y;
    } else {
        pos = GetParent()->ClientToScreen(wxPoint(0, 0));
        off = GetParent()->GetSize();
        pos.y -= 6;
        off.x = 0;
        off.y += 12;
    }
    wxPoint old = GetPosition();
    wxSize size = GetSize();
    Position(pos, off);
    if (old != GetPosition()) {
        size = rowSize;
        size.y *= std::min((size_t) 15, count);
        size.y += count > 15 ? rowSize.y / 2 : 0;
        if (size != GetSize()) {
            wxWindow::SetSize(size);
            offset = wxPoint();
            Position(pos, off);
        }
    }
    if (GetPosition().y > pos.y) {
        // may exceed
        auto drect = wxDisplay(GetParent()).GetGeometry();
        if (GetPosition().y + size.y + 10 > drect.GetBottom()) {
            int available_height = drect.GetBottom() - GetPosition().y - 10;
            if (available_height < rowSize.y * 2)
                return;
            if (use_content_width && count <= 15) size.x += 6;
            size.y = available_height;
            wxWindow::SetSize(size);
            if (selection >= 0) {
                if (offset.y + rowSize.y * (selection + 1) > size.y)
                    offset.y = size.y - rowSize.y * (selection + 1);
                else if (offset.y + rowSize.y * selection < 0)
                    offset.y = -rowSize.y * selection;
            }
        }
    }
}

void DropDown::setHoverItem(int hover, bool open_group)
{
    hover_item = hover;
    int index  = hoverIndex();
    if (index < -1 && open_group) {
        auto & drop = *subDropDown;
        drop.group  = items[-index - 2].group_key;
        drop.need_sync = true;
        drop.messureSize();
#ifdef __WXGTK__
        // wxGTK wraps popup contents in a native GtkWindow. Make the submenu
        // transient for the currently mapped parent popup window before
        // positioning/showing it, so wlroots/Hyprland sees the topmost parent.
        if (m_widget && drop.m_widget)
            gtk_window_set_transient_for(GTK_WINDOW(drop.m_widget), GTK_WINDOW(m_widget));
#endif
        drop.autoPosition();
        drop.paintNow();
        if (!drop.IsShown())
            drop.Popup(&drop);
    } else if (index != -1) { // an item, or a group header the keys only highlight
        if (subDropDown) {
            subDropDown->group.clear();
            if (subDropDown->IsShown())
                subDropDown->Dismiss();
        }
        if (index >= 0)
            SetToolTip(items[index].tip);
    }
}

int DropDown::findSelectable(int hover, int delta) const
{
    for (; hover >= 0 && hover < (int) count; hover += delta) {
        int index = itemIndex(hover);
        // Group headers (index < -1) open their group's list; separators and disabled items are not selectable
        if (index < -1 || (index >= 0 && (items[index].style & (DD_ITEM_STYLE_SPLIT_ITEM | DD_ITEM_STYLE_DISABLED)) == 0))
            return hover;
    }
    return -1;
}

// Highlight a row from the keyboard: headers stay closed until Right or Enter opens them
void DropDown::moveHighlight(int hover)
{
    setHoverItem(hover, false);
    // The mouse can only hover visible rows, but arrows walk the whole list,
    // so scroll the new row into view the way autoPosition() does for selection.
    wxSize size = GetSize();
    if (offset.y + rowSize.y * (hover + 1) > size.y)
        offset.y = size.y - rowSize.y * (hover + 1);
    else if (offset.y + rowSize.y * hover < 0)
        offset.y = -rowSize.y * hover;
    // render() draws the highlight only while the window carries the hovered
    // state, which is otherwise fed by enter/leave events from the mouse.
    state_handler.set_state(StateHandler::Hovered, StateHandler::Hovered);
    paintNow();
    notifyFocusedRow();
}

// Show the highlighted header's group in the second list and move into it
void DropDown::openHoverGroup()
{
    setHoverItem(hover_item); // positions and shows the second list, which takes the focus
    DropDown &sub = *subDropDown;
    if (wxWindow::FindFocus() != &sub)
        sub.SetFocus();
    int start = sub.selectedItem();
    if (start < 0)
        start = sub.findSelectable(0, 1);
    if (start >= 0)
        sub.moveHighlight(start);
}

// Leave a group's list for the header that opened it, keeping the main list open
void DropDown::closeToMain()
{
    Dismiss(); // not DismissAndNotify: OnDismiss closes the main list unless the pointer is over it
    group.clear();
#ifdef __WIN32__
    SetActiveWindow(mainDropDown->GetHandle());
#endif
    mainDropDown->SetFocus();
    mainDropDown->notifyFocusedRow();
}

void DropDown::selectHoverItem()
{
    if (hover_item < 0)
        return;
    if (subDropDown == nullptr || subDropDown->group.empty()) { // not a group header
        sendDropDownEvent();
        if (mainDropDown)
            mainDropDown->hover_item = -1; // To Dismiss mainDropDown
        DismissAndNotify();
    } else {
        subDropDown->Popup(subDropDown);
    }
}

void DropDown::mouseDown(wxMouseEvent& event)
{
    // Receivce unexcepted LEFT_DOWN on Mac after OnDismiss
    if (!IsShown())
        return;
    // force calc hover item again
    mouseMove(event);
    pressedDown = true;
    CaptureMouse();
    dragStart   = event.GetPosition();
}

void DropDown::mouseReleased(wxMouseEvent& event)
{
    if (pressedDown) {
        dragStart = wxPoint();
        pressedDown = false;
        if (HasCapture())
            ReleaseMouse();
        if (hover_item >= 0 && (subDropDown == nullptr || subDropDown->group.empty())) { // not moved
            int index = hoverIndex();
            if (index >= 0 && items[index].action_icon.IsOk()) {
                const auto action_column = actionColumn();
                if (event.GetX() >= action_column.first && event.GetX() < action_column.second) {
                    sendDropDownActionEvent(index);
                    paintNow();
                    return;
                }
            }
        }
        selectHoverItem();
    }
}

void DropDown::mouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    wxMouseEvent evt;
    mouseReleased(evt);
}

void DropDown::mouseMove(wxMouseEvent &event)
{
    wxPoint pt  = event.GetPosition();
#ifdef __WXOSX__
    if (mainDropDown) {
        auto size = GetSize();
        if (pt.x < 0 || pt.y < 0 || pt.x >= size.x || pt.y >= size.y) {
            auto diff = GetPosition() - mainDropDown->GetPosition();
            event.SetX(pt.x + diff.x);
            event.SetY(pt.y + diff.y);
            mainDropDown->mouseMove(event);
            return;
        }
    }
#endif
    if (pressedDown) {
        wxPoint pt2 = offset + pt - dragStart;
        wxSize  size = GetSize();
        dragStart    = pt;
        if (pt2.y > 0)
            pt2.y = 0;
        else if (pt2.y + rowSize.y * int(count) < size.y)
            pt2.y = size.y - rowSize.y * int(count);
        if (pt2.y != offset.y) {
            offset = pt2;
            hover_item = -1; // moved
        } else {
            return;
        }
    }
    if (!pressedDown || hover_item >= 0) {
        int hover = (pt.y - offset.y) / rowSize.y;
        if (hover >= (int) count) hover = -1;
        if (hover == hover_item) return;
        setHoverItem(hover);
    }
    paintNow();
}

void DropDown::mouseWheelMoved(wxMouseEvent &event)
{
    auto delta = event.GetWheelRotation();
    wxSize  size  = GetSize();
    wxPoint pt2   = offset + wxPoint{0, delta};
    if (pt2.y > 0)
        pt2.y = 0;
    else if (pt2.y + rowSize.y * int(count) < size.y)
        pt2.y = size.y - rowSize.y * int(count);
    if (pt2.y != offset.y) {
        offset = pt2;
    } else {
        return;
    }
    int hover = (event.GetPosition().y - offset.y) / rowSize.y;
    if (hover >= (int) count) hover = -1;
    if (hover != hover_item) {
        hover_item = hover;
        if (auto index = hoverIndex(); index >= 0)
            SetToolTip(items[index].tip);
    }
    paintNow();
}

void DropDown::charHook(wxKeyEvent &event)
{
    const int code = event.GetKeyCode();
    // Alt+Up, Alt+Down and F4 close the whole list, as they do a native combo box's
    if (code == WXK_F4 || (event.AltDown() && (code == WXK_UP || code == WXK_DOWN || code == WXK_NUMPAD_UP || code == WXK_NUMPAD_DOWN))) {
        DropDown *top = mainDropDown ? mainDropDown : this;
        if (mainDropDown)
            Dismiss();
        top->DismissAndNotify();
        return;
    }
    int base  = focusedRow();
    int hover = -1;
    switch (code) {
    case WXK_DOWN:
    case WXK_NUMPAD_DOWN:
        hover = findSelectable(base < 0 ? 0 : base + 1, 1);
        break;
    case WXK_UP:
    case WXK_NUMPAD_UP:
        hover = findSelectable(base < 0 ? (int) count - 1 : base - 1, -1);
        break;
    case WXK_HOME:
    case WXK_NUMPAD_HOME:
        hover = findSelectable(0, 1);
        break;
    case WXK_END:
    case WXK_NUMPAD_END:
        hover = findSelectable((int) count - 1, -1);
        break;
    case WXK_RIGHT:
    case WXK_NUMPAD_RIGHT:
        if (hover_item >= 0 && hoverIndex() < -1)
            openHoverGroup();
        return;
    case WXK_LEFT:
    case WXK_NUMPAD_LEFT:
        if (mainDropDown)
            closeToMain();
        return;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
    case WXK_SPACE:
        if (hover_item < 0)
            DismissAndNotify(); // nothing highlighted, keep the current selection
        else if (hoverIndex() < -1)
            openHoverGroup();
        else
            selectHoverItem();
        return;
    case WXK_ESCAPE:
        if (mainDropDown)
            closeToMain(); // a group's list closes back to its header
        else
            DismissAndNotify();
        return;
    default:
        event.Skip();
        return;
    }
    if (hover >= 0 && hover != hover_item)
        moveHighlight(hover);
}

// currently unused events
void DropDown::sendDropDownEvent()
{
    int index = hoverIndex();
    if (index < 0 || (items[index].style & DD_ITEM_STYLE_DISABLED))
        return;
    wxCommandEvent event(wxEVT_COMBOBOX, GetId());
    event.SetEventObject(this);
    event.SetInt(index);
    event.SetString(items[index].text);
    GetEventHandler()->ProcessEvent(event);
}

void DropDown::sendDropDownActionEvent(int index)
{
    if (index < 0 || index >= int(items.size()) || (items[index].style & DD_ITEM_STYLE_DISABLED))
        return;
    wxCommandEvent event(EVT_DROPDOWN_ITEM_ACTION, GetId());
    event.SetEventObject(this);
    event.SetInt(index);
    event.SetString(items[index].text);
    GetEventHandler()->ProcessEvent(event);
}

int DropDown::focusedRow()
{
    if (hover_item >= 0)
        return hover_item;
    if (int row = selectedItem(); row >= 0 || selection < 0 || !group.IsEmpty())
        return row;
    // The selection sits inside a group, so this list shows it only as its group's header
    for (int row = 0; row < (int) count; ++row)
        if (int index = itemIndex(row); index < -1 && items[-index - 2].group_key == items[selection].group_key)
            return row;
    return -1;
}

// The text render() draws for a row: a group header shows its group's label
wxString DropDown::rowText(int row) const
{
    const int index = itemIndex(row);
    if (index >= 0)
        return items[index].text;
    if (index < -1)
        return items[-index - 2].group_label;
    return {};
}

void DropDown::notifyFocusedRow()
{
#if wxUSE_ACCESSIBILITY
    if (!IsShown() || wxWindow::FindFocus() != this)
        return;
    const int row = focusedRow();
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, row >= 0 ? row + 1 : wxACC_SELF);
#endif
}

#if wxUSE_ACCESSIBILITY

// ORCA: the popup is custom drawn, so a screen reader would otherwise see one nameless
// pane. Expose it as a list whose children are the rows it draws, group headers
// included, with the highlighted row (else the selected one) reported as focused.
class DropDownAccessible : public wxWindowAccessible
{
public:
    explicit DropDownAccessible(DropDown *drop) : wxWindowAccessible(drop), drop(drop) {}

    wxAccStatus GetChildCount(int *count) override
    {
        *count = int(drop->count);
        return wxACC_OK;
    }

    wxAccStatus GetChild(int childId, wxAccessible **child) override
    {
        if (childId == wxACC_SELF) {
            *child = this;
            return wxACC_OK;
        }
        if (!valid(childId))
            return wxACC_FAIL;
        *child = nullptr; // a simple element, answered by this object
        return wxACC_OK;
    }

    wxAccStatus GetRole(int childId, wxAccRole *role) override
    {
        if (childId == wxACC_SELF)
            *role = wxROLE_SYSTEM_LIST;
        else if (!valid(childId))
            return wxACC_FAIL;
        else
            *role = (style(childId) & DD_ITEM_STYLE_SPLIT_ITEM) ? wxROLE_SYSTEM_SEPARATOR : wxROLE_SYSTEM_LISTITEM;
        return wxACC_OK;
    }

    wxAccStatus GetName(int childId, wxString *name) override
    {
        if (childId != wxACC_SELF) {
            if (!valid(childId))
                return wxACC_FAIL;
            *name = drop->rowText(childId - 1);
            return wxACC_OK;
        }
        // A group's list is named after its header, the main list after the combo that owns it
        if (!drop->group.IsEmpty()) {
            for (const auto &item : drop->items)
                if (item.group_key == drop->group) {
                    *name = item.group_label;
                    return wxACC_OK;
                }
        }
        wxWindow     *owner = drop->GetParent();
        wxAccessible *acc   = owner ? owner->GetOrCreateAccessible() : nullptr;
        if (!acc || acc->GetName(wxACC_SELF, name) != wxACC_OK)
            name->clear(); // an unnamed list beats the popup window's own name, "panel"
        return wxACC_OK;
    }

    wxAccStatus GetState(int childId, long *state) override
    {
        const bool has_focus = wxWindow::FindFocus() == drop;
        const int  current   = drop->focusedRow();
        const int  focused   = has_focus ? current : -1;
        if (childId == wxACC_SELF) {
            *state = wxACC_STATE_SYSTEM_FOCUSABLE | (has_focus && focused < 0 ? wxACC_STATE_SYSTEM_FOCUSED : 0);
            return wxACC_OK;
        }
        if (!valid(childId))
            return wxACC_FAIL;
        const int row   = childId - 1;
        const int index = drop->itemIndex(row);
        long      s     = selectable(childId) ? wxACC_STATE_SYSTEM_SELECTABLE | wxACC_STATE_SYSTEM_FOCUSABLE : wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (index < -1) { // a group header, whose presets open in a second list
            const DropDown *sub = drop->subDropDown;
            const bool open = sub && sub->IsShown() && sub->group == drop->items[-index - 2].group_key;
            s |= open ? wxACC_STATE_SYSTEM_EXPANDED : wxACC_STATE_SYSTEM_COLLAPSED;
        }
        // Like a native drop-down list, selection follows the highlight: the row Enter would pick
        if (row == current)
            s |= wxACC_STATE_SYSTEM_SELECTED;
        if (row == focused)
            s |= wxACC_STATE_SYSTEM_FOCUSED;
        if (!drop->GetClientRect().Intersects(rowRect(row)))
            s |= wxACC_STATE_SYSTEM_OFFSCREEN;
        *state = s;
        return wxACC_OK;
    }

    wxAccStatus GetLocation(wxRect &rect, int elementId) override
    {
        if (elementId == wxACC_SELF)
            return wxACC_NOT_IMPLEMENTED;
        if (!valid(elementId))
            return wxACC_FAIL;
        rect = rowRect(elementId - 1);
        rect.SetPosition(drop->ClientToScreen(rect.GetPosition()));
        return wxACC_OK;
    }

    wxAccStatus HitTest(const wxPoint &pt, int *childId, wxAccessible **childObject) override
    {
        const wxPoint p = drop->ScreenToClient(pt);
        if (!drop->GetClientRect().Contains(p) || drop->rowSize.y <= 0)
            return wxACC_NOT_IMPLEMENTED;
        const int row = (p.y - drop->offset.y) / drop->rowSize.y;
        *childId      = valid(row + 1) ? row + 1 : wxACC_SELF;
        *childObject  = nullptr;
        return wxACC_OK;
    }

    wxAccStatus GetFocus(int *childId, wxAccessible **child) override
    {
        if (wxWindow::FindFocus() != drop)
            return wxACC_NOT_IMPLEMENTED;
        const int row = drop->focusedRow();
        *childId      = valid(row + 1) ? row + 1 : wxACC_SELF;
        *child        = nullptr;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int childId, wxString *action) override
    {
        if (!selectable(childId))
            return wxACC_NOT_IMPLEMENTED;
        *action = drop->itemIndex(childId - 1) < -1 ? _L("Open") : _L("Select");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int childId) override
    {
        if (!selectable(childId))
            return wxACC_NOT_IMPLEMENTED;
        // Act the way a click on the row does, once the screen reader's call has returned
        drop->CallAfter([drop = drop, row = childId - 1] {
            if (!drop->IsShown())
                return;
            drop->setHoverItem(row, false);
            if (drop->hoverIndex() < -1)
                drop->openHoverGroup();
            else
                drop->selectHoverItem();
        });
        return wxACC_OK;
    }

private:
    bool valid(int childId) const { return childId >= 1 && childId <= int(drop->count); }

    // Style of the item behind a row; group headers carry none of their own
    int style(int childId) const
    {
        const int index = drop->itemIndex(childId - 1);
        return index >= 0 ? drop->items[index].style : 0;
    }

    bool selectable(int childId) const
    {
        return valid(childId) && drop->itemIndex(childId - 1) != -1 &&
               (style(childId) & (DD_ITEM_STYLE_SPLIT_ITEM | DD_ITEM_STYLE_DISABLED)) == 0;
    }

    wxRect rowRect(int row) const { return {0, drop->offset.y + row * drop->rowSize.y, drop->rowSize.x, drop->rowSize.y}; }

    DropDown *drop;
};

wxAccessible *DropDown::CreateAccessible() { return new DropDownAccessible(this); }

#endif // wxUSE_ACCESSIBILITY

void DropDown::Dismiss()
{
    if (subDropDown && subDropDown->IsShown())
        return;
    PopupWindow::Dismiss();
}

bool DropDown::ShouldDismissOnTopWindowDeactivate()
{
    // On Wayland, mapping a chained xdg_popup with grab makes the parent
    // toplevel inactive, which would otherwise cascade-dismiss the whole
    // chain. Skip when our chain peer is shown.
    return !((mainDropDown && mainDropDown->IsShown()) ||
             (subDropDown  && subDropDown->IsShown()));
}

void DropDown::OnDismiss()
{
    if (mainDropDown) {

        const wxPoint& mouse_pos = wxGetMousePosition();
        if (!mainDropDown->GetScreenRect().Contains(mouse_pos))
            mainDropDown->DismissAndNotify();
        else
#ifdef __WIN32__
            SetActiveWindow(mainDropDown->GetHandle());
#else
            ;
#endif
        return;
    }
    if (subDropDown && subDropDown->IsShown())
        return;
    dismissTime = boost::posix_time::microsec_clock::universal_time();
    hover_item  = -1;
    wxCommandEvent e(EVT_DISMISS);
    GetEventHandler()->ProcessEvent(e);
}
