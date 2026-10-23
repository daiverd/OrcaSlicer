#ifndef slic3r_GUI_WebView_hpp_
#define slic3r_GUI_WebView_hpp_

#include <wx/string.h>
#include <wx/setup.h>
#include <wx/webview.h>
#include <wx/event.h>

class wxWebView;
class wxWindow;

wxDECLARE_EVENT(EVT_WEBVIEW_RECREATED, wxCommandEvent);

class WebView
{
public:
    // name is what assistive technology announces for the host - pass the tab, page or
    // dialog the view serves, translated. See SetupAccessibility() for why it matters.
    static wxWebView *CreateWebView(wxWindow *parent, wxString const &url, wxString const &name);

    // wx installs no wxAccessible on any of its web view backends, so a host is announced
    // as an unnamed "pane" carrying wx's default window name, "wxWebView". Name it and
    // report the document role instead. CreateWebView() does this for every view it makes;
    // this is public only for the few hosts that build their wxWebView directly.
    static void SetupAccessibility(wxWebView *webView, wxString const &name);

    // What a web view host should answer from AcceptsFocusFromKeyboard(): whether any of
    // its children will take the focus from a Tab. See the definition for why a host has
    // to be asked this explicitly.
    static bool HostAcceptsFocusFromKeyboard(wxWindow const *host);

#if wxUSE_WEBVIEW_EDGE
    static bool CheckWebViewRuntime();
    static bool DownloadAndInstallWebViewRuntime();
#endif
    static void LoadUrl(wxWebView * webView, wxString const &url);

    // Returns the URL truncated at the first '?' or '#', so a query/fragment carrying secrets
    // (e.g. a session-handoff token) is never written to a log.
    static wxString RedactUrl(const wxString& url);

    static bool RunScript(wxWebView * webView, wxString const & msg);

    // Marks "wx" as registered so CreateWebView's deferred add skips the duplicate.
    static void MarkScriptMessageHandlerAdded(wxWebView * webView);

    // On Windows, a WebView2 backend created during a GUI rebuild (language switch) can come up
    // ignoring every navigation. A panel that gets true here recreates its view on first Show().
    static bool NeedsRecreateOnShow();

    // True when the linked WebKitGTK predates 2.42, where accelerated compositing
    // could fail silently on XWayland sessions, so it is forced off as a fallback.
    // False on every other platform. Safe to call before GTK initialises.
    static bool NeedsCompositingWorkaround();

    // True when the linked WebKitGTK is new enough (>= 2.44) to honour
    // WEBKIT_DMABUF_RENDERER_FORCE_SHM, the shared-memory transport switch.
    // False on every other platform. Safe to call before GTK initialises.
    static bool SupportsForceSharedMemory();

    static void RecreateAll();
};

#endif // !slic3r_GUI_WebView_hpp_
