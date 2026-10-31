#include "TransformDialog.hpp"

#include <cmath>

#include <wx/access.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Field.hpp"
#include "GLCanvas3D.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "NotificationManager.hpp"
#include "Plater.hpp"
#include "Selection.hpp"
#include "Gizmos/GizmoObjectManipulation.hpp"

namespace Slic3r { namespace GUI {

static const char* g_axis_names[3] = {"X", "Y", "Z"};

namespace {

#if wxUSE_ACCESSIBILITY

// Publishes wxWindow::SetName() as the MSAA accName of a *native* control (the real Win32
// EDIT behind a wxTextCtrl).
//
// SetName() on its own never reaches MSAA: it is only read by wxWindowAccessible, and
// wxWindowBase::CreateAccessible() returns nullptr for a plain wxTextCtrl, so nothing is
// attached, wx leaves WM_GETOBJECT to DefWindowProc, and the OS proxy names the control
// after the static text in front of it. Here that is just "X:", identical in all five
// groups, which is exactly what a screen reader read back before this.
//
// Every method other than GetName is deliberately left unimplemented, which makes
// wxIAccessible fall back to the control's own standard accessible object (see
// wxAccessible::GetIAccessibleStd), so role, value, caret, selection, the read-only state
// and everything else keep coming from the native control exactly as before.
//
// The `integration` branch carries an equivalent class as NativeCtrlAccessible in
// Widgets/WidgetAccessible.hpp (branch a11y-axe-nrvs). This file-local copy keeps this
// branch self-contained on main; its shape is kept identical so that consolidating the
// two into the shared header is a mechanical change once the branches are sequenced.
class NativeCtrlAccessible : public wxAccessible
{
public:
    explicit NativeCtrlAccessible(wxWindow* win) : wxAccessible(win) {}

    wxAccStatus GetName(int childId, wxString* name) override
    {
        if (childId != wxACC_SELF)
            return wxACC_NOT_IMPLEMENTED;

        wxWindow* win = GetWindow();
        if (win == nullptr)
            return wxACC_FAIL;

        const wxString title = win->GetName();
        if (title.empty())
            // Nothing has named this control - leave the native accessible to its own
            // guess rather than overriding it with nothing.
            return wxACC_NOT_IMPLEMENTED;

        *name = title;
        return wxACC_OK;
    }
};

#endif // wxUSE_ACCESSIBILITY

// Names a native control for both wx and the accessibility API. wxWindowBase::SetAccessible()
// takes ownership and deletes the object from ~wxWindowBase, so every control gets its own
// instance and the dialog has nothing to free.
void set_accessible_name(wxWindow* win, const wxString& name)
{
    win->SetName(name);
#if wxUSE_ACCESSIBILITY
    win->SetAccessible(new NativeCtrlAccessible(win));
#endif
}

// A read-only wxTextCtrl that Tab still reaches.
//
// wxMSW's wxTextCtrl::AcceptsFocusFromKeyboard() is `(IsEditable() || IsMultiLine()) &&
// wxControl::AcceptsFocus()`, so a single-line wxTE_READONLY control is skipped by wx's
// dialog navigation (wxControlContainer::HandleOnNavigationKey) and Tab never lands on it -
// which defeats the whole reason for using a text control instead of a static text. Only
// keyboard reachability is restored here; the control stays read-only, so it still reports
// the READONLY state and still rejects typing.
class ReadOnlyFocusableTextCtrl : public wxTextCtrl
{
public:
    using wxTextCtrl::wxTextCtrl;

