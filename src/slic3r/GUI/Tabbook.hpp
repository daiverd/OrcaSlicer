#ifndef slic3r_Tabbook_hpp_
#define slic3r_Tabbook_hpp_

//#ifdef _WIN32

#include <cstddef>
#include <string>
#include <vector>
#include <wx/bookctrl.h>
#include <wx/containr.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/debug.h>
#include <wx/chartype.h>
#include <wx/object.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/window.h>
#include <wx/withimages.h>
#include "wxExtensions.hpp"
#include <wx/defs.h>
#include <wx/notebook.h>

class wxObject;
class wxStaticText;


class TabButton;

// custom message the ButtonsListCtrl sends to its parent (Notebook) to notify a selection change:
wxDECLARE_EVENT(wxCUSTOMEVT_TABBOOK_SEL_CHANGED, wxCommandEvent);

// ORCA: plain wxControl has no TAB-traversal container logic - that only exists in the
// wxNavigationEnabled<W> mixin, which wxPanel gets for free. Without it this tab strip
// was a Tab trap: the strip carries wxTAB_TRAVERSAL, so it is where wxMSW's message
// pre-processing turns a Tab press into a wxNavigationKeyEvent and fires it on the
// strip's own event handler chain. wxNavigationKeyEvent is a plain wxEvent, so it does
// not propagate to a grandparent, and the only thing that ever handles it is the
// binding wxNavigationEnabled installs. With nothing bound here the event was dropped
// and ::IsDialogMessage() then ate the key, so focus never left the strip. Same defect
// as ButtonsListCtrl (Notebook) and StaticBox had.
class TabButtonsListCtrl : public wxNavigationEnabled<wxControl>
{
public:
    // BBS
    TabButtonsListCtrl(wxWindow *parent, wxBoxSizer *side_tools = NULL);
    ~TabButtonsListCtrl() {}

    void OnPaint(wxPaintEvent&);
    void SetSelection(int sel);
    void showNewTag(int sel, bool show = false);
    void Rescale();
    bool InsertPage(size_t n, const wxString& text, bool bSelect = false, const std::string& bmp_name = "");
    void RemovePage(size_t n);
    bool SetPageImage(size_t n, const std::string& bmp_name);
    void SetPageText(size_t n, const wxString& strText);
    wxString GetPageText(size_t n) const;
    const wxSize& GetPaddingSize(size_t n);
    void SetPaddingSize(const wxSize& size);
    void SetFooterText(const wxString& text);
    // ORCA: button of the currently selected page, the landing target for keyboard pane navigation
    wxWindow* GetSelectedButton() const;
    TabButton*                      pageButton;

private:
    // ORCA: shared by the click handler and the arrow-key handler below - both need to
    // update the strip's own selection state (handing over the roving tabindex/focus
    // immediately) and then notify the parent Tabbook so it flips the associated page.
    void SelectAndNotify(size_t idx);

    wxFlexGridSizer*                m_buttons_sizer;
    wxBoxSizer*                     m_sizer;
    ScalableBitmap                  m_arrow_img;
    std::vector<TabButton*>         m_pageButtons;
    int                             m_selection {-1};
    int                             m_btn_margin;
    int                             m_line_margin;
    wxStaticText*                   m_footer_text {nullptr};
};

class Tabbook: public wxBookCtrlBase
{
public:
    Tabbook(wxWindow *     parent,
                 wxWindowID winid = wxID_ANY,
                 const wxPoint & pos = wxDefaultPosition,
                 const wxSize & size = wxDefaultSize,
                // BBS
                 wxBoxSizer* side_tools = NULL,
                 long style = 0)
    {
        Init();
        Create(parent, winid, pos, size, side_tools, style);
    }

