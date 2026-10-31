//var TestData={"sequence_id":"0","command":"get_recent_projects","response":[{"path":"D:\\work\\Models\\Toy\\3d-puzzle-cube-model_files\\3d-puzzle-cube.3mf","time":"2022\/3\/24 20:33:10"},{"path":"D:\\work\\Models\\Art\\Carved Stone Vase - remeshed+drainage\\Carved Stone Vase.3mf","time":"2022\/3\/24 17:11:51"},{"path":"D:\\work\\Models\\Art\\Kity & Cat\\Cat.3mf","time":"2022\/3\/24 17:07:55"},{"path":"D:\\work\\Models\\Toy\\鐩村墤.3mf","time":"2022\/3\/24 17:06:02"},{"path":"D:\\work\\Models\\Toy\\minimalistic-dual-tone-whistle-model_files\\minimalistic-dual-tone-whistle.3mf","time":"2022\/3\/22 21:12:22"},{"path":"D:\\work\\Models\\Toy\\spiral-city-model_files\\spiral-city.3mf","time":"2022\/3\/22 18:58:37"},{"path":"D:\\work\\Models\\Toy\\impossible-dovetail-puzzle-box-model_files\\impossible-dovetail-puzzle-box.3mf","time":"2022\/3\/22 20:08:40"}]};

var m_HotModelList=null;

function OnInit()
{
	//-----Official-----
    TranslatePage();
    let pluginTip = document.getElementById("NoPluginText");
    if (pluginTip)
        AppendTemplate(pluginTip, T("plugin_not_detected", "Bambu Network plug-in not detected. Click <a>here</a> to install it."),
                       pluginTip.querySelector(".LinkBtn"));

	Init_RecentFile_Events();

	SendMsg_GetLoginInfo();
	SendMsg_GetBambuLoginInfo();
	SendMsg_GetRecentFile();
	SendMsg_GetStaffPick();

	Set_AccountMenu_Event();
}

//------最佳打开文件的右键菜单功能----------
var RightBtnFilePath='';

var MousePosX=0;
var MousePosY=0;
var sImages = {};
 
// Bound once: the list is re-rendered on every push from the app.
// Delegated from the document: a listener on the <ul> exposes the list as clickable.
// On click, not mousedown, so Enter/Space on the buttons act too.
function Init_RecentFile_Events()
{
	$(document).on("click", ".FileItem", function () {
		OnOpenRecentFile( encodeURI($(this).attr('fpath')) );
	});

	// ORCA file actions; the innermost handler runs first, so returning false keeps the file closed
	$(document).on("click", ".FileActions", function () {
		return false; // the empty area between the buttons
	});
	$(document).on("click", ".FileActionsReveal", function () {
		RightBtnFilePath=$(this).attr('fpath');
		if(RightBtnFilePath != "")
			OnExploreRecentFile();
		return false;
	});
	$(document).on("click", ".FileActionsRemove", function () {
		RightBtnFilePath=$(this).attr('fpath');
		OnDeleteRecentFile();
		return false;
	});
}

function SetLoginPanelVisibility(visible) {
  var leftBoard = document.getElementById("LeftBoard");
  leftBoard.style.display = "block";
}

