#include "NotificationAccessible.hpp"

#if wxUSE_ACCESSIBILITY

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "Plater.hpp"

#include <wx/thread.h>
#include <wx/window.h>

#include <algorithm>

namespace Slic3r {
namespace GUI {

namespace {
// How many announcements stay readable at once. An alert is read from the accessible object
// long after the notification was pushed, so the text has to survive the ones which follow it;
// eight is more notifications than a slice produces in a screen reader's reading time.
constexpr int ALERT_SLOTS = 8;
// Child ids of the host window. The progress bar is always there, the alert slots appear as
// they are used, so both keep the same id for the life of the window.
constexpr int PROGRESS_CHILD_ID     = 1;
constexpr int FIRST_ALERT_CHILD_ID  = 2;
} // namespace

// The window the alerts and the progress bar belong to. Zero-sized and outside every sizer, so
// it lays nothing out and paints nothing, and it refuses the focus so that it does not turn
// into a tab stop between the plater's real controls.
class NotificationAccessibleHost : public wxWindow
{
public:
    explicit NotificationAccessibleHost(wxWindow* parent);

    bool AcceptsFocus() const override { return false; }
    bool AcceptsFocusFromKeyboard() const override { return false; }

    void announce(const wxString& text);
    void set_progress(int percent);

    // Number of alert slots which have been written to, which is how many of them exist as
    // far as a screen reader is concerned.
    int             alert_count() const { return m_alert_count; }
    const wxString& alert_text(int slot) const { return m_alerts[slot]; }
    // 0 to 100 while a slice is running, negative when none is.
    int             progress() const { return m_progress; }

private:
    wxString m_alerts[ALERT_SLOTS];
    int      m_alert_count{0};
    int      m_next_slot{0};
    int      m_progress{-1};
};

// Everything about the host window is a simple child of it: an alert or the progress bar has no
// window of its own, so the host answers every query about them by child id.
class NotificationAccessible : public wxAccessible
{
public:
    explicit NotificationAccessible(NotificationAccessibleHost* host) : wxAccessible(host), m_host(*host) {}

    wxAccStatus GetChildCount(int* childCount) override
    {
        *childCount = 1 + m_host.alert_count();
        return wxACC_OK;
    }

    wxAccStatus GetChild(int childId, wxAccessible** child) override
    {
        if (!is_valid(childId))
            return wxACC_FAIL;

        // a simple element: it has no accessible object of its own
        *child = nullptr;
        return wxACC_OK;
    }

    wxAccStatus GetName(int childId, wxString* name) override
    {
        if (childId == wxACC_SELF) {
            *name = _L("Notifications");
            return wxACC_OK;
        }
        if (!is_valid(childId))
            return wxACC_FAIL;

        // The alert's name is the whole of what the notification says: NVDA reads the name of
        // the object an alert event names, and reads nothing else about it.
        *name = childId == PROGRESS_CHILD_ID ? _L("Slicing progress") : m_host.alert_text(childId - FIRST_ALERT_CHILD_ID);
        return wxACC_OK;
    }

    wxAccStatus GetRole(int childId, wxAccRole* role) override
    {
        if (childId == wxACC_SELF)
            *role = wxROLE_SYSTEM_GROUPING;
        else if (childId == PROGRESS_CHILD_ID)
            *role = wxROLE_SYSTEM_PROGRESSBAR;
        else if (is_valid(childId))
            // NVDA ignores an alert event whose object is not an alert, so this role is what
            // makes the announcement happen at all.
            *role = wxROLE_SYSTEM_ALERT;
        else
            return wxACC_FAIL;

        return wxACC_OK;
    }

    wxAccStatus GetState(int childId, long* state) override
    {
        if (childId != wxACC_SELF && !is_valid(childId))
            return wxACC_FAIL;

        // Nothing here is operable, and nothing here takes the focus. The visibility is that of
        // the plater, which is where the notifications are drawn: a screen reader drops progress
        // bar updates from an object reported invisible or offscreen, which is what should
        // happen while the user is on another tab and no notification is on screen.
        long            s      = wxACC_STATE_SYSTEM_READONLY;
        const wxWindow* window = GetWindow();
        if (window == nullptr || !window->IsShownOnScreen())
            s |= wxACC_STATE_SYSTEM_INVISIBLE | wxACC_STATE_SYSTEM_OFFSCREEN;

        *state = s;
        return wxACC_OK;
    }

