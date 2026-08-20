// Shared keyboard-accessibility behavior for the packaged web pages.
// Self-contained: include after the page's other scripts, no init call needed.

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
// unclaimed modifier chord instead of enumerating the browser's bindings
// (reload, find bar, print, zoom, view source, pane cycling, history
// navigation, ...). Successor to the old blanket key canceller, which also
// swallowed the Tab/Enter/Space/arrow keys needed for keyboard use - so plain
// keys pass through untouched. Pages that repurpose one of these keys (the
// homepage forwards Ctrl chords and F6 to the app's shortcut bridge) keep
// their own listeners, which a cancelled default does not stop.
document.addEventListener('keydown', function (e) {
	if (e.defaultPrevented)
		return;
	var editable = e.target.closest('input, textarea') || e.target.isContentEditable;
	if (/^F\d\d?$/.test(e.key)) {
		// Shift+F10 in a text field opens the edit menu (cut/copy/paste) -
		// that one is page content, not browser chrome
		if (!(e.shiftKey && e.key === 'F10' && editable))
			e.preventDefault();
		return;
	}
	if (!e.ctrlKey && !e.metaKey && !e.altKey)
		return;   // plain and Shift+ keys belong to the page
	if (e.key.length > 1 && !e.altKey)
		return;   // Ctrl+named-key: caret movement in fields; the browser binds none
	if (e.ctrlKey && e.altKey && e.key.length === 1)
		return;   // AltGr-composed characters on European layouts
	if (!e.altKey && 'acvxyz'.indexOf(e.key.toLowerCase()) !== -1)
		return;   // select-all / clipboard / undo-redo editing chords
	e.preventDefault();
});

// The default context menu is browser chrome too, and the keyboard reaches it
// (Shift+F10, the ContextMenu key) as well as the mouse - suppress both routes
// together. Pages with a menu of their own preventDefault on their own element
// first; text fields keep the edit menu, matching the Shift+F10 carve-out.
document.addEventListener('contextmenu', function (e) {
	if (e.defaultPrevented || e.target.closest('input, textarea') || e.target.isContentEditable)
		return;
	e.preventDefault();
});
