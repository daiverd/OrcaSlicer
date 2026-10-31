#include "TabCtrl.hpp"

#include "slic3r/GUI/Widgets/StaticBox.hpp"
#include <wx/anybutton.h>
#include <wx/checklst.h>
#include <cstddef>
#include <utility>
#include <wx/colour.h>
#include <algorithm>
#include <wx/dc.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/StateHandler.hpp"

#include <algorithm>

#if wxUSE_ACCESSIBILITY
namespace {
// ORCA: the strip is the selection container for its tab buttons, so it is the one asked
// which of them is current (IAccessible::get_accSelection, which the UIA proxy turns into
// the Selection pattern's selected item). The buttons cannot take the focus, so this is
// the only place that answer can come from.
class TabCtrlAccessible : public WidgetAccessible
{
public:
    explicit TabCtrlAccessible(TabCtrl* ctrl) : WidgetAccessible(ctrl, wxROLE_SYSTEM_PAGETABLIST) {}

#if wxUSE_VARIANT
    wxAccStatus GetSelections(wxVariant* selections) override
    {
        auto* ctrl = static_cast<TabCtrl*>(GetWindow());
        wxWindow* btn = ctrl ? ctrl->GetSelectedItemButton() : nullptr;
        wxAccessible* btn_acc = btn ? btn->GetOrCreateAccessible() : nullptr;
        if (!btn_acc)
            return wxACC_NOT_IMPLEMENTED;

        // wx passes a single selected child on as a "void*" variant holding its
        // wxAccessible (see wxIAccessible::get_accSelection).
        *selections = wxVariant(static_cast<void*>(btn_acc));
        return wxACC_OK;
    }
#endif // wxUSE_VARIANT

    // A native tab control puts the focus on its current tab, and that is what a screen
    // reader announces; the strip holds the real focus here, so report the tab instead.
    wxAccStatus GetFocus(int* childId, wxAccessible** child) override
    {
        auto* ctrl = static_cast<TabCtrl*>(GetWindow());
        wxWindow* btn = ctrl && wxWindow::FindFocus() == ctrl ? ctrl->GetSelectedItemButton() : nullptr;
        wxAccessible* btn_acc = btn ? btn->GetOrCreateAccessible() : nullptr;
        if (!btn_acc)
            return wxACC_NOT_IMPLEMENTED;
        *childId = wxACC_SELF;
        *child   = btn_acc;
        return wxACC_OK;
    }
};

// Announce the current tab as focused, while the strip itself holds the keyboard focus
void notify_focused_tab(TabCtrl* ctrl)
{
    if (wxWindow::FindFocus() != ctrl)
        return;
    if (wxWindow* btn = ctrl->GetSelectedItemButton())
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, btn, wxOBJID_CLIENT, wxACC_SELF);
}
} // namespace
#endif // wxUSE_ACCESSIBILITY

wxDEFINE_EVENT(wxEVT_TAB_SEL_CHANGING, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_TAB_SEL_CHANGED, wxCommandEvent);

BEGIN_EVENT_TABLE(TabCtrl, StaticBox)

EVT_KEY_DOWN(TabCtrl::keyDown)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

#define TAB_BUTTON_SPACE 2
#define TAB_BUTTON_PADDING_X 3
#define TAB_BUTTON_PADDING_Y 2
#define TAB_BUTTON_PADDING TAB_BUTTON_PADDING_X, TAB_BUTTON_PADDING_Y

TabCtrl::TabCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : StaticBox(parent, id, pos, size, style)
{
#if 0
    radius = 5;
#else
    radius = 1;
#endif
    border_width = 1;
    SetBorderColor(0xcecece);
    sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(10);
    auto hsizer = new wxBoxSizer(wxVERTICAL);
    hsizer->Add(sizer, 0, wxEXPAND | wxBOTTOM, border_width * 4);
    SetSizer(hsizer);
    Bind(wxEVT_COMMAND_BUTTON_CLICKED, &TabCtrl::buttonClicked, this);
    // ORCA: the tab list role, and which tab is current, come from TabCtrlAccessible
    // (see CreateAccessible), so no SetAccessibleRole call here
#if wxUSE_ACCESSIBILITY
    // After the strip's own focus event, so the current tab is announced when it is reached
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& e) {
        e.Skip();
        CallAfter([this] { notify_focused_tab(this); });
    });
#endif
    // wxString reason;
    // IsTransparentBackgroundSupported(&reason);
}

TabCtrl::~TabCtrl() { delete images; }