function HandleStudio( pVal )
{
	let strCmd = pVal['command'];
	
	if (strCmd == "get_recent_projects") {
    ShowRecentFileList(pVal["response"]);
  } else if (strCmd == "orca_userlogin") {
    SetOrcaLoginInfo(pVal["data"]["avatar"], pVal["data"]["name"], pVal["data"]["account"]);
  } else if (strCmd == "orca_useroffline") {
    SetOrcaUserOffline();
  } else if (strCmd == "studio_bambu_userlogin") {
    SetBambuLoginInfo(pVal["data"]["avatar"], pVal["data"]["name"]);
  } else if (strCmd == "studio_bambu_useroffline") {
    SetBambuUserOffline();
  } else if (strCmd == "studio_set_mallurl") {
    SetMallUrl(pVal["data"]["url"]);
  } else if (strCmd == "studio_clickmenu") {
    let strName = pVal["data"]["menu"];

    GotoMenu(strName);
  } else if (strCmd == "cloud_providers_info") {
    var providers = (pVal["data"] && pVal["data"]["providers"]) || [];

    if (providers.indexOf("bbl") >= 0) {
      $("#BambuCloudSection").show();
    } else {
      SetBambuUserOffline();
      $("#BambuCloudSection").hide();
    }

    if (providers.indexOf("orca") >= 0) {
      $("#LeftBoard").show();
    } else {
      $("#LeftBoard").hide();
    }
  } else if (strCmd == "network_plugin_installtip") {
    // Bambu Cloud is unreachable without the network plugin, so its row only carries the tip.
    let bMissing = pVal["show"] * 1 == 1;

    $("#NoPluginTip").css("display", bMissing ? "block" : "none");
    $("#BambuAccount").toggleClass("Disabled", bMissing).attr("aria-disabled", bMissing ? "true" : null);
    if (bMissing) SetBambuSectionExpanded(true, false);
  } else if (strCmd == "modelmall_model_advise_get") {
    //alert('hot');
    if (m_HotModelList != null) {
      let SS1 = JSON.stringify(pVal["hits"]);
      let SS2 = JSON.stringify(m_HotModelList);

      if (SS1 == SS2) return;
    }

    m_HotModelList = pVal["hits"];
    ShowStaffPick(m_HotModelList);
  } else if (strCmd == "SetLoginPanelVisibility") {
    SetLoginPanelVisibility(pVal["data"]["visible"]);
  }
}

/*---- OrcaCloud opens as a native pane beside this page, so the shell only switches its
      own cloud-mode state and asks the app to show or hide that pane (WebViewPanel::set_cloud_mode). ----*/
var m_CloudMode=false;

function SetCloudMode( bOn )
{
	if( m_CloudMode==bOn )
		return;

	m_CloudMode=bOn;
	$("body").toggleClass("cloud-mode", bOn);

	if( bOn )
	{
		$(".BtnItem").removeClass("BtnItemSelected");
		$(".BtnItem[menu='orca_cloud']").addClass("BtnItemSelected");
		// leaving it, GotoMenu presses the menu button it switches to
		SetAriaState($(".BtnItem[menu]").not("[menu='orca_cloud']"), "aria-pressed", false);
		SetAriaState($(".BtnItem[menu='orca_cloud']"), "aria-pressed", true);
	}
}

function GotoCloud()
{
	SetCloudMode(true);
	SendSimpleCommand("homepage_open_cloud");
}

function GotoMenu( strMenu )
{
	/*---- Any board in the right region means leaving the cloud pane. ----*/
	if( m_CloudMode )
	{
		SetCloudMode(false);
		SendSimpleCommand("homepage_close_cloud");
	}

	let MenuList=$(".BtnItem");
	let nAll=MenuList.length;
	
	for(let n=0;n<nAll;n++)
	{
		let OneBtn=MenuList[n];
		
		if( $(OneBtn).attr("menu")==strMenu )
		{
			$(".BtnItem").removeClass("BtnItemSelected");
			$(OneBtn).addClass("BtnItemSelected");

			SetAriaState($(".BtnItem[menu]").not(OneBtn), "aria-pressed", false);
			SetAriaState(OneBtn, "aria-pressed", true);

			$("[board]").hide();   // #ContentBoard is the <main>
			$("[board=\'"+strMenu+"\']").show();
		}
	}
}

/*------Account rows------*/

/*----Everything that differs between the two cloud providers lives here----*/
var AccountRows = {
  orca:  { row: "#OrcaAccount",  name: "#UserName",      avatar: "#UserAvatarIcon",  dot: null,
           loginCmd: "homepage_orca_login_or_register",  logoutCmd: "homepage_orca_logout" },
  bambu: { row: "#BambuAccount", name: "#BambuUserName", avatar: "#BambuAvatarIcon", dot: "#BambuStatusDot",
           loginCmd: "homepage_bambu_login_or_register", logoutCmd: "homepage_bambu_logout" }
};

