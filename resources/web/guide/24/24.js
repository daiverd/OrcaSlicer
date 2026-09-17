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
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="check_for_new_printers";
	tSend['data']={};

	// ORCA: pointer-events only stopped the mouse - Enter/Space on the focused button still
	// sent a second request, and nothing told a screen reader the button was busy. The
	// native disabled state does both; the opacity stays as the visual.
	var button = document.getElementById("CheckNewPrintersBtn");
	if (button) {
		button.disabled = true;
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
