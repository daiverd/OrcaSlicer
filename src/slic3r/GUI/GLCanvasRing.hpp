#ifndef slic3r_GLCanvasRing_hpp_
#define slic3r_GLCanvasRing_hpp_

#include <string>
#include <vector>

#include <wx/access.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

#include "Gizmos/GLGizmosManager.hpp"

class wxKeyEvent;

namespace Slic3r {
namespace GUI {

class GLCanvas3D;
class GLToolbar;

// Keyboard access to the controls drawn on the GL canvas. They are sprites, not
// windows, so wx focus traversal cannot see any of them. Each toolbar object is a
// focus stop of its own: Tab and Shift+Tab move between the stops and in and out of
// the canvas, the arrow keys traverse the entries of the focused stop and Enter or
// Space activates one. The scene itself is the first stop of every canvas and claims
// no key there, so every existing canvas binding keeps working as it does today.
class GLCanvasKeyboardRing
{
public:
    explicit GLCanvasKeyboardRing(GLCanvas3D& canvas) : m_canvas(canvas) {}

    // true while a toolbar stop, rather than the scene, holds the virtual focus
    bool has_focus() const { return m_focused_stop != Scene_Stop; }

    // Move the virtual focus one stop forward or backward. Returns false when there
    // is no stop left to move to and the focus must leave the canvas.
    bool focus_next_stop();
    bool focus_previous_stop();
    // back to the scene stop, with every stop back on its first entry
    void reset();

    // returns true if the event was consumed
    bool on_key(wxKeyEvent& evt);

    void render_focus_ring(const GLToolbar& toolbar) const;
    void render_focus_ring(const GLGizmosManager& gizmos) const;

#if wxUSE_ACCESSIBILITY
    // What GLCanvasAccessible exposes as the MSAA simple children of the canvas. The ids
    // are flat, 1..N over the entries of every stop in stop order, and they are resolved
    // against live data on every call, so an id read before a toolbar changed can be stale.
    struct ChildInfo
    {
        wxString name;        // the label of the entry alone, localized
        wxString description; // the name of the stop it belongs to, plus what the tooltip adds
        wxString shortcut;
        bool     enabled{ true };
        bool     pressed{ false };
    };

    int  child_count() const;
    // the entry the virtual focus is on, 0 while the focus is on the scene stop
    int  focused_child_id() const;
    bool child_info(int child_id, ChildInfo& info) const;
    // in canvas client pixels; false while the entry has no geometry to report
    bool child_rect(int child_id, wxRect& rect) const;
    bool activate_child(int child_id);
#endif // wxUSE_ACCESSIBILITY

private:
    // the toolbar stops, in the order Tab visits them; a stop with no entry is skipped
    enum EStop
    {
        MainToolbar,
        GizmoBar,
        AssembleViewToolbar,
        CollapseToolbar,
        Num_Stops
    };

    static constexpr int Scene_Stop = -1;

    // Identifies one entry by what it is rather than by where it is, so that a toolbar
    // changing shape under the focus does not move it onto another entry.
    struct Entry
    {
        // null for a gizmo entry
        GLToolbar*             toolbar{ nullptr };
        std::string            item_name;
        GLGizmosManager::EType gizmo{ GLGizmosManager::Undefined };

        bool operator == (const Entry& other) const {
            return toolbar == other.toolbar && item_name == other.item_name && gizmo == other.gizmo;
        }
    };

    // The name is what the canvas accessible object will announce when the focus
    // enters the stop; nothing reads it yet.
    struct Stop
    {
        wxString           name;
        std::vector<Entry> entries;
    };

    // Where a stop is left when the focus moves off it, kept for as long as the canvas
    // keeps the wx focus so that Shift+Tab undoes the Tab which preceded it.
    struct Parked
    {
        Entry  entry;
        size_t index{ 0 };
    };

    // built from live data on every access, never cached
    Stop stop_of(int stop) const;
    GLToolbar* toolbar_of(int stop) const;
    size_t parked_index(int stop, const std::vector<Entry>& entries) const;
    bool focused_entry(Entry& entry) const;
    void activate(const Entry& entry);

    // Announce the virtual focus, and what the focused entry has become when it was just
    // activated. Both are Windows only: wx has an accessibility backend there and nowhere
    // else, and the ring itself works on all three platforms without one.
    void notify_focus();
    void notify_activated();

#if wxUSE_ACCESSIBILITY
    bool child_at(int child_id, Stop& stop, size_t& index) const;
#endif // wxUSE_ACCESSIBILITY

    GLCanvas3D& m_canvas;
    int         m_focused_stop{ Scene_Stop };
    Parked      m_parked[Num_Stops];
#if wxUSE_ACCESSIBILITY
    // the child count the last event went out with, to notice that the ids have moved
    int         m_announced_child_count{ 0 };
#endif // wxUSE_ACCESSIBILITY
};

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GLCanvasRing_hpp_