    bool Create(wxWindow * parent,
                wxWindowID winid = wxID_ANY,
                const wxPoint & pos = wxDefaultPosition,
                const wxSize & size = wxDefaultSize,
                // BBS
                wxBoxSizer* side_tools = NULL,
                long style = 0)
    {
        if (!wxBookCtrlBase::Create(parent, winid, pos, size, style))
            return false;

        m_bookctrl = new TabButtonsListCtrl(this, side_tools);

        wxSizer* mainSizer = new wxBoxSizer(IsVertical() ? wxVERTICAL : wxHORIZONTAL);

        if (style & wxBK_RIGHT || style & wxBK_BOTTOM)
            mainSizer->Add(0, 0, 1, wxEXPAND, 0);

        m_controlSizer = new wxBoxSizer(IsVertical() ? wxHORIZONTAL : wxVERTICAL);
        m_controlSizer->Add(m_bookctrl, wxSizerFlags(0).Expand());
        wxSizerFlags flags;
        if (IsVertical())
            flags.Expand();
        else
            flags.Expand();
        mainSizer->Add(m_controlSizer, flags.Border(wxALL, m_controlMargin));
        SetSizer(mainSizer);

        this->Bind(wxCUSTOMEVT_TABBOOK_SEL_CHANGED, [this](wxCommandEvent& evt)
        {
            if (int page_idx = evt.GetId(); page_idx >= 0)
                SetSelection(page_idx);
        });

        this->Bind(wxEVT_NAVIGATION_KEY, &Tabbook::OnNavigationKey, this);

        return true;
    }


    // Methods specific to this class.

    // A method allowing to add a new page without any label (which is unused
    // by this control) and show it immediately.
    bool ShowNewPage(wxWindow * page)
    {
        return AddPage(page, wxString());
    }

    // Set effect to use for showing/hiding pages.
    void SetEffects(wxShowEffect showEffect, wxShowEffect hideEffect)
    {
        m_showEffect = showEffect;
        m_hideEffect = hideEffect;
    }

    // Or the same effect for both of them.
    void SetEffect(wxShowEffect effect)
    {
        SetEffects(effect, effect);
    }

    // And the same for time outs.
    void SetEffectsTimeouts(unsigned showTimeout, unsigned hideTimeout)
    {
        m_showTimeout = showTimeout;
        m_hideTimeout = hideTimeout;
    }

    void SetEffectTimeout(unsigned timeout)
    {
        SetEffectsTimeouts(timeout, timeout);
    }


    // Implement base class pure virtual methods.

    bool AddPage(wxWindow* page,
                 const wxString& text,
                 bool bSelect = false,
                 int imageId = NO_IMAGE) override
    {
        DoInvalidateBestSize();
        return InsertPage(GetPageCount(), page, text, bSelect, imageId);
    }

    //// Page management
    virtual bool InsertPage(size_t n,
                            wxWindow * page,
                            const wxString & text,
                            bool bSelect = false,
                            int imageId = NO_IMAGE) override
    {
        if (!wxBookCtrlBase::InsertPage(n, page, text, bSelect, imageId))
            return false;

        GetBtnsListCtrl()->InsertPage(n, text, bSelect);

        if (!DoSetSelectionAfterInsertion(n, bSelect))
            page->Hide();

        return true;
    }

    bool RemovePage(size_t n) override
    {
        if (!wxBookCtrlBase::RemovePage(n))
            return false;

        SetSelection(0);
        return true;
    }

    virtual int SetSelection(size_t n) override
    {
        GetBtnsListCtrl()->SetSelection(n);
        int ret = DoSetSelection(n, SetSelection_SendEvent);

        // check that only the selected page is visible and others are hidden:
        for (size_t page = 0; page < m_pages.size(); page++) {
            wxWindow* win_a = GetPage(page);
            wxWindow* win_b = GetPage(n);
            if (page != n && GetPage(page) != GetPage(n)) {
                m_pages[page]->Hide();
            }
        }

        return ret;
    }

    virtual int ChangeSelection(size_t n) override
    {
        GetBtnsListCtrl()->SetSelection(n);
        return DoSetSelection(n);
    }

    // Neither labels nor images are supported but we still store the labels
    // just in case the user code attaches some importance to them.
    virtual bool SetPageText(size_t n, const wxString & strText) override
    {
        wxCHECK_MSG(n < GetPageCount(), false, wxS("Invalid page"));

        GetBtnsListCtrl()->SetPageText(n, strText);

        return true;
    }

    virtual wxString GetPageText(size_t n) const override
    {
        wxCHECK_MSG(n < GetPageCount(), wxString(), wxS("Invalid page"));
        return GetBtnsListCtrl()->GetPageText(n);
    }

    virtual bool SetPageImage(size_t WXUNUSED(n), int WXUNUSED(imageId)) override
    {
        return false;
    }