var BAMBU_FOLD_KEY = "OrcaHome_BambuCloudExpanded";

var m_OpenAccountMenu = null;
var m_AccountMenu = null;

function SetAccountAvatar(selector, strAvatar) {
  if (strAvatar != null && strAvatar.trim() !== '') {
    if ($(selector).attr("src") !== strAvatar) $(selector).attr("src", strAvatar);
  } else {
    $(selector).removeAttr("src");
  }
}

function OnAvatarLoadError(img) {
  img.removeAttribute("src");  // fall back to the placeholder glyph
}

function SetAccountSignedIn(strProvider, strAvatar, strName, strHandle) {
  var account = AccountRows[strProvider];

  $(account.name).text(strName).attr("title", strName);  /*----long names are ellipsized----*/
  SetAccountAvatar(account.avatar, strAvatar);
  /*----the handle is the one thing the row does not already show----*/
  $(account.row).addClass("SignedIn").data("handle", strHandle !== strName ? strHandle : null);
  /*----signed in, the row opens the account menu; signed out, it opens the login dialog----*/
  SetAriaState($(account.row), "aria-haspopup", true);
  SetAriaState($(account.row), "aria-expanded", m_OpenAccountMenu === strProvider);
  $(account.dot).addClass("Online");

  if (m_OpenAccountMenu === strProvider) OpenAccountMenu(strProvider);
}

function SetAccountSignedOut(strProvider) {
  var account = AccountRows[strProvider];

  if (m_OpenAccountMenu === strProvider) CloseAccountMenu();
  $(account.name).text('').removeAttr("title");
  SetAccountAvatar(account.avatar, null);
  $(account.row).removeClass("SignedIn").removeData("handle").removeAttr("aria-haspopup aria-expanded");
  $(account.dot).removeClass("Online");
}

function OnAccountRowClick(strProvider) {
  var account = AccountRows[strProvider];
  var row = $(account.row);
  if (row.hasClass("Disabled")) return;

  if (!row.hasClass("SignedIn")) {
    CloseAccountMenu();
    SendSimpleCommand(account.loginCmd);
  } else if (m_OpenAccountMenu === strProvider) {
    CloseAccountMenu();
  } else {
    OpenAccountMenu(strProvider);
  }
}

function OpenAccountMenu(strProvider) {
  var account = AccountRows[strProvider];
  var row = $(account.row);
  var top = row[0].offsetTop + row[0].offsetHeight + 6;  /*----read the geometry before writing----*/
  var strHandle = row.data("handle") || "";

  if (m_AccountMenu) m_AccountMenu.close(false);  /*----before the writes below, which its hide() would undo----*/
  $("#AccountMenuHandle").text(strHandle).css("display", strHandle === "" ? "none" : "block");
  $("#AccountMenu").css("top", top + "px").addClass("Open");
  row.addClass("MenuOpen");
  SetAriaState(row[0], "aria-expanded", true);
  m_OpenAccountMenu = strProvider;
  m_AccountMenu = A11yMenu($("#AccountMenu")[0], row[0], HideAccountMenu);
}

function HideAccountMenu() {
  $("#AccountMenu").removeClass("Open");
  $(".AccountRow").removeClass("MenuOpen");
  SetAriaState($(".AccountRow.SignedIn"), "aria-expanded", false);
  m_OpenAccountMenu = null;
}

function CloseAccountMenu() {
  if (m_AccountMenu) m_AccountMenu.close();
}

function OnAccountMenuLogout() {
  var account = AccountRows[m_OpenAccountMenu];
  CloseAccountMenu();
  if (account) SendSimpleCommand(account.logoutCmd);
}