    // The plain wxWindow rule (focusable and not opted out), i.e. wxTextCtrl's without its
    // editable test.
    bool AcceptsFocusFromKeyboard() const override { return wxWindow::AcceptsFocusFromKeyboard(); }
};

} // namespace

// The text controls accept whatever the user's locale uses as a decimal separator, and fall
// back to the C locale so that values pasted from elsewhere still parse.
static bool parse_double(const wxString& text, double& value)
{
    wxString trimmed = text;
    trimmed.Trim(true).Trim(false);
    if (trimmed.IsEmpty())
        return false;
    return trimmed.ToDouble(&value) || trimmed.ToCDouble(&value);
}

bool TransformDialog::can_open()
{
    if (wxGetApp().plater() == nullptr || wxGetApp().obj_manipul() == nullptr)
        return false;

    GLCanvas3D* canvas = wxGetApp().plater()->get_view3D_canvas3D();
    return canvas != nullptr && !canvas->get_selection().is_empty();
}

void TransformDialog::show_for_selection(wxWindow* parent)
{
    GizmoObjectManipulation* manipulation = wxGetApp().obj_manipul();
    if (manipulation == nullptr)
        return;

    if (!can_open()) {
        wxGetApp().plater()->get_notification_manager()->push_notification(
            NotificationType::CustomNotification, NotificationManager::NotificationLevel::RegularNotificationLevel,
            _u8L("Select an object or part first to edit its transform."));
        return;
    }

    TransformDialog dialog(parent != nullptr ? parent : static_cast<wxWindow*>(wxGetApp().mainframe), *manipulation);
    dialog.ShowModal();
}

TransformDialog::TransformDialog(wxWindow* parent, GizmoObjectManipulation& manipulation)
    : DPIDialog(parent, wxID_ANY, _L("Transform"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
    , m_manipulation(manipulation)
{
    // Refresh the panel's cached values from the current selection before reading them.
    m_manipulation.UpdateAndShow(true);

    // Positions and sizes are cached in display units, so the unit shown here is the one the
    // values are already in and the one on_change() expects back.
    const wxString length_unit = m_manipulation.m_imperial_units ? _L_CONTEXT("in", "inches") : _L("mm");
    const wxString degree_unit = wxString::FromUTF8("\xc2\xb0");

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    // The coordinate system is owned by the gizmo panels; this dialog mirrors the current mode
    // rather than changing it. A read-only text control rather than a static text so that Tab
    // reaches it and it is therefore announced - see ReadOnlyFocusableTextCtrl, which is what
    // makes a read-only control keyboard-reachable at all under wxMSW.
    wxString coordinates;
    switch (m_manipulation.get_coordinates_type()) {
    case ECoordinatesType::Instance: coordinates = _L("Object"); break;
    case ECoordinatesType::Local:    coordinates = _L("Part");   break;
    default:                         coordinates = _L("World");  break;
    }

    wxBoxSizer*   coordinates_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* coordinates_label = new wxStaticText(this, wxID_ANY, _L("Coordinate system") + ":");
    wxTextCtrl*   coordinates_value = new ReadOnlyFocusableTextCtrl(this, wxID_ANY, coordinates, wxDefaultPosition,
                                                                    wxDefaultSize, wxTE_READONLY);
    set_accessible_name(coordinates_value, _L("Coordinate system"));
    coordinates_sizer->Add(coordinates_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    coordinates_sizer->Add(coordinates_value, 1, wxALIGN_CENTER_VERTICAL);
    main_sizer->Add(coordinates_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    // The move and scale labels come from the panel's own state, so a group selection reads
    // "Translate" and "Scale" exactly as it does in the gizmo panels.
    main_sizer->Add(create_group(gPosition, _L(m_manipulation.m_new_move_label_string), length_unit, m_manipulation.m_buffered_position),
                    0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    main_sizer->Add(create_group(gRotation, _L("Rotation"), degree_unit, m_manipulation.m_buffered_absolute_rotation),
                    0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    main_sizer->Add(create_group(gRotateBy, _L("Rotate by"), degree_unit, m_manipulation.m_buffered_rotation),
                    0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    main_sizer->Add(create_group(gScale, _L(m_manipulation.m_new_scale_label_string), "%", m_manipulation.m_buffered_scale),
                    0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    main_sizer->Add(create_group(gSize, _L("Size"), length_unit, m_manipulation.m_buffered_size),
                    0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    m_initial_uniform_scale = m_manipulation.get_uniform_scaling();
    m_uniform_scale         = new wxCheckBox(this, wxID_ANY, _L("Uniform scale"));
    // wxCheckBox does supply its own accessible (wxCheckBoxAccessible), so unlike the text
    // controls above it only needs a name, not one of ours: wxWindowAccessible::GetName()
    // special-cases wxButton and otherwise falls back to wxWindow::GetName(), which without
    // this is the wxCheckBoxNameStr default and is read out as "check". Attaching a
    // NativeCtrlAccessible here would replace wxCheckBoxAccessible and lose the role and
    // checked state it reports while the control is owner-drawn, which it is as soon as
    // UpdateDlgDarkUI() gives it a foreground colour.
    m_uniform_scale->SetName(_L("Uniform scale"));
    m_uniform_scale->SetValue(m_initial_uniform_scale);
    main_sizer->Add(m_uniform_scale, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    main_sizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, FromDIP(12));

    SetSizerAndFit(main_sizer);
    Layout();
    CenterOnParent();

    wxGetApp().UpdateDlgDarkUI(this);

    if (m_fields[gPosition][0] != nullptr) {
        m_fields[gPosition][0]->SetFocus();
        m_fields[gPosition][0]->SelectAll();
    }
}

// One labelled group per transform, so that a screen reader announces the group name on entry
// and the axis letter on each field.
wxSizer* TransformDialog::create_group(Group group, const wxString& name, const wxString& unit, const Vec3d& values)
{
    m_initial[group] = values;

    wxStaticBoxSizer* box    = new wxStaticBoxSizer(wxVERTICAL, this, wxString::Format("%s (%s)", name, unit));
    wxWindow*         holder = box->GetStaticBox();
    wxFlexGridSizer*  grid   = new wxFlexGridSizer(0, 6, FromDIP(4), FromDIP(6));

    for (int axis = 0; axis < 3; ++axis) {
        // The label stays the visible, sighted-user cue; the accessible name below is the
        // fully qualified one, because the platform's "name from the preceding label" rule
        // would otherwise give all five groups the same three names ("X:", "Y:", "Z:").
        wxStaticText* label = new wxStaticText(holder, wxID_ANY, wxString(g_axis_names[axis]) + ":");
        wxTextCtrl*   field = new wxTextCtrl(holder, wxID_ANY, double_to_string(values(axis), 2), wxDefaultPosition,
                                             wxSize(FromDIP(80), -1));
        set_accessible_name(field, wxString::Format("%s %s", name, g_axis_names[axis]));

        grid->Add(label, 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(field, 0, wxALIGN_CENTER_VERTICAL);
        m_fields[group][axis] = field;
    }

    box->Add(grid, 0, wxALL, FromDIP(6));
    return box;
}

void TransformDialog::apply_group(Group group, const std::string& opt_key)
{
    for (int axis = 0; axis < 3; ++axis) {
        wxTextCtrl* field = m_fields[group][axis];
        if (field == nullptr)
            continue;

        double value = 0.;
        // Anything that does not parse is treated as "not edited" rather than as zero.
        if (!parse_double(field->GetValue(), value))
            continue;
        if (std::abs(value - m_initial[group](axis)) < EPSILON)
            continue;

        m_manipulation.on_change(opt_key, axis, value);
    }
}

void TransformDialog::apply()
{
    // Uniform scale first: it decides how the scale and size edits below are spread over the
    // axes, exactly as ticking the panel's checkbox before typing would.
    if (m_uniform_scale != nullptr && m_uniform_scale->GetValue() != m_initial_uniform_scale)
        m_manipulation.set_uniform_scaling(m_uniform_scale->GetValue());

    // One axis at a time through the panel's own entry point, which takes the undo snapshot and
    // refreshes the cache for us. The order is fixed so that editing several groups at once is
    // predictable: a size edit wins over a scale edit on the same axis, because size runs last.
    apply_group(gPosition, "position");
    apply_group(gRotation, "absolute_rotation");
    apply_group(gRotateBy, "rotation");
    apply_group(gScale,    "scale");
    apply_group(gSize,     "size");
}

bool TransformDialog::TransferDataFromWindow()
{
    apply();
    return true;
}

void TransformDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    Layout();
    Fit();
    Refresh();
}

}} // namespace Slic3r::GUI
