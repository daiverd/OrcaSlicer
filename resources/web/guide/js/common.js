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