int TabCtrl::GetSelection() const { return sel; }

wxWindow* TabCtrl::GetSelectedItemButton() const
{
    return sel >= 0 && sel < int(btns.size()) ? btns[sel] : nullptr;
}

#if wxUSE_ACCESSIBILITY
wxAccessible* TabCtrl::CreateAccessible()
{
    return new TabCtrlAccessible(this);
}
#endif

void TabCtrl::SelectItem(int item)
{
    if (item == sel || !sendTabCtrlEvent(true))
        return;
    if (sel >= 0) {
        wxCommandEvent e(wxEVT_CHECKBOX);
        auto b = btns[sel];
        e.SetEventObject(b);
        b->GetEventHandler()->ProcessEvent(e);
    }
    sel = item;
    if (sel >= 0) {
        wxCommandEvent e(wxEVT_CHECKBOX);
        auto b = btns[sel];
        e.SetEventObject(b);
        b->GetEventHandler()->ProcessEvent(e);
    }
    sendTabCtrlEvent();
    relayout();
    Refresh();
#if wxUSE_ACCESSIBILITY
    // ORCA: the tab buttons are not focusable, so without this a screen reader is never
    // told that an arrow key (or a click) moved to another page. The selection event alone
    // is ignored by NVDA for anything outside the focus, hence the focus event as well.
    if (wxWindow* btn = GetSelectedItemButton())
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_SELECTION, btn, wxOBJID_CLIENT, wxACC_SELF);
    notify_focused_tab(this);
#endif
}

void TabCtrl::Unselect() { SelectItem(-1); }

void TabCtrl::Rescale()
{
    for (auto& b : btns)
        b->Rescale();
    relayout();
}

bool TabCtrl::SetFont(wxFont const& font)
{
    StaticBox::SetFont(font);
    bold = font.Bold();
    for (size_t i = 0; i < btns.size(); ++i)
        btns[i]->SetFont(i == sel ? bold : font);
    return true;
}

int TabCtrl::AppendItem(const wxString& item, int image, int selImage, void* clientData)
{
    Button* btn = new Button();
    btn->Create(this, item, "", wxBORDER_NONE);
    // ORCA: TabCtrl itself is the single Tab stop and owns arrow-key
    // navigation (WM_GETDLGCODE below, keyDown()); buttonClicked() always
    // hands focus back to `this` after a mouse click. Now that StaticBox is
    // a real wxNavigationEnabled container, a focusable child here would let
    // Tab traversal (and real OS focus) land on individual tab buttons
    // instead, which stops TabCtrl's own key handler from ever seeing the
    // arrow keys. Keep tab buttons click-only, never a Tab stop.
    btn->SetCanFocus(false);
    btn->SetFont(GetFont());
    btn->SetTextColor(
        StateColor(std::make_pair(0x6B6B6C, (int) StateColor::NotChecked), std::make_pair(wxColour("#262E30"), (int) StateColor::Normal)));
    btn->SetBackgroundColor(StateColor());
    btn->SetCornerRadius(0);
    btn->SetPaddingSize({TAB_BUTTON_PADDING});
#if wxUSE_ACCESSIBILITY
    // ORCA: this Button is a tab, not a plain pushbutton - override Button's default role
    btn->SetAccessibleRole(wxROLE_SYSTEM_PAGETAB, [this, btn] {
        auto it = std::find(btns.begin(), btns.end(), btn);
        if (it == btns.end() || int(it - btns.begin()) != sel)
            return 0L;
        // The current tab stands in for the strip's focus (see TabCtrlAccessible::GetFocus)
        return wxACC_STATE_SYSTEM_SELECTED | (wxWindow::FindFocus() == this ? wxACC_STATE_SYSTEM_FOCUSED : 0L);
    });
#endif
    btns.push_back(btn);
    if (btns.size() > 1)
        sizer->GetItem(sizer->GetItemCount() - 1)->SetMinSize({0, 0});
    sizer->Add(btn, 0, wxALIGN_CENTER_VERTICAL);
    sizer->AddStretchSpacer(1);
    relayout();
    return btns.size() - 1;
}

int TabCtrl::AppendItem(const wxString& item, const wxBitmap& bitmap, void* clientData)
{
    const int index = AppendItem(item, -1, -1, clientData);
    SetItemBitmap(index, bitmap);
    return index;
}

