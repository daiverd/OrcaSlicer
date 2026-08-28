#ifndef slic3r_GUI_TransformDialog_hpp_
#define slic3r_GUI_TransformDialog_hpp_

#include <array>
#include <string>

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "GUI_Utils.hpp"
#include "libslic3r/Point.hpp"

namespace Slic3r { namespace GUI {

class GizmoObjectManipulation;

// Keyboard and screen reader accessible entry of exact object transforms.
//
// The ImGui Move/Rotate/Scale panels (GizmoObjectManipulation::do_render_*_window) are the
// only place in Orca where a numeric transform can be typed, and ImGui publishes nothing to
// any accessibility API. This dialog is a second view onto the same model: it reads
// GizmoObjectManipulation's cached values and writes every edit back through its existing
// on_change() entry point, so it shares the undo stack and the transformation rules with the
// panels instead of reimplementing them. No ImGui code is involved.
class TransformDialog : public DPIDialog
{
public:
    // True when there is a selection to transform. Used by the context menu item's enable
    // callback and by the keyboard shortcut.
    static bool can_open();
    // Opens the modal dialog on the current View3D selection.
    static void show_for_selection(wxWindow* parent = nullptr);

    TransformDialog(wxWindow* parent, GizmoObjectManipulation& manipulation);
    ~TransformDialog() override = default;

    // Called by wxDialog when OK is pressed (button, Enter or the mnemonic); Cancel never
    // reaches it, so cancelling simply discards the typed values.
    bool TransferDataFromWindow() override;

private:
    enum Group {
        gPosition = 0,
        gRotation,
        gRotateBy,
        gScale,
        gSize,
        gCount
    };

    wxSizer* create_group(Group group, const wxString& name, const wxString& unit, const Vec3d& values);
    void     apply_group(Group group, const std::string& opt_key);
    void     apply();

    GizmoObjectManipulation& m_manipulation;

    std::array<std::array<wxTextCtrl*, 3>, gCount> m_fields{};
    // Values shown when the dialog opened, in display units. Only fields the user actually
    // changed are applied on OK.
    std::array<Vec3d, gCount>                     m_initial{};

    wxCheckBox* m_uniform_scale{nullptr};
    bool        m_initial_uniform_scale{true};

    void on_dpi_changed(const wxRect& suggested_rect) override;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_TransformDialog_hpp_
