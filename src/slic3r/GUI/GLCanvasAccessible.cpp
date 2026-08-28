#include "GLCanvasAccessible.hpp"

#if wxUSE_ACCESSIBILITY

#include "GLCanvas3D.hpp"
#include "GLCanvasRing.hpp"
#include "I18N.hpp"

#include <wx/window.h>

namespace Slic3r {
namespace GUI {

wxAccStatus GLCanvasAccessible::GetChildCount(int* childCount)
{
    *childCount = m_canvas.get_keyboard_ring().child_count();
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetChild(int childId, wxAccessible** child)
{
    if (childId < 1 || childId > m_canvas.get_keyboard_ring().child_count())
        return wxACC_FAIL;

    // a simple element: it has no accessible object of its own, so every query about it
    // comes back here with its child id
    *child = nullptr;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetName(int childId, wxString* name)
{
    if (childId == wxACC_SELF)
        // the canvas keeps whatever the standard accessible reports for the window
        return wxACC_NOT_IMPLEMENTED;

    GLCanvasKeyboardRing::ChildInfo info;
    if (!m_canvas.get_keyboard_ring().child_info(childId, info))
        return wxACC_FAIL;

    *name = info.name;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetDescription(int childId, wxString* description)
{
    if (childId == wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    GLCanvasKeyboardRing::ChildInfo info;
    if (!m_canvas.get_keyboard_ring().child_info(childId, info))
        return wxACC_FAIL;

    // the toolbar an entry belongs to has no accessible object to carry its name, so the
    // description is where a user hears which one they have moved into
    *description = info.description;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetRole(int childId, wxAccRole* role)
{
    if (childId == wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    GLCanvasKeyboardRing::ChildInfo info;
    if (!m_canvas.get_keyboard_ring().child_info(childId, info))
        return wxACC_FAIL;

    // A toolbar button, including the toggles and the gizmos: a toggled entry is a button
    // reported as pressed, which is what a toolbar does, rather than a check box. Or one of
    // the Preview sliders.
    *role = info.role;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetState(int childId, long* state)
{
    if (childId == wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    const GLCanvasKeyboardRing& ring = m_canvas.get_keyboard_ring();
    GLCanvasKeyboardRing::ChildInfo info;
    if (!ring.child_info(childId, info))
        return wxACC_FAIL;

    // A screen reader which is told an element cannot take the focus discards the focus
    // events sent for it, so every entry is focusable. The sliders are not: they are read
    // where they are and never focused, which is what read only says.
    long s = info.focusable ? wxACC_STATE_SYSTEM_FOCUSABLE : wxACC_STATE_SYSTEM_READONLY;
    const wxWindow* window = GetWindow();
    if (window == nullptr || !window->IsShownOnScreen())
        s |= wxACC_STATE_SYSTEM_INVISIBLE | wxACC_STATE_SYSTEM_OFFSCREEN;
    else if (info.focusable && window->HasFocus() && ring.focused_child_id() == childId)
        s |= wxACC_STATE_SYSTEM_FOCUSED;
    if (!info.enabled)
        s |= wxACC_STATE_SYSTEM_UNAVAILABLE;
    if (info.pressed)
        s |= wxACC_STATE_SYSTEM_PRESSED;

    *state = s;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetValue(int childId, wxString* strValue)
{
    if (childId == wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    GLCanvasKeyboardRing::ChildInfo info;
    if (!m_canvas.get_keyboard_ring().child_info(childId, info))
        return wxACC_FAIL;

    if (info.value.empty())
        // a toolbar button is what it is named, with no value on top of that
        return wxACC_NOT_IMPLEMENTED;

    *strValue = info.value;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetLocation(wxRect& rect, int elementId)
{
    if (elementId == wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    wxWindow* window = GetWindow();
    if (window == nullptr)
        return wxACC_FAIL;

    // there is nothing to report before a render has laid the toolbar out
    wxRect client_rect;
    if (!m_canvas.get_keyboard_ring().child_rect(elementId, client_rect))
        return wxACC_NOT_IMPLEMENTED;

    rect = wxRect(window->ClientToScreen(client_rect.GetPosition()), client_rect.GetSize());
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetKeyboardShortcut(int childId, wxString* shortcut)
{
    if (childId == wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    GLCanvasKeyboardRing::ChildInfo info;
    if (!m_canvas.get_keyboard_ring().child_info(childId, info))
        return wxACC_FAIL;

    // kept out of the name, which would otherwise have it read back on every move
    *shortcut = info.shortcut;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::GetDefaultAction(int childId, wxString* actionName)
{
    if (childId == wxACC_SELF)
        return wxACC_NOT_IMPLEMENTED;

    GLCanvasKeyboardRing::ChildInfo info;
    if (!m_canvas.get_keyboard_ring().child_info(childId, info))
        return wxACC_FAIL;

    if (info.role != wxROLE_SYSTEM_PUSHBUTTON)
        // the sliders are read from here, not driven
        return wxACC_NOT_IMPLEMENTED;

    *actionName = _L("Press");
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::DoDefaultAction(int childId)
{
    if (childId == wxACC_SELF)
        return wxACC_NOT_SUPPORTED;

    // the in-process action of the entry, the same one the keyboard and the mouse run
    return m_canvas.get_keyboard_ring().activate_child(childId) ? wxACC_OK : wxACC_FAIL;
}

wxAccStatus GLCanvasAccessible::GetFocus(int* childId, wxAccessible** child)
{
    const int focused = m_canvas.get_keyboard_ring().focused_child_id();
    const wxWindow* window = GetWindow();
    if (window == nullptr || !window->HasFocus()) {
        // nothing in here has the focus
        *childId = wxACC_SELF;
        *child = nullptr;
        return wxACC_OK;
    }

    *childId = focused;
    // the scene, rather than a toolbar entry, is where the focus is
    *child = focused == 0 ? this : nullptr;
    return wxACC_OK;
}

wxAccStatus GLCanvasAccessible::HitTest(const wxPoint& pt, int* childId, wxAccessible** childObject)
{
    wxWindow* window = GetWindow();
    if (window == nullptr)
        return wxACC_FAIL;

    const GLCanvasKeyboardRing& ring = m_canvas.get_keyboard_ring();
    const wxPoint client_pt = window->ScreenToClient(pt);
    const int count = ring.child_count();
    for (int id = 1; id <= count; ++id) {
        wxRect rect;
        if (ring.child_rect(id, rect) && rect.Contains(client_pt)) {
            *childId = id;
            *childObject = nullptr;
            return wxACC_OK;
        }
    }

    // the scene, or a control the ring does not know about: the standard accessible for the
    // window is a better answer than the canvas claiming the point for itself
    return wxACC_NOT_IMPLEMENTED;
}

} // namespace GUI
} // namespace Slic3r

#endif // wxUSE_ACCESSIBILITY
