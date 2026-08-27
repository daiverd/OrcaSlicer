// Shared keyboard-accessibility behavior for the packaged web pages.
// Self-contained: one <script> include per page, no init call needed. All
// listeners are document-level and delegated, so include order does not matter;
// page handlers on the elements themselves always run first.

// Enter/Space activates any element that declares an ARIA widget role but is
// not a native control (the `<div onclick>` pattern used across these pages).
// One delegated listener instead of a per-element keydown next to every onclick.
document.addEventListener('keydown', function (e) {
	if (e.defaultPrevented || e.ctrlKey || e.metaKey || e.altKey)
		return;
	var enter = e.key === 'Enter';
	if (!enter && e.key !== ' ')
		return;
	var t = e.target;
	if (!t || !t.closest)
		return;   // key arrived with no focused element (target is the document)
	// native controls handle these keys themselves - never hijack a key aimed at one,
	// including a button or link sitting inside a widget row (a plugin row, a bundle row).
	if (t.closest('input, textarea, select, button, a[href]') || t.isContentEditable)
		return;
	var el = t.closest('[role="button"], [role="checkbox"], [role="tab"], [role="menuitem"], [role="option"], [role="link"]');
	if (!el)
		return;
	if (el.getAttribute('role') === 'link' && !enter)
		return;   // links activate on Enter only
	e.preventDefault();
	el.click();
});

// Keep the webview's browser-chrome shortcuts out of these pages - they are
// application UI, not websites. Deny by default: cancel every function key and
// unclaimed Ctrl/Meta chord instead of enumerating the browser's bindings
// (reload, find bar, print, zoom, view source, pane cycling, ...). Successor
// to the old blanket key canceller, which also swallowed the Tab/Enter/Space/
// arrow keys needed for keyboard use - so plain keys pass through untouched.
// Pages that repurpose one of these chords (the homepage forwards Ctrl chords
// to the app's shortcut bridge) keep their own listeners, which a cancelled
// default does not stop.
document.addEventListener('keydown', function (e) {
	if (e.defaultPrevented || !e.target || !e.target.closest)
		return;
	if (/^F\d\d?$/.test(e.key)) {
		// Alt+F-key combos are the window system's (Alt+F4 closes the app),
		// not the browser's - cancelling them here would eat the close key.
		// F10 is the window system's too: it moves focus to the host frame's
		// menu bar, a keyboard affordance of its own. Shift+F10 opens the
		// context menu, which the contextmenu listener below already handles.
		if (!e.altKey && e.key !== 'F10')
			e.preventDefault();
		return;
	}
	if (e.altKey)
		return;   // Alt chords are the window system's (menu-bar activation,
		          // Alt+F4, Alt-mnemonics) or AltGr composition - never eat
		          // them, including the bare Alt tap itself. The history keys
		          // Alt+Left/Right do nothing in these single-page views.
	if (e.metaKey && 'qwmh,'.indexOf(e.key.toLowerCase()) !== -1)
		return;   // the macOS app-level Cmd set (quit, close, minimize, hide,
		          // preferences) is the window system's, like the Alt set above;
		          // whether WKWebView hands them to content first is untested,
		          // so never eat them
	if (!e.ctrlKey && !e.metaKey)
		return;   // plain and Shift+ keys belong to the page
	if (e.key.length > 1)
		return;   // Ctrl+named-key: caret movement in fields; the browser's own
		          // (Ctrl+Tab, Ctrl+PageUp/Down) switch tabs, which a single-
		          // page webview does not have
	if ('acvxyz'.indexOf(e.key.toLowerCase()) !== -1)
		return;   // select-all / clipboard / undo-redo editing chords
	e.preventDefault();
});

// The default context menu is browser chrome too, and the keyboard reaches it
// (Shift+F10, the ContextMenu key) as well as the mouse - suppress both routes
// together. Pages with a menu of their own preventDefault on their own element
// first; text fields keep the edit menu.
document.addEventListener('contextmenu', function (e) {
	if (e.defaultPrevented || !e.target || !e.target.closest)
		return;
	if (e.target.closest('input, textarea') || e.target.isContentEditable)
		return;
	e.preventDefault();
});