bool TabCtrl::DeleteItem(int item)
{
    if (item < 0 || item >= btns.size()) {
        return false;
    }
    const bool selection_changed = sel >= item;

    if (selection_changed) {
        sendTabCtrlEvent(true);
    }

    Button* btn = btns[item];
    btn->Destroy();
    btns.erase(btns.begin() + item);
    sizer->Remove(item * 2);
    if (btns.size() > 1)
        sizer->GetItem(sizer->GetItemCount() - 1)->SetMinSize({0, 0});

    if (selection_changed) {
        sel--; // `relayout()` uses `sel` so we need to update this before calling `relayout()`
    }
    relayout();
    if (selection_changed) {
        sendTabCtrlEvent();
    }

    return true;
}

void TabCtrl::DeleteAllItems()
{
    sizer->Clear(true);
    sizer->AddSpacer(10);
    btns.clear();
    if (sel >= 0) {
        sel = -1;
        sendTabCtrlEvent();
    }
}

unsigned int TabCtrl::GetCount() const { return btns.size(); }

wxString TabCtrl::GetItemText(unsigned int item) const { return item < btns.size() ? btns[item]->GetLabel() : wxString{}; }

void TabCtrl::SetItemText(unsigned int item, wxString const& value)
{
    if (item >= btns.size())
        return;
    btns[item]->SetLabel(value);
}

void TabCtrl::SetItemBitmap(unsigned int item, const wxBitmap& bitmap)
{
    if (item >= btns.size())
        return;
    btns[item]->SetIcon(bitmap);
    relayout();
}

void TabCtrl::SetItemName(unsigned int item, wxString const& name)
{
    if (item >= btns.size())
        return;
    btns[item]->SetName(name);
}

void TabCtrl::SetItemIndicator(unsigned int item, bool on)
{
    if (item >= btns.size())
        return;
    btns[item]->SetIndicator(on);
    relayout();
}

bool TabCtrl::GetItemBold(unsigned int item) const
{
    if (item >= btns.size())
        return false;
    return btns[item]->GetFont() == bold;
}

void TabCtrl::SetItemBold(unsigned int item, bool bold)
{
    if (item >= btns.size())
        return;
    btns[item]->SetFont(bold ? this->bold : GetFont());
    btns[item]->Rescale();
}

void* TabCtrl::GetItemData(unsigned int item) const
{
    if (item >= btns.size())
        return nullptr;
    return btns[item]->GetClientData();
}

void TabCtrl::SetItemData(unsigned int item, void* clientData)
{
    if (item >= btns.size())
        return;
    btns[item]->SetClientData(clientData);
}

void TabCtrl::AssignImageList(wxImageList* imageList)
{
    if (images == imageList)
        return;
    delete images;
    images = imageList;
}

void TabCtrl::SetItemTextColour(unsigned int item, const StateColor& col)
{
    if (item >= btns.size())
        return;
    btns[item]->SetTextColor(col);
}

int TabCtrl::GetFirstVisibleItem() const { return btns.size() == 0 ? -1 : 0; }

int TabCtrl::GetNextVisible(int item) const { return ++item < btns.size() ? item : -1; }

bool TabCtrl::IsVisible(unsigned int item) const { return true; }

void TabCtrl::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    auto size = GetSize();
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
    if (size == GetSize())
        return;
    relayout();
}

#ifdef __WIN32__

WXLRESULT TabCtrl::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if (nMsg == WM_GETDLGCODE) {
        return DLGC_WANTARROWS;
    }
    return wxWindow::MSWWindowProc(nMsg, wParam, lParam);
}

#endif

void TabCtrl::relayout()
{
    int offset = 10;
    int item   = sel + 1;
    int first  = 0;
    for (int i = 0; i < item; ++i)
        offset += btns[i]->GetMinSize().x;
    if (item < btns.size())
        offset += btns[item]->GetMinSize().x;
    int width = GetSize().x;
    for (int i = 0; i < btns.size(); ++i) {
        auto size = btns[i]->GetMinSize().x;
        if (i < sel && offset > width) {
            sizer->Show(i * 2 + 1, false);
            sizer->Show(i * 2 + 2, false);
            offset -= size;
            first = i + 1;
        } else if (i <= item) {
            sizer->Show(i * 2 + 1, true);
            sizer->Show(i * 2 + 2, true);
        } else if (offset <= width) {
            sizer->Show(i * 2 + 1, true);
            sizer->Show(i * 2 + 2, true);
            offset += size;
            item = i;
        } else {
            sizer->Show(i * 2 + 1, false);
            sizer->Show(i * 2 + 2, false);
        }
        sizer->GetItem(i * 2 + 2)->SetMinSize({0, 0});
    }
    if (item >= btns.size())
        --item;
    // Keep spacing 2 ~ 10 TAB_BUTTON_SPACE
    int b = GetSize().x - offset - 10 - (item + 1 - first) * 16;
    sizer->GetItem(item * 2 + 2)->SetMinSize({b > 0 ? b : 0, 0});
    Layout();
}

