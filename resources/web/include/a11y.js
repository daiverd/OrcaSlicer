// Shared accessibility support for the packaged web pages: the keyboard behavior
// below, plus the guarded state writes at the end of the file.
// Self-contained: one <script> include per page, no init call needed. All
// listeners are document-level and delegated, so include order does not matter;
// page handlers on the elements themselves always run first.

// Enter/Space activates any element that declares an ARIA widget role but is
// not a native control (the `<div onclick>` pattern used across these pages).
// One delegated listener instead of a per-element keydown next to every onclick.
document.addEventListener('keydown', function (e) {
	if (e.defaultPrevented || e.ctrlKey || e.metaKey || e.altKey)
		return;
	if (e.repeat)
		return;   // auto-repeat must not re-activate; a native control fires once per press
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
	var el = t.closest('[role="button"], [role="checkbox"], [role="menuitem"], [role="link"]');
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
	if (e.metaKey && /^Key[QWMH]$|^Comma$/.test(e.code))
		return;   // the macOS app-level Cmd set (quit, close, minimize, hide,
		          // preferences) is the window system's, like the Alt set above;
		          // whether WKWebView hands them to content first is untested,
		          // so never eat them. Matched by physical key (e.code), like the
		          // Ctrl chord whitelist below: e.key is layout-dependent, and on
		          // a non-Latin layout it would miss this allowlist entirely
	if (!e.ctrlKey && !e.metaKey)
		return;   // plain and Shift+ keys belong to the page
	if (e.target.closest('input, textarea') || e.target.isContentEditable)
		return;   // a chord aimed at a text field is the field's: the edit chords
		          // below are only the common ones, and fields also take Ctrl+
		          // Backspace/Delete, Ctrl+Home/End and the browser's own inserts
	if (e.key.length > 1)
		return;   // Ctrl+named-key: the browser's own (Ctrl+Tab, Ctrl+PageUp/Down)
		          // switch tabs, which a single-page webview does not have; the
		          // rest are caret and selection keys, harmless outside a field
	if (/^Key[ACVXYZ]$/.test(e.code))
		return;   // select-all / clipboard / undo-redo editing chords, which work
		          // on read-only content too (copy from a page, select-all).
		          // Matched by physical key (e.code), because the browser resolves
		          // these chords positionally: on a Russian, Greek or Hebrew layout
		          // Ctrl+V still pastes while e.key reports that layout's letter, so
		          // an e.key whitelist would cancel the very paste the browser fired.
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

// One state write per real change. Chromium raises an accessibility state-change event
// for every write to `checked`, `aria-checked` or `aria-pressed` - including a write
// that stores the value the element already held - and a screen reader reads each one.
// The batch handlers on these pages re-stamp a whole group whenever any member changes,
// so the control the user just toggled got written twice and was announced twice
// ("checked / checked", "pressed / pressed"). These two write only where the value
// actually differs. `nodes` is a single element or anything array-like: a jQuery
// object, a NodeList, an array.
function SetCheckedState(nodes, bChecked) {
	var List = nodes.nodeType ? [nodes] : nodes;
	for (var n = 0; n < List.length; n++)
		if (List[n].checked !== bChecked)
			List[n].checked = bChecked;
}

function SetAriaState(nodes, strName, value) {
	var List = nodes.nodeType ? [nodes] : nodes;
	var strValue = String(value);
	for (var n = 0; n < List.length; n++)
		if (List[n].getAttribute(strName) !== strValue)
			List[n].setAttribute(strName, strValue);
}
