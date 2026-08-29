// ORCA the confirm/warning dialog the setup guide pages share (#NoticeMask + #NoticeBody).
// One copy here rather than one per page: the markup and the behaviour are the same on
// every page that has it, and the pages without it never call it.
// NoticeOpener is the element focus came from, so hiding the notice can hand it back.
let NoticeOpener = null;

function ShowNotice( nShow )
{
	if(nShow==0) {
		$("#NoticeMask").hide();
		$("#NoticeBody").hide();

		if(NoticeOpener && document.contains(NoticeOpener))
			NoticeOpener.focus();
		NoticeOpener = null;
	}
	else {
		NoticeOpener = document.activeElement;

		$("#NoticeMask").show();
		$("#NoticeBody").show();

		$("#NoticeBtns button:first").focus();
	}
}

// ORCA every wizard step is a full document load, so after Get Started, Back or Next the
// focus sits on the nameless <body>: arriving on a step announced nothing and the first Tab
// read a context-free "button". Landing the focus on the step's own <h1> makes the arrival
// announce the step. The heading is a target, not a control - tabindex="-1" keeps it out of
// the Tab order and global.css keeps the ring off it, so nothing changes for a sighted user.
// Called at the end of each page's OnInit, after TranslatePage, so the announced heading is
// the translated one. Pages without a heading (the loading splash, the plugin download) have
// nothing to land on and do not call it.
function FocusPageHeading()
{
	let Heading = document.querySelector("h1");
	if (!Heading)
		return;

	Heading.setAttribute("tabindex", "-1");
	Heading.focus();
}

function ClosePage() {
	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "close_page";
	SendWXMessage(JSON.stringify(tSend));
}

window.addEventListener('wheel', function (event) {
    if (event.ctrlKey === true || event.metaKey) {
        event.preventDefault();
    }
}, { passive: false });
