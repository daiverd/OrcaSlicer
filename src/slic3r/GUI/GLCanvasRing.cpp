#include "GLCanvasRing.hpp"

#include "GLCanvas3D.hpp"
#include "GLModel.hpp"
#include "GLShader.hpp"
#include "GLToolbar.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "Plater.hpp"
#include "Gizmos/GLGizmoEmboss.hpp"

#include <algorithm>

#include <wx/event.h>

namespace Slic3r {
namespace GUI {

// A rectangular frame of the given thickness, drawn inwards from the given edges.
static void render_outline(float left, float right, float top, float bottom, float thickness_x, float thickness_y, const ColorRGBA& color)
{
    const float inner_left = left + thickness_x;
    const float inner_right = right - thickness_x;
    const float inner_top = top - thickness_y;
    const float inner_bottom = bottom + thickness_y;

    GLModel::Geometry init_data;
    init_data.format = { GLModel::Geometry::EPrimitiveType::Triangles, GLModel::Geometry::EVertexLayout::P2 };
    init_data.color = color;
    init_data.reserve_vertices(16);
    init_data.reserve_indices(24);

    auto add_quad = [&init_data](float l, float r, float t, float b) {
        const unsigned int first = (unsigned int)init_data.vertices_count();
        init_data.add_vertex(Vec2f(l, b));
        init_data.add_vertex(Vec2f(r, b));
        init_data.add_vertex(Vec2f(r, t));
        init_data.add_vertex(Vec2f(l, t));
        init_data.add_triangle(first, first + 1, first + 2);
        init_data.add_triangle(first, first + 2, first + 3);
    };

    add_quad(left, right, top, inner_top);
    add_quad(left, right, inner_bottom, bottom);
    add_quad(left, inner_left, inner_top, inner_bottom);
    add_quad(inner_right, right, inner_top, inner_bottom);

    GLModel model;
    model.init_from(std::move(init_data));
    model.render();
}

// Two pixels of accent inside the item and one neutral pixel around it, so that the
// ring reads as focus on both the dark toolbar background and the light icons.
static void render_focus_rect(float left, float right, float top, float bottom, float pixel_x, float pixel_y)
{
    GLShaderProgram* shader = wxGetApp().get_shader("flat");
    if (shader == nullptr)
        return;

    shader->start_using();
    shader->set_uniform("view_model_matrix", Transform3d::Identity());
    shader->set_uniform("projection_matrix", Transform3d::Identity());
    render_outline(left - pixel_x, right + pixel_x, top + pixel_y, bottom - pixel_y, pixel_x, pixel_y, ColorRGBA::WHITE());
    render_outline(left, right, top, bottom, 2.0f * pixel_x, 2.0f * pixel_y, ColorRGBA::ORCA());
    shader->stop_using();
}

bool GLCanvasKeyboardRing::focus_next_stop()
{
    for (int stop = m_focused_stop + 1; stop < Num_Stops; ++stop) {
        if (!stop_of(stop).entries.empty()) {
            m_focused_stop = stop;
            m_canvas.set_as_dirty();
            return true;
        }
    }

    m_focused_stop = Scene_Stop;
    m_canvas.set_as_dirty();
    return false;
}

bool GLCanvasKeyboardRing::focus_previous_stop()
{
    if (!has_focus())
        return false;

    for (int stop = m_focused_stop - 1; stop >= 0; --stop) {
        if (!stop_of(stop).entries.empty()) {
            m_focused_stop = stop;
            m_canvas.set_as_dirty();
            return true;
        }
    }

    m_focused_stop = Scene_Stop;
    m_canvas.set_as_dirty();
    return true;
}

void GLCanvasKeyboardRing::reset()
{
    m_focused_stop = Scene_Stop;
    for (Parked& parked : m_parked) {
        parked = Parked();
    }
}

bool GLCanvasKeyboardRing::on_key(wxKeyEvent& evt)
{
    if (!has_focus())
        return false;

    const int key = evt.GetKeyCode();
    const bool activation = (key == WXK_RETURN || key == WXK_NUMPAD_ENTER || key == WXK_SPACE) && !evt.HasAnyModifiers();
    const bool navigation = key == WXK_LEFT || key == WXK_RIGHT || key == WXK_UP || key == WXK_DOWN ||
                            key == WXK_HOME || key == WXK_END;
    if (!activation && !navigation)
        return false;

    // Act on the key down and swallow the matching key up, which would otherwise reach
    // the meaning the key has at the scene stop.
    if (evt.GetEventType() != wxEVT_KEY_DOWN)
        return true;

    const Stop stop = stop_of(m_focused_stop);
    if (stop.entries.empty()) {
        // the focused toolbar lost its last entry between two key events
        m_focused_stop = Scene_Stop;
        return false;
    }

    const size_t count = stop.entries.size();
    size_t index = parked_index(m_focused_stop, stop.entries);

    if (activation) {
        // the action may switch the canvas, which resets the focus, so nothing below
        activate(stop.entries[index]);
        m_canvas.set_as_dirty();
        return true;
    }

    switch (key)
    {
    case WXK_LEFT:  case WXK_UP:   { index = (index + count - 1) % count; break; }
    case WXK_RIGHT: case WXK_DOWN: { index = (index + 1) % count; break; }
    case WXK_HOME:                 { index = 0; break; }
    case WXK_END:                  { index = count - 1; break; }
    default:                       { break; }
    }

    m_parked[m_focused_stop] = { stop.entries[index], index };
    m_canvas.set_as_dirty();
    return true;
}

void GLCanvasKeyboardRing::render_focus_ring(const GLToolbar& toolbar) const
{
    Entry entry;
    if (!focused_entry(entry) || entry.toolbar != &toolbar)
        return;

    const int item_id = toolbar.get_item_id(entry.item_name);
    float left, right, top, bottom;
    if (item_id < 0 || !toolbar.get_item_rect(item_id, left, right, top, bottom))
        return;

    const Size cnv_size = m_canvas.get_canvas_size();
    const float cnv_w = (float)cnv_size.get_width();
    const float cnv_h = (float)cnv_size.get_height();
    if (cnv_w == 0.0f || cnv_h == 0.0f)
        return;

    // the toolbar layout is in canvas pixels around the canvas center, y growing up
    render_focus_rect(2.0f * left / cnv_w, 2.0f * right / cnv_w, 2.0f * top / cnv_h, 2.0f * bottom / cnv_h,
        2.0f / cnv_w, 2.0f / cnv_h);
}

void GLCanvasKeyboardRing::render_focus_ring(const GLGizmosManager& gizmos) const
{
    Entry entry;
    if (!focused_entry(entry) || entry.toolbar != nullptr)
        return;

    float left, right, top, bottom;
    if (!gizmos.get_gizmo_rect(entry.gizmo, left, right, top, bottom))
        return;

    const Size cnv_size = m_canvas.get_canvas_size();
    const float cnv_w = (float)cnv_size.get_width();
    const float cnv_h = (float)cnv_size.get_height();
    if (cnv_w == 0.0f || cnv_h == 0.0f)
        return;

    // the gizmo strip is in canvas pixels from the top left corner, y growing down
    render_focus_rect(2.0f * left / cnv_w - 1.0f, 2.0f * right / cnv_w - 1.0f,
        1.0f - 2.0f * top / cnv_h, 1.0f - 2.0f * bottom / cnv_h,
        2.0f / cnv_w, 2.0f / cnv_h);
}

GLCanvasKeyboardRing::Stop GLCanvasKeyboardRing::stop_of(int stop) const
{
    Stop out;

    if (stop == GizmoBar) {
        out.name = _L("Gizmo bar");
        const GLGizmosManager& gizmos = m_canvas.get_gizmos_manager();
        if (gizmos.is_enabled()) {
            for (size_t idx : gizmos.get_selectable_idxs()) {
                out.entries.push_back({ nullptr, std::string(), static_cast<GLGizmosManager::EType>(idx) });
            }
        }
        return out;
    }

    out.name = stop == MainToolbar         ? _L("Main toolbar") :
               stop == AssembleViewToolbar ? _L("Assembly view toolbar") :
                                             _L("Sidebar");

    GLToolbar* toolbar = toolbar_of(stop);
    if (toolbar == nullptr || !toolbar->is_enabled())
        return out;

    // Separators are not entries and invisible items are gone, but disabled items stay,
    // so that the toolbar does not change shape as the selection changes.
    for (int id = 0; id < (int)toolbar->get_items_count(); ++id) {
        const GLToolbarItem* item = toolbar->get_item(id);
        if (item->is_separator() || !item->is_visible())
            continue;

        out.entries.push_back({ toolbar, item->get_name(), GLGizmosManager::Undefined });
    }

    return out;
}

GLToolbar* GLCanvasKeyboardRing::toolbar_of(int stop) const
{
    switch (stop)
    {
    case MainToolbar:         { return &m_canvas.m_main_toolbar; }
    case AssembleViewToolbar: { return &m_canvas.m_assemble_view_toolbar; }
    case CollapseToolbar:     { return &wxGetApp().plater()->get_collapse_toolbar(); }
    default:                  { return nullptr; }
    }
}

size_t GLCanvasKeyboardRing::parked_index(int stop, const std::vector<Entry>& entries) const
{
    const Parked& parked = m_parked[stop];
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i] == parked.entry)
            return i;
    }

    // the parked entry is gone: clamp to the nearest surviving one
    return std::min(parked.index, entries.size() - 1);
}