/*----Bambu Cloud is secondary, so its account folds away under the main one----*/
function SetBambuSectionExpanded(bExpanded, bPersist) {
  $("#BambuCloudBody").toggleClass("Expanded", bExpanded).prop("inert", !bExpanded);  /*----inert keeps the fold animation----*/
  $("#BambuCloudHeader").toggleClass("Expanded", bExpanded).attr("aria-expanded", bExpanded ? "true" : "false");
  if (!bExpanded && m_OpenAccountMenu === "bambu") CloseAccountMenu();
  // Best effort: the fold is a per-machine convenience, not a synced preference.
  if (bPersist) { try { localStorage.setItem(BAMBU_FOLD_KEY, bExpanded ? "1" : "0"); } catch (e) {} }
}

function ToggleBambuSection() {
  SetBambuSectionExpanded(!$("#BambuCloudBody").hasClass("Expanded"), true);
}

function Set_AccountMenu_Event() {
  var bExpanded = false;
  try { bExpanded = localStorage.getItem(BAMBU_FOLD_KEY) === "1"; } catch (e) {}
  SetBambuSectionExpanded(bExpanded, false);
}

function SetMallUrl( strUrl )
{
	$("#MallWeb").prop("src",strUrl);
}


function ShowRecentFileList( pList )
{
	let nTotal=pList.length;
	
	let strHtml='';
	for(let n=0;n<nTotal;n++)
	{
		let OneFile=pList[n];
		
		let sPath=OneFile['path'];
		let sImg=OneFile["image"] || sImages[sPath];
		let sTime=OneFile['time'];
		let sName=OneFile['project_name'];
		let sPublished=OneFile['published'] == '1';
		sImages[sPath] = sImg;

		// a model file or an older project has no project_name; the button needs a name
		if(!sName)
		{
			let nCut=Math.max(sPath.lastIndexOf('\\'),sPath.lastIndexOf('/'));
			sName=sPath.substring(nCut+1);
		}

		let isExist  = !isNaN(sTime[0]); // its a valid file with time stamp
		let btnStyle = isExist ? "ButtonStyleConfirm" : "ButtonStyleDisabled";
		let btnPath  = isExist ? sPath : ""; // blank path will disable btn event

		// the "PUB" badge is spoken as the full word, inside the name button
		let sBadge=sPublished? '<span class="FilePublishedBadge" aria-hidden="true">'+T("published_badge", "PUB")+'</span>':'';
		let sPubText=sPublished? '<span class="A11yOnly"> '+EscAttr(T("published","Published"))+'</span>':'';
		let sLogoBadge=sPublished? '<img class="FileLogoBadge" src="../../images/OrcaSlicer_gradient_circle.svg" alt="" />':'';

		// the clipped path reads between the name and the date
		let TmpHtml='<li class="FileItem" fpath="'+EscAttr(sPath)+'">'+
						'<a class="FileTip" title="'+EscAttr(sPath)+'" aria-hidden="true"></a>'+
						'<div class="FileImg"><img src="'+EscAttr(sImg)+'" onerror="this.onerror=null;this.src=\'img/d.png\';" alt="" />'+sLogoBadge+'</div>'+
						'<div class="FileNamePack">'+sBadge+'<button type="button" class="FileName TextS1 FocusRingInset" aria-describedby="RecentPath'+n+' RecentDate'+n+'">'+EscAttr(sName)+sPubText+'</button></div>'+
						'<span class="A11yOnly" id="RecentPath'+n+'">'+EscAttr(sPath)+'</span>'+
						'<div class="FileDate" id="RecentDate'+n+'">'+EscAttr(sTime)+'</div>'+
						'<div class="FileActions">'+
							'<button type="button" class="FileActionsReveal '+btnStyle+' ButtonTypeWindow trans" fpath="'+EscAttr(btnPath)+'"'+(isExist ? '' : ' disabled')+' data-i18n="show_in_folder">Show in folder</button>'+
							'<button type="button" class="FileActionsRemove ButtonStyleAlert ButtonTypeWindow" fpath="'+EscAttr(sPath)+'" aria-label="Remove" data-i18n-aria-label="remove">'+
								'<span class="icon16"></span>'+
							'</button>'+
						'</div>'+
			    	'</li>';

		strHtml+=TmpHtml;
	}

	$("#FileList").html(strHtml);
	TranslatePage();   // translate the data-i18n attributes on the freshly inserted cards

	UpdateRecentClearBtnDisplay();
}
/*
function ShowRecnetFileContextMenu()
{
	$("#recnet_context_menu").offset({top: 10000, left:-10000});
	$('#recnet_context_menu').show();
	
	let ContextMenuWidth=$('#recnet_context_menu').width();
	let ContextMenuHeight=$('#recnet_context_menu').height();
	
    let DocumentWidth=$(document).width();
	let DocumentHeight=$(document).height();

	let RealX=MousePosX;
	let RealY=MousePosY;
	
	if( MousePosX + ContextMenuWidth + 24 >DocumentWidth )
		RealX=DocumentWidth-ContextMenuWidth-24;
	if( MousePosY+ContextMenuHeight+24>DocumentHeight )
		RealY=DocumentHeight-ContextMenuHeight-24;
	
	$("#recnet_context_menu").offset({top: RealY, left:RealX});
}
*/
/*-------RecentFile MX Message------*/
function SendMsg_GetLoginInfo()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="get_login_info";
	
	SendWXMessage( JSON.stringify(tSend) );	
}

