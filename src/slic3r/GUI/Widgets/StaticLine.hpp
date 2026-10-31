#ifndef slic3r_GUI_StaticLine_hpp_
#define slic3r_GUI_StaticLine_hpp_

#include "../wxExtensions.hpp"
#include "wx/window.h"
#include <wx/string.h>
#include <wx/colour.h>
#include <wx/event.h>

class StaticLine : public wxWindow
{
public:
    StaticLine(wxWindow *parent, bool vertical = false, const wxString &label = {}, const wxString &icon = {});

public:
    // ORCA: a decorative rule is never a TAB stop. wxStaticLine, which this class
    // replaces, refuses the focus the same way; deriving from plain wxWindow
    // instead made every separator an invisible, unlabelled stop in TAB traversal.
    bool AcceptsFocus() const override { return false; }

    void SetLabel(const wxString& label) override;

    void SetIcon(const wxString& icon);

    void SetLineColour(wxColour color);
    
    void Rescale();

private:
    wxColour       lineColor;
    bool vertical;
    ScalableBitmap icon;

private:
    void paintEvent(wxPaintEvent& evt);

    void messureSize();

    void render(wxDC &dc);

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_StaticLine_hpp_
