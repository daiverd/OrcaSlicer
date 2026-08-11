
function OnInit()
{
	TranslatePage();

	SendInstallPluginCheck();

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

	document.getElementById('AcceptBtn').addEventListener('keydown', function(event) {
		if (event.key === 'Enter' || event.key === ' ') {
			event.preventDefault();
			FinishGuide();
		}
	});
}



function SendInstallPluginCheck()
{
	let nVal="no";
	if( $('#InstallCheck').is(':checked') ) 
		nVal="yes";
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="network_plugin_install";
	tSend['data']={};
	tSend['data']['action']=nVal;
	
	SendWXMessage( JSON.stringify(tSend) );		
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