function SendSimpleCommand(command) {
  var tSend = {};
  tSend['sequence_id'] = Math.round(new Date() / 1000);
  tSend['command'] = command;
  SendWXMessage(JSON.stringify(tSend));
}

function SendMsg_GetOrcaLoginInfo() { SendSimpleCommand("get_orca_login_info"); }


function SendMsg_GetRecentFile()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="get_recent_projects";
	
	SendWXMessage( JSON.stringify(tSend) );
}


function OnClickModelDepot()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="homepage_modeldepot";
	
	SendWXMessage( JSON.stringify(tSend) );		
}

function OnClickNewProject()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="homepage_newproject";
	
	SendWXMessage( JSON.stringify(tSend) );		
}

function OnClickOpenProject()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="homepage_openproject";
	
	SendWXMessage( JSON.stringify(tSend) );		
}

function OnOpenRecentFile( strPath )
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="homepage_open_recentfile";
	tSend['data']={};
	tSend['data']['path']=decodeURI(strPath);
	
	SendWXMessage( JSON.stringify(tSend) );	
}

function OnDeleteRecentFile( )
{
	//Clear in UI
	//$("#recnet_context_menu").hide();

	let AllFile=$(".FileItem");
	let nFile=AllFile.length;
	let pFocus=null;
	for(let p=0;p<nFile;p++)
	{
		let pp=AllFile[p].getAttribute("fpath");
		if(pp==RightBtnFilePath)
		{
			pFocus=AllFile[p+1] || AllFile[p-1];
			$(AllFile[p]).remove();
		}
	}

	UpdateRecentClearBtnDisplay();

	// the next or previous file, else the Open Project card before the emptied list
	if (pFocus && pFocus.isConnected)
		$(pFocus).find(".FileName").focus();
	else
		$(".MenuItem").last().focus();

	//Send Msg to C++
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="homepage_delete_recentfile";
	tSend['data']={};
	tSend['data']['path']=RightBtnFilePath;
	
	SendWXMessage( JSON.stringify(tSend) );
}

function OnDeleteAllRecentFiles()
{
	$('#FileList').html('');
	UpdateRecentClearBtnDisplay();
	
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="homepage_delete_all_recentfile";
	
	SendWXMessage( JSON.stringify(tSend) );
}

function UpdateRecentClearBtnDisplay()
{
    let AllFile=$(".FileItem");
	let nFile=AllFile.length;	
	if( nFile>0 )
		$("#RecentClearAllBtn").show();
	else
		$("#RecentClearAllBtn").hide();
}




function OnExploreRecentFile( )
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="homepage_explore_recentfile";
	tSend['data']={};
	tSend['data']['path']=decodeURI(RightBtnFilePath);
	
	SendWXMessage( JSON.stringify(tSend) );	
	
	//$("#recnet_context_menu").hide();
}

// --- Cloud providers ---

