#ifndef slic3r_GUI_NotificationAccessible_hpp_
#define slic3r_GUI_NotificationAccessible_hpp_

#include <wx/access.h>
#include <wx/string.h>

namespace Slic3r {
namespace GUI {

#if wxUSE_ACCESSIBILITY
// Defined in NotificationAccessible.cpp: a zero-sized child window of the plater which owns
// the accessible object below and is the window MSAA events are sent for.
class NotificationAccessibleHost;
#endif

// Screen reader view of the notification stack. NotificationManager draws it with ImGui into
// the GL canvas, so the notifications are sprites: they have no window, and ImGui publishes
// nothing to any accessibility API, which leaves a screen reader with no way of knowing that
// slicing finished, failed or warned.
//
// Two things are exposed here, and deliberately nothing else:
//
//  - the text of a notification the user must not miss - an error, a serious warning, the end
//    of a slice - is announced once as an MSAA alert. NVDA speaks an alert when the object the
//    event names has the alert role and a name, so each announcement is parked in one of a few
//    slots and the event points at the slot it went into. The slots are what keeps a burst of
//    notifications readable: wxAccessible::NotifyEvent defers the event to the next idle and
//    the screen reader reads the text later still, by which time a single "latest text" field
//    would already have been overwritten.
//
//  - slicing progress is a progress bar with a live value, and is never announced. A screen
//    reader user has their own setting for whether and how progress bars are reported; speaking
//    every percentage at them is the thing that setting exists to prevent.
//
// Both live on a zero-sized child window of the plater, created on first use, because an
// accessible object and an MSAA event each need a window to belong to and the notifications
// have none of their own. It draws nothing and refuses the focus, so no pixel of the
// notification stack moves and the tab order does not change.
//
// Windows only, as wxAccessible itself is: wx 3.3 ships an accessibility backend for MSW alone,
// so every method here is a no-op on macOS and Linux.
class NotificationAccessibility
{
public:
    NotificationAccessibility() = default;
    ~NotificationAccessibility();

    NotificationAccessibility(const NotificationAccessibility&)            = delete;
    NotificationAccessibility& operator=(const NotificationAccessibility&) = delete;

    // Speaks text through the screen reader straight away. For the notifications a user must
    // not miss; everything else stays silent.
    void announce(const wxString& text);
    // Percentage of the running slice, 0 to 100, or a negative value when no slice is running.
    void set_progress(int percent);

private:
#if wxUSE_ACCESSIBILITY
    // Created lazily and destroyed by this object, see the destructor for why it is not left
    // to the parent window.
    NotificationAccessibleHost* m_host{nullptr};

    NotificationAccessibleHost* ensure_host();
#endif // wxUSE_ACCESSIBILITY
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_NotificationAccessible_hpp_