    virtual int GetPageImage(size_t WXUNUSED(n)) const override
    {
        return NO_IMAGE;
    }

    bool SetPageImage(size_t n, const std::string& bmp_name)
    {
        return GetBtnsListCtrl()->SetPageImage(n, bmp_name);
    }

    // Override some wxWindow methods too.
    virtual void SetFocus() override
    {
        wxWindow* const page = GetCurrentPage();
        if (page)
            page->SetFocus();
    }

    TabButtonsListCtrl *GetBtnsListCtrl() const { return static_cast<TabButtonsListCtrl *>(m_bookctrl); }

    void Rescale()
    {
        GetBtnsListCtrl()->Rescale();
    }

    void SetFooterText(const wxString& text)
    {
        GetBtnsListCtrl()->SetFooterText(text);
    }

    void OnNavigationKey(wxNavigationKeyEvent& event)
    {
        if (event.IsWindowChange()) {
            // change pages
            //AdvanceSelection(event.GetDirection());
            this->GetGrandParent()->HandleWindowEvent(event);
        }
        else {
            // we get this event in 3 cases
            //
            // a) one of our pages might have generated it because the user TABbed
            // out from it in which case we should propagate the event upwards and
            // our parent will take care of setting the focus to prev/next sibling
            //
            // or
            //
            // b) the parent panel wants to give the focus to us so that we
            // forward it to our selected page. We can't deal with this in
            // OnSetFocus() because we don't know which direction the focus came
            // from in this case and so can't choose between setting the focus to
            // first or last panel child
            //
            // or
            //
            // c) we ourselves (see MSWTranslateMessage) generated the event
            //
            wxWindow* const parent = GetParent();

            // the wxObject* casts are required to avoid MinGW GCC 2.95.3 ICE
            const bool isFromParent = event.GetEventObject() == (wxObject*)parent;
            const bool isFromSelf = event.GetEventObject() == (wxObject*)this;
            // ORCA: our tab strip is a child window of its own rather than a part of this
            // control, so everything wxNotebook deals with as coming from its tab area
            // reaches us as a from-child event and has to be told apart from a page's.
            const bool isFromBtns = IsFromTabStrip(event);
            const bool isForward = event.GetDirection();

            if ((isFromSelf || isFromBtns) && !isForward)
            {
                // focus is currently on notebook tab and should leave
                // it backwards (Shift-TAB)
                event.SetCurrentFocus(this);
                if (!parent->HandleWindowEvent(event) &&
                    !(m_selection != wxNOT_FOUND && WrapNavigationInto(m_pages[m_selection], event)))
                    event.Skip();
            }
            else if (isFromParent || isFromSelf || isFromBtns)
            {
                // no, it doesn't come from child, case (b) or (c): forward to a
                // page but only if entering notebook page (i.e. direction is
                // backwards (Shift-TAB) comething from out-of-notebook, or
                // direction is forward (TAB) from ourselves),
                if (m_selection != wxNOT_FOUND &&
                    (!isForward || isFromSelf || isFromBtns))
                {
                    // so that the page knows that the event comes from it's parent
                    // and is being propagated downwards
                    event.SetEventObject(this);

                    wxWindow* page = m_pages[m_selection];
                    if (!page->HandleWindowEvent(event))
                    {
                        page->SetFocus();
                    }
                    //else: page manages focus inside it itself
                }
                else // otherwise set the focus to the tab strip
                {
                    SetFocusToTabStrip();
                }
            }
            else
            {
                // it comes from our child, case (a), pass to the parent, but only
                // if the direction is forwards. Otherwise set the focus to the
                // tab strip, which is always the 'first' control of a page.
                if (!isForward)
                {
                    SetFocusToTabStrip();
                }
                else if (parent)
                {
                    event.SetCurrentFocus(this);
                    // ORCA: every host panel of ours - AuxiliaryPanel, MonitorPanel,
                    // CalibrationPanel, MultiMachinePage - hands us its side tools and then
                    // holds nothing but us, so it has nowhere to send a forward key and this is
                    // the end of the traversal cycle. Wrap round to the tab strip ourselves;
                    // only if even that finds nothing to focus is the event left skipped, or
                    // wxControlContainer::HandleOnNavigationKey() reads "processed" from our
                    // return value and stops both escalating and looking for a control to focus.
                    if (!parent->HandleWindowEvent(event) &&
                        !WrapNavigationInto(GetBtnsListCtrl(), event))
                        event.Skip();
                }
            }
        }
    }

protected:
    virtual void UpdateSelectedPage(size_t WXUNUSED(newsel)) override
    {
        // Nothing to do here, but must be overridden to avoid the assert in
        // the base class version.
    }