    wxAccStatus GetValue(int childId, wxString* strValue) override
    {
        if (childId != PROGRESS_CHILD_ID)
            // an alert is what it says, and has no value on top of that
            return wxACC_NOT_IMPLEMENTED;

        const int percent = m_host.progress();
        *strValue         = wxString::Format("%d", percent < 0 ? 0 : percent);
        return wxACC_OK;
    }

    wxAccStatus GetLocation(wxRect& rect, int elementId) override
    {
        if (elementId != wxACC_SELF && !is_valid(elementId))
            return wxACC_FAIL;

        wxWindow* window = GetWindow();
        if (window == nullptr)
            return wxACC_FAIL;

        // The notifications are drawn by ImGui and their rectangles exist only for the frame
        // which drew them, so there is no honest rectangle to report. The host's own is empty.
        rect = window->GetScreenRect();
        return wxACC_OK;
    }

private:
    bool is_valid(int childId) const { return childId >= PROGRESS_CHILD_ID && childId < FIRST_ALERT_CHILD_ID + m_host.alert_count(); }

    NotificationAccessibleHost& m_host;
};

NotificationAccessibleHost::NotificationAccessibleHost(wxWindow* parent) : wxWindow(parent, wxID_ANY, wxPoint(0, 0), wxSize(0, 0))
{
    SetAccessible(new NotificationAccessible(this));
}

void NotificationAccessibleHost::announce(const wxString& text)
{
    if (text.empty())
        return;

    const int child_id    = FIRST_ALERT_CHILD_ID + m_next_slot;
    m_alerts[m_next_slot] = text;
    m_next_slot           = (m_next_slot + 1) % ALERT_SLOTS;
    if (m_alert_count < ALERT_SLOTS)
        ++m_alert_count;

    // wxAccessible::NotifyEvent posts the event through wxTheApp->CallAfter, so the screen
    // reader comes back for the text after this call has returned - see the slots above.
    wxAccessible::NotifyEvent(wxACC_EVENT_SYSTEM_ALERT, this, wxOBJID_CLIENT, child_id);
}

void NotificationAccessibleHost::set_progress(int percent)
{
    percent = percent < 0 ? -1 : std::min(percent, 100);
    if (percent == m_progress)
        return;

    m_progress = percent;
    if (percent < 0)
        // No slice is running. The bar goes back to nothing silently: a screen reader reports a
        // value change, and there is no longer a value to report.
        return;

    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_VALUECHANGE, this, wxOBJID_CLIENT, PROGRESS_CHILD_ID);
}

NotificationAccessibleHost* NotificationAccessibility::ensure_host()
{
    if (m_host != nullptr)
        return m_host;

    // Windows are created on the GUI thread only. Every NotificationManager entry point which
    // reaches here runs on it, and an announcement is not worth a crash if one ever does not.
    if (!wxIsMainThread())
        return nullptr;

    wxWindow* parent = wxGetApp().plater();
    if (parent == nullptr)
        return nullptr;

    m_host = new NotificationAccessibleHost(parent);
    return m_host;
}

NotificationAccessibility::~NotificationAccessibility()
{
    // The plater owns this object through Plater::priv and the host window through its child
    // list, and destroys priv from its own destructor body, which runs before wxWindow destroys
    // the children. So the host is still alive here and is ours to take down. Doing it now takes
    // the accessible object with it, which is what stops a screen reader still holding a pointer
    // to notification state that has gone.
    if (m_host != nullptr) {
        m_host->Destroy();
        m_host = nullptr;
    }
}

void NotificationAccessibility::announce(const wxString& text)
{
    if (NotificationAccessibleHost* host = ensure_host())
        host->announce(text);
}

void NotificationAccessibility::set_progress(int percent)
{
    // Do not bring the host into existence just to say that nothing is happening.
    if (m_host == nullptr && percent < 0)
        return;
    if (NotificationAccessibleHost* host = ensure_host())
        host->set_progress(percent);
}

} // namespace GUI
} // namespace Slic3r

#else // wxUSE_ACCESSIBILITY

namespace Slic3r {
namespace GUI {

// wx has no accessibility backend outside MSW, so there is nothing to announce to.
NotificationAccessibility::~NotificationAccessibility() = default;
void NotificationAccessibility::announce(const wxString& /* text */) {}
void NotificationAccessibility::set_progress(int /* percent */) {}

} // namespace GUI
} // namespace Slic3r

#endif // wxUSE_ACCESSIBILITY
