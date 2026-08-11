
function OnInit()
{
	TranslatePage();

	SendStealthModeCheck();

	BindKeyboardHandlers();
}

function BindKeyboardHandlers()
{
	document.getElementById('PreBtn').addEventListener('keydown', function(event) {
		if (event.key === 'Enter' || event.key === ' ') {
			event.preventDefault();
			window.history.back();
		}
	});

	document.getElementById('GotoNetPluginBtn').addEventListener('keydown', function(event) {
		if (event.key === 'Enter' || event.key === ' ') {
			event.preventDefault();
			GotoNetPluginPage();
		}
	});
}



function SendStealthModeCheck()
{
	let nVal="no";
	if( $('#StealthMode').is(':checked') ) 
		nVal="yes";
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="save_stealth_mode";
	tSend['data']={};
	tSend['data']['action']=nVal;
	
	SendWXMessage( JSON.stringify(tSend) );

	return true;
}

function GotoNetPluginPage()
{
	let bRet=SendStealthModeCheck();
	
	if(bRet)
		window.location.href="../5/index.html";
}


function FinishGuide()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="user_guide_finish";
	tSend['data']={};
	tSend['data']['action']="finish";
	
	SendWXMessage( JSON.stringify(tSend) );	
	
	//window.location.href="../6/index.html";
}
