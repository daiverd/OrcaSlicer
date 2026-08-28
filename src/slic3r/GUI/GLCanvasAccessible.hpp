#ifndef slic3r_GLCanvasAccessible_hpp_
#define slic3r_GLCanvasAccessible_hpp_

#include <wx/access.h>

#if wxUSE_ACCESSIBILITY

namespace Slic3r {
namespace GUI {

class GLCanvas3D;

// Screen reader view of the controls drawn on the GL canvas. They are sprites with no window
// of their own, so they cannot be accessible objects: they are exposed as MSAA simple children
// of the canvas instead, which means the canvas answers every query about them by child id.
// The data comes from GLCanvasKeyboardRing, which already knows which entries exist and which
// one has the virtual focus, and is read live on every query so that a toolbar changing under
// a screen reader is reported as it is rather than as it was.
//
// The toolbar entries are buttons which take the focus and can be pressed from here. The
// Preview sliders are children too, but they are announced only: they report a value and
// nothing else, because the keys which drive them belong to the canvas as a whole.
//
// Windows only, as wxAccessible itself is: wx 3.3 has no accessibility backend on the other
// platforms, where the keyboard ring works but announces nothing.
class GLCanvasAccessible : public wxAccessible
{
public:
    GLCanvasAccessible(wxWindow* window, GLCanvas3D& canvas) : wxAccessible(window), m_canvas(canvas) {}

    wxAccStatus GetChildCount(int* childCount) override;
    wxAccStatus GetChild(int childId, wxAccessible** child) override;
    wxAccStatus GetName(int childId, wxString* name) override;
    wxAccStatus GetDescription(int childId, wxString* description) override;
    wxAccStatus GetRole(int childId, wxAccRole* role) override;
    wxAccStatus GetState(int childId, long* state) override;
    wxAccStatus GetValue(int childId, wxString* strValue) override;
    wxAccStatus GetLocation(wxRect& rect, int elementId) override;
    wxAccStatus GetKeyboardShortcut(int childId, wxString* shortcut) override;
    wxAccStatus GetDefaultAction(int childId, wxString* actionName) override;
    wxAccStatus DoDefaultAction(int childId) override;
    wxAccStatus GetFocus(int* childId, wxAccessible** child) override;
    wxAccStatus HitTest(const wxPoint& pt, int* childId, wxAccessible** childObject) override;

private:
    GLCanvas3D& m_canvas;
};

} // namespace GUI
} // namespace Slic3r

#endif // wxUSE_ACCESSIBILITY

#endif // slic3r_GLCanvasAccessible_hpp_
