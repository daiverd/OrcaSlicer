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

// Keep the webview's browser-chrome shortcuts (reload, find bar, print, zoom,
// save/open page, view source, downloads) out of these pages - they are
// application UI, not websites. Successor to the old blanket key canceller,
// which also swallowed the Tab/Enter/Space/arrow keys needed for keyboard use.
document.addEventListener('keydown', function (e) {
	if (e.key === 'F5' || e.key === 'F3') {
		e.preventDefault();
		return;
	}
	if ((e.ctrlKey || e.metaKey) && e.key.length === 1 && 'rfgpsouj+-=0'.indexOf(e.key.toLowerCase()) !== -1)
		e.preventDefault();
});