    virtual wxBookCtrlEvent * CreatePageChangingEvent() const override
    {
        return new wxBookCtrlEvent(wxEVT_BOOKCTRL_PAGE_CHANGING,
                                   GetId());
    }

    virtual void MakeChangedEvent(wxBookCtrlEvent & event) override
    {
        event.SetEventType(wxEVT_BOOKCTRL_PAGE_CHANGED);
    }

    virtual wxWindow * DoRemovePage(size_t page) override
    {
        wxWindow* const win = wxBookCtrlBase::DoRemovePage(page);
        if (win)
        {
            GetBtnsListCtrl()->RemovePage(page);
            DoSetSelectionAfterRemoval(page);
        }

        return win;
    }

    virtual void DoSize() override
    {
        wxWindow* const page = GetCurrentPage();
        if (page)
            page->SetSize(GetPageRect());
    }

    virtual void DoShowPage(wxWindow * page, bool show) override
    {
        if (show)
            page->ShowWithEffect(m_showEffect, m_showTimeout);
        else
            page->HideWithEffect(m_hideEffect, m_hideTimeout);
    }

private:
    // ORCA: a Tab press leaving the strip reaches us still stamped with the window it
    // started from, never with the strip itself: wxWindow::Navigate() makes the focused
    // window the event object, and wxControlContainer::HandleOnNavigationKey()'s
    // escalate-to-parent loop rewrites only the event's *current focus* on the way up.
    // Comparing against the strip by identity therefore missed the "Return" button
    // reparented into it, so its Shift+Tab was treated as a page's and bounced straight
    // back to the selected tab. Ask for membership instead; pages are siblings of the
    // strip rather than descendants of it, so they still take the from-a-page branch.
    bool IsFromTabStrip(const wxNavigationKeyEvent& event) const
    {
        const wxWindow* const btns = GetBtnsListCtrl();
        for (const wxWindow* win = wxDynamicCast(event.GetEventObject(), wxWindow);
             win != nullptr && win != this; win = win->GetParent())
            if (win == btns)
                return true;
        return false;
    }

    // ORCA: closes the traversal cycle when nothing outside us wants the key. Without this
    // the container that started the walk falls back on wrapping inside itself, which
    // strands the focus in whichever half of the book control it started in - the page
    // cycling over its own fields forever, the strip over its own buttons. Re-entering
    // `win` as if the event came from us restarts the walk at its first (forward) or last
    // (backward) child, exactly like a container wrapping round, so the cycle spans strip
    // and page alike. The event object is restored if `win` has nothing to focus either,
    // so an unhandled event reaches the caller as it arrived. The Notebook twin needs no
    // counterpart: its pages have deeper ancestors of their own to wrap inside.
    bool WrapNavigationInto(wxWindow* win, wxNavigationKeyEvent& event)
    {
        wxObject* const from = event.GetEventObject();
        event.SetEventObject(this);
        if (win->HandleWindowEvent(event))
            return true;
        event.SetEventObject(from);
        return false;
    }

    // ORCA: the roving tabindex makes the selected tab the strip's only Tab stop, and the
    // side tools reparented into the strip are its first children, so a plain SetFocus()
    // on the strip would land on one of those instead.
    void SetFocusToTabStrip()
    {
        if (wxWindow* const tab = GetBtnsListCtrl()->GetSelectedButton())
            tab->SetFocusFromKbd();
        else
            GetBtnsListCtrl()->SetFocus();
    }

    void Init()
    {
        // We don't need any border as we don't have anything to separate the
        // page contents from.
        SetInternalBorder(0);

        // No effects by default.
        m_showEffect =
        m_hideEffect = wxSHOW_EFFECT_NONE;

        m_showTimeout =
        m_hideTimeout = 0;
    }

    wxShowEffect m_showEffect,
                 m_hideEffect;

    unsigned m_showTimeout,
             m_hideTimeout;

};
//#endif // _WIN32
#endif // slic3r_Tabbook_hpp_