int TabCtrl::GetFullSize() const
{
    // Mirrors relayout(): a 10px leading spacer plus every button's min width.
    int width = 10;
    for (const Button* btn : btns)
        width += btn->GetMinSize().x;
    return width;
}

void TabCtrl::buttonClicked(wxCommandEvent& event)
{
    SetFocus();
    auto btn  = event.GetEventObject();
    auto iter = std::find(btns.begin(), btns.end(), btn);
    SelectItem(iter == btns.end() ? -1 : iter - btns.begin());
}

void TabCtrl::keyDown(wxKeyEvent& event)
{
    switch (event.GetKeyCode()) {
    case WXK_UP:
    case WXK_DOWN:
    case WXK_LEFT:
    case WXK_RIGHT:
        if ((event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_LEFT) && GetSelection() > 0) {
            SelectItem(GetSelection() - 1);
        } else if ((event.GetKeyCode() == WXK_DOWN || event.GetKeyCode() == WXK_RIGHT) && GetSelection() + 1 < btns.size()) {
            SelectItem(GetSelection() + 1);
        }
        break;
    // ORCA: a tab strip is expected to jump to its first/last tab on Home/End. Unlike the
    // arrows, these need nothing from WM_GETDLGCODE: wxMSW only turns Tab, Return and the
    // arrows into dialog navigation, so Home/End arrive here as ordinary key events.
    case WXK_HOME:
    case WXK_NUMPAD_HOME:
    case WXK_END:
    case WXK_NUMPAD_END:
        if (!btns.empty()) {
            const bool home = event.GetKeyCode() == WXK_HOME || event.GetKeyCode() == WXK_NUMPAD_HOME;
            SelectItem(home ? 0 : int(btns.size()) - 1);
        }
        break;
    default:
        event.Skip();
        break;
    }
}

void TabCtrl::doRender(wxDC& dc)
{
    wxSize size = GetSize();
    int states  = state_handler.states();
    if (sel < 0) {
        return;
    }

    auto x1       = btns[sel]->GetPosition().x;
    auto x2       = x1 + btns[sel]->GetSize().x;
    const int BS2 = (1 + border_width) / 2;
#if 0
    const int BS = border_width / 2;
    x1 -= TAB_BUTTON_SPACE; x2 += TAB_BUTTON_SPACE;
    dc.SetPen(wxPen(border_color.colorForStates(states), border_width));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawLine(0, size.y - BS2, x1 - radius + BS2, size.y - BS2);
    dc.DrawArc(x1 - radius, size.y, x1, size.y - radius, x1 - radius, size.y - radius);
    dc.DrawLine(x1, size.y - radius, x1, radius);
    dc.DrawArc(x1 + radius, 0, x1, radius, x1 + radius, radius);
    dc.DrawLine(x1 + radius, BS, x2 - radius, BS);
    dc.DrawArc(x2, radius, x2 - radius, 0, x2 - radius, radius);
    dc.DrawLine(x2, radius, x2, size.y - radius);
    dc.DrawArc(x2, size.y - radius, x2 + radius, size.y, x2 + radius, size.y - radius);
    dc.DrawLine(x2 + radius - BS2, size.y - BS2, size.x, size.y - BS2);
#else
    dc.SetPen(wxPen(border_color.colorForStates(states), border_width));
    dc.DrawLine(0, size.y - BS2, size.x, size.y - BS2);
    wxColour c("#009688"); // ORCA selected tab underline stroke color
    dc.SetPen(wxPen(c, 1));
    dc.SetBrush(c);
    dc.DrawRoundedRectangle(x1 - radius, size.y - BS2 - border_width * 3, x2 + radius * 2 - x1, border_width * 3, radius);
#endif
}

bool TabCtrl::sendTabCtrlEvent(bool changing)
{
    wxCommandEvent event(changing ? wxEVT_TAB_SEL_CHANGING : wxEVT_TAB_SEL_CHANGED, GetId());
    event.SetEventObject(this);
    event.SetInt(sel);
    GetEventHandler()->ProcessEvent(event);
    return true;
}
