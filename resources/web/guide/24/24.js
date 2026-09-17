// UNIQUE FUNCTIONS

// Keep in here for future additions
function OnInit()
{
	//let strInput=JSON.stringify(cData);
	//HandleModelList(cData);
	
	TranslatePage();
	FocusPageHeading();
	RequestProfile();
}

function CancelSelect()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="user_guide_cancel";
	tSend['data']={};
		
	SendWXMessage( JSON.stringify(tSend) );			
}

function ConfirmSelect()
{
	let nChoose=OnExitFilter();
	
	if(nChoose>0)
    {
		var tSend={};
		tSend['sequence_id']=Math.round(new Date() / 1000);
		tSend['command']="user_guide_finish";
		tSend['data']={};
		tSend['data']['action']="finish";
		
		SendWXMessage( JSON.stringify(tSend) );			
	}
}

function CheckForNewPrinters()
{
	// ORCA: pointer-events only stopped the mouse - Enter/Space on the focused button still
	// sent a second request, and nothing told a screen reader the button was busy. The native
	// `disabled` property fixed both, but a disabled element cannot hold focus: activeElement
	// fell back to <body>, so the busy state was never announced and the next keypress landed
	// somewhere else. aria-disabled keeps the button focusable and in the tab order while still
	// reporting disabled to assistive tech, so pair it with the guard below - Enter, Space and
	// a mouse click on a native <button> all reach this function via `onclick`, so gating here
	// blocks the repeat activation that aria-disabled alone does not prevent.
	var button = document.getElementById("CheckNewPrintersBtn");
	if (button && button.getAttribute("aria-disabled") === "true")
		return;

	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="check_for_new_printers";
	tSend['data']={};

	if (button) {
		SetAriaState(button, "aria-disabled", true);
		button.style.opacity = "0.6";
	}

	SendWXMessage( JSON.stringify(tSend) );
}

function CreateNewPrinter()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="user_guide_create_printer";
	tSend['data']={};
		
	SendWXMessage( JSON.stringify(tSend) );			
}