bool GLCanvasKeyboardRing::focused_entry(Entry& entry) const
{
    if (!has_focus())
        return false;

    const Stop stop = stop_of(m_focused_stop);
    if (stop.entries.empty())
        return false;

    entry = stop.entries[parked_index(m_focused_stop, stop.entries)];
    return true;
}

void GLCanvasKeyboardRing::activate(const Entry& entry)
{
    if (entry.toolbar == nullptr) {
        GLGizmosManager& gizmos = m_canvas.get_gizmos_manager();
        if (entry.gizmo == GLGizmosManager::Emboss) {
            // as on a mouse click, the emboss gizmo decides for itself what to open
            if (auto* emboss = dynamic_cast<GLGizmoEmboss*>(gizmos.get_gizmo(GLGizmosManager::Emboss)))
                emboss->on_shortcut_key();
        }
        else
            gizmos.open_gizmo(entry.gizmo);

        return;
    }

    const int item_id = entry.toolbar->get_item_id(entry.item_name);
    if (item_id < 0)
        return;

    entry.toolbar->force_left_action(item_id, m_canvas);

    // do_action leaves a non toggable item hovered, which it is not with no mouse on it
    GLToolbarItem* item = entry.toolbar->get_item(entry.item_name);
    if (item != nullptr && item->get_state() == GLToolbarItem::Hover)
        item->set_state(GLToolbarItem::Normal);
}

} // namespace GUI
} // namespace Slic3r
