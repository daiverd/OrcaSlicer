var m_OldRegion='';
var m_Region='';

function OnInit()
{
	//let strInput=JSON.stringify(cData);
	//HandleStudio(strInput);
	
	TranslatePage();
	FocusPageHeading();

	InitRegionKeys();

	RequestProfile();
}

// ORCA the keyboard half of the radiogroup (index.html): the arrows move the choice
// within the group and Home/End jump to its ends, while Tab leaves the group entirely -
// only the checked region is a tab stop. Space and Enter come from the buttons
// themselves, whose click already runs ChooseRegion.
function InitRegionKeys()
{
	$('#RegionList').on('keydown', '.RegionItem', function (event) {
		let Items=$('.RegionItem');
		let nCount=Items.length;
		let nNext=Items.index(this);

		if (event.key === 'ArrowDown' || event.key === 'ArrowRight')
			nNext=(nNext+1)%nCount;
		else if (event.key === 'ArrowUp' || event.key === 'ArrowLeft')
			nNext=(nNext+nCount-1)%nCount;
		else if (event.key === 'Home')
			nNext=0;
		else if (event.key === 'End')
			nNext=nCount-1;
		else
			return;

		event.preventDefault();
		ChooseRegion(Items.eq(nNext).attr('region'));
		Items[nNext].focus();
	});
}


function RequestProfile()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="request_userguide_profile";
	
	SendWXMessage( JSON.stringify(tSend) );
}

function HandleStudio( pVal )
{
	let strCmd=pVal['command'];
	//alert(strCmd);
	
	if(strCmd=='response_userguide_profile')
	{
		HandleRegionlList(pVal['response']);
	}
}

function HandleRegionlList(  pItem )
{
	m_OldRegion=pItem['region'];
	
	let nNum=$(".RegionItem[region='"+m_OldRegion+"']").length;
	if( nNum==1 )
		ChooseRegion(m_OldRegion);
}


function ChooseRegion( strRegion )
{
	m_Region=strRegion;

	let $Items=$('.RegionItem');
	let $Chosen=$Items.filter("[region='"+strRegion+"']");
	let $Rest=$Items.not($Chosen);

	$Items.removeClass('RegionSelected');
	$Chosen.addClass('RegionSelected');

	// ORCA clearing every button and then setting the chosen one wrote the state twice
	// on that button, and a screen reader read "checked / checked" - SetAriaState
	// (a11y.js) writes only on a real change
	SetAriaState($Rest, 'aria-checked', 'false');
	SetAriaState($Chosen, 'aria-checked', 'true');

	// the group is a single tab stop, and it is the checked region the Tab lands on
	$Rest.attr('tabindex', '-1');
	$Chosen.attr('tabindex', '0');
}

function GotoPolicyPage()
{
	let ItemSelected=$('.RegionSelected')[0];
	let RegionFinal=$(ItemSelected).attr("region");
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="save_region";
	tSend['region']=RegionFinal;
	
	SendWXMessage( JSON.stringify(tSend) );
	
	window.location.href="../21/index.html";
}