function SetOrcaLoginInfo(strAvatar, strName, strAccount) { SetAccountSignedIn("orca", strAvatar, strName, strAccount); }
function SetOrcaUserOffline() { SetAccountSignedOut("orca"); }
function SetBambuLoginInfo(strAvatar, strName) { SetAccountSignedIn("bambu", strAvatar, strName, null); }
function SetBambuUserOffline() { SetAccountSignedOut("bambu"); }

function SendMsg_GetBambuLoginInfo() { SendSimpleCommand("get_bambu_login_info"); }

function BeginDownloadNetworkPlugin()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="begin_network_plugin_download";
	
	SendWXMessage( JSON.stringify(tSend) );		
}

function OutputKey(keyCode, isCtrlDown, isShiftDown, isCmdDown) {
	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "get_web_shortcut";
	tSend['key_event'] = {};
	tSend['key_event']['key'] = keyCode;
	tSend['key_event']['ctrl'] = isCtrlDown;
	tSend['key_event']['shift'] = isShiftDown;
	tSend['key_event']['cmd'] = isCmdDown;

	SendWXMessage(JSON.stringify(tSend));
}

//-------------User Manual------------

function OpenWikiUrl( strUrl )
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="userguide_wiki_open";
	tSend['data']={};
	tSend['data']['url']=strUrl;
	
	SendWXMessage( JSON.stringify(tSend) );	
}

//--------------Staff Pick-------
var StaffPickSwiper=null;
function InitStaffPick()
{
	if( StaffPickSwiper!=null )
	{
		StaffPickSwiper.destroy(true,true);
		StaffPickSwiper=null;
	}	
	
	StaffPickSwiper = new Swiper('#HotModel_Swiper.swiper', {
            slidesPerView : 'auto',
		    spaceBetween: 16,
			navigation: {
				nextEl: '.swiper-button-next',
				prevEl: '.swiper-button-prev',
			},
		    slidesPerView : 'auto',
		    slidesPerGroup : 3

			});
}

function SendMsg_GetStaffPick()
{
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="modelmall_model_advise_get";
	
	SendWXMessage( JSON.stringify(tSend) );
	
	setTimeout("SendMsg_GetStaffPick()",3600*1000*1);
}

function ShowStaffPick( ModelList )
{
	let PickTotal=ModelList.length;
	if(PickTotal==0)
	{
		$('#HotModelList').html('');
		$('#HotModelArea').hide();
		
		return;
	}
	
	let strPickHtml='';
	for(let a=0;a<PickTotal;a++)
	{
		let OnePickModel=ModelList[a];
		
		let ModelID=OnePickModel['design']['id'];
		let ModelName=OnePickModel['design']['title'];
		let ModelCover=OnePickModel['design']['cover']+'?image_process=resize,w_200/format,webp';
		
		let DesignerName=OnePickModel['design']['designCreator']['name'];
		let DesignerAvatar=OnePickModel['design']['designCreator']['avatar']+'?image_process=resize,w_32/format,webp';
		
		strPickHtml+='<div class="HotModelPiece swiper-slide"  onClick="OpenOneStaffPickModel('+ModelID+')" >'+
			    '<div class="HotModel_Designer_Info"><img src="'+DesignerAvatar+'" /><span class="TextS2">'+DesignerName+'</span></div>'+
				'	<div class="HotModel_PrevBlock"><img class="HotModel_PrevImg" src="'+ModelCover+'" /></div>'+
				'	<div  class="HotModel_NameText TextS1" title="'+ModelName+'">'+ModelName+'</div>'+
				'</div>';
	}
	
	$('#HotModelList').html(strPickHtml);
	InitStaffPick();
	$('#HotModelArea').show();
}

function OpenOneStaffPickModel( ModelID )
{
	//alert(ModelID);
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="modelmall_model_open";
	tSend['data']={};
	tSend['data']['id']=ModelID;
	
	SendWXMessage( JSON.stringify(tSend) );		
}


//---------------Global-----------------
window.postMessage = HandleStudio;

