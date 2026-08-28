#pragma once

#include <wx/textctrl.h>
#include <wx/colour.h>

#ifdef __WXMSW__
#include "WidgetAccessible.hpp"

class TextCtrl : public wxTextCtrl
{
public:
    using wxTextCtrl::wxTextCtrl;
    WXHBRUSH DoMSWControlColor(WXHDC pDC, wxColour colBg, WXHWND hWnd) { return wxTextCtrl::DoMSWControlColor(pDC, wxColour(), hWnd); }

#if wxUSE_ACCESSIBILITY
    // ORCA: TextInput and SpinInput forward their accessible name to this inner
    // control, but the native EDIT's own accessible cannot see wxWindow::SetName()
    // and has no static label to fall back on, so the field is announced unnamed.
    // NativeCtrlAccessible publishes the name and leaves the rest to the EDIT.
    wxAccessible* CreateAccessible() override { return new NativeCtrlAccessible(this); }
#endif
};
#else
typedef wxTextCtrl TextCtrl;
#endif
