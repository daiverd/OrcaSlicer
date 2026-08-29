//var TestData={"sequence_id":"0","command":"get_recent_projects","response":[{"path":"D:\\work\\Models\\Toy\\3d-puzzle-cube-model_files\\3d-puzzle-cube.3mf","time":"2022\/3\/24 20:33:10"},{"path":"D:\\work\\Models\\Art\\Carved Stone Vase - remeshed+drainage\\Carved Stone Vase.3mf","time":"2022\/3\/24 17:11:51"},{"path":"D:\\work\\Models\\Art\\Kity & Cat\\Cat.3mf","time":"2022\/3\/24 17:07:55"},{"path":"D:\\work\\Models\\Toy\\鐩村墤.3mf","time":"2022\/3\/24 17:06:02"},{"path":"D:\\work\\Models\\Toy\\minimalistic-dual-tone-whistle-model_files\\minimalistic-dual-tone-whistle.3mf","time":"2022\/3\/22 21:12:22"},{"path":"D:\\work\\Models\\Toy\\spiral-city-model_files\\spiral-city.3mf","time":"2022\/3\/22 18:58:37"},{"path":"D:\\work\\Models\\Toy\\impossible-dovetail-puzzle-box-model_files\\impossible-dovetail-puzzle-box.3mf","time":"2022\/3\/22 20:08:40"}]};

var m_HotModelList=null;

function OnInit()
{
	//-----Official-----
    TranslatePage();

	Init_RecentFile_Events();
	Init_StaffPick_Events();

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
var ContextMenuOpener=null;   // file item the menu was opened from, when opened by keyboard
var sImages = {};
 
function Set_RecentFile_MouseRightBtn_Event()
{
	$(".FileItem").mousedown(
		function(e)
		{			
			//FilePath
			RightBtnFilePath=$(this).attr('fpath');
			
			if(e.which == 3){
				//鼠标点击了右键+$(this).attr('ff') );
				ShowRecnetFileContextMenu();
			}
		});
}

// ORCA: everything below is bound once - the list is re-rendered on every push from
// the app, so these handlers are delegated or document-level rather than per item
function Init_RecentFile_Events()
{
	// ORCA: opening on click rather than mousedown keeps the keyboard route working -
	// Enter/Space on the filename <button> fires a native click that bubbles here.
	// Delegated from the document rather than from #FileList: a click listener on the
	// <ul> makes Chromium expose the list itself as clickable, and a screen reader
	// announced "list clickable" on every pass over the recent files.
	$(document).on("click", ".FileItem", function () {
		OnOpenRecentFile( encodeURI($(this).attr('fpath')) );
	});

	// ORCA: the context menu keys are the only ones the item handles itself
	$("#FileList").on("keydown", ".FileItem", function (event) {
		if (event.key === 'ContextMenu' || (event.shiftKey && event.key === 'F10')) {
			event.preventDefault();
			RightBtnFilePath=$(this).attr('fpath');
			OpenRecnetFileContextMenuAt(this);
		}
	});

	// ORCA: Escape closes the keyboard-opened menu and hands focus back to the file
	$(document).on("keydown", function (event) {
		if (event.key !== 'Escape' || !$("#recnet_context_menu").is(":visible"))
			return;

		CloseRecnetFileContextMenu();
	});

	// ORCA: the menu keyboard pattern - the arrows and Home/End walk the items, Tab leaves.
	// Enter and Space come from a11y.js, which clicks whatever menuitem holds the focus.
	$("#recnet_context_menu").on("keydown", "[role='menuitem']", function (event) {
		let Items=$("#recnet_context_menu [role='menuitem']");
		let nCount=Items.length;
		let nNext=Items.index(this);

		if (event.key === 'ArrowDown')
			nNext=(nNext+1)%nCount;
		else if (event.key === 'ArrowUp')
			nNext=(nNext+nCount-1)%nCount;
		else if (event.key === 'Home')
			nNext=0;
		else if (event.key === 'End')
			nNext=nCount-1;
		else if (event.key === 'Tab') {
			// leaving the menu closes it - the focus goes back to the file first so the
			// browser's own move carries on from there and not from the top of the page
			CloseRecnetFileContextMenu();
			return;
		}
		else
			return;

		event.preventDefault();
		Items.eq(nNext).focus();
	});

	$(document).bind("contextmenu",function(e){
		//在这里书写代码，构建个性右键化菜单
		return false;
	});	
	
    $(document).mousemove( function(e){
		MousePosX=e.pageX;
		MousePosY=e.pageY;
		
		let ContextMenuWidth=$('#recnet_context_menu').width();
		let ContextMenuHeight=$('#recnet_context_menu').height();
	
		let DocumentWidth=$(document).width();
		let DocumentHeight=$(document).height();
		
		//$("#DebugText").text( ContextMenuWidth+' - '+ContextMenuHeight+'<br/>'+
		//					 DocumentWidth+' - '+DocumentHeight+'<br/>'+
		//					 MousePosX+' - '+MousePosY +'<br/>' );
	} );
	

	$(document).click( function(){		
		var e = e || window.event;
        var elem = e.target || e.srcElement;
        while (elem) {
			if (elem.id && elem.id == 'recnet_context_menu') {
                    return;
			}
			elem = elem.parentNode;
		}		
		
		CloseRecnetFileContextMenu(false);   // the click itself decides where the focus goes
	} );

	
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

function GotoMenu( strMenu )
{
	let MenuList=$(".BtnItem");
	let nAll=MenuList.length;
	
	for(let n=0;n<nAll;n++)
	{
		let OneBtn=MenuList[n];
		
		if( $(OneBtn).attr("menu")==strMenu )
		{
			$(".BtnItem[menu]").removeClass("BtnItemSelected");
			$(OneBtn).addClass("BtnItemSelected");

			// ORCA: clearing every button and then setting the target wrote aria-pressed
			// twice on the target itself, and the app confirms the default menu with a
			// studio_clickmenu on load - hence "Recent toggle button pressed" twice in the
			// load announcement. SetAriaState (a11y.js) writes only on a real change.
			SetAriaState($(".BtnItem[menu]").not(OneBtn), "aria-pressed", "false");
			SetAriaState(OneBtn, "aria-pressed", "true");

			$("[board]").hide();   // the boards are not all <div> - #ContentBoard is the <main>
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
  $(account.dot).addClass("Online");

  if (m_OpenAccountMenu === strProvider) OpenAccountMenu(strProvider);
}

function SetAccountSignedOut(strProvider) {
  var account = AccountRows[strProvider];

  if (m_OpenAccountMenu === strProvider) CloseAccountMenu();
  $(account.name).text('').removeAttr("title");
  SetAccountAvatar(account.avatar, null);
  $(account.row).removeClass("SignedIn").removeData("handle");
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

  $("#AccountMenuHandle").text(strHandle).css("display", strHandle === "" ? "none" : "block");
  $("#AccountMenu").css("top", top + "px").addClass("Open");
  $(".AccountRow").removeClass("MenuOpen").attr("aria-expanded", "false");
  row.addClass("MenuOpen").attr("aria-expanded", "true");
  m_OpenAccountMenu = strProvider;
}

function CloseAccountMenu() {
  $("#AccountMenu").removeClass("Open");
  $(".AccountRow").removeClass("MenuOpen").attr("aria-expanded", "false");
  m_OpenAccountMenu = null;
}

function OnAccountMenuLogout() {
  var account = AccountRows[m_OpenAccountMenu];
  CloseAccountMenu();
  if (account) SendSimpleCommand(account.logoutCmd);
}

/*----Bambu Cloud is secondary, so its account folds away under the main one----*/
function SetBambuSectionExpanded(bExpanded, bPersist) {
  $("#BambuCloudBody").toggleClass("Expanded", bExpanded);
  $("#BambuAccount").attr("tabindex", bExpanded ? "0" : "-1");  /*----keep the folded row out of the tab order----*/
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

  $(document).mousedown(function (e) {
    if (m_OpenAccountMenu === null) return;
    if ($(e.target).closest("#AccountMenu, .AccountRow").length === 0) CloseAccountMenu();
  });

  $(document).keydown(function (e) {
    if (m_OpenAccountMenu !== null && e.key === "Escape") CloseAccountMenu();
  });

  /*----Enter and Space activate the div-based buttons of this panel----*/
  $(document).on("keydown", "#LoginArea [role=button], #LoginArea [role=menuitem]", function (e) {
    if (e.key !== "Enter" && e.key !== " " && e.key !== "Spacebar") return;
    e.preventDefault();
    this.click();
  });
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

		// ORCA: an entry can arrive with no project_name - a model file opened directly, or
		// a project saved before the field existed. The button text is the row's only name,
		// so fall back to the file name from the path: those rows were blank on screen and
		// read as "blank" lines in browse mode, with nothing to say which file they open.
		if(!sName)
		{
			let nCut=Math.max(sPath.lastIndexOf('\\'),sPath.lastIndexOf('/'));
			sName=sPath.substring(nCut+1);
		}

		let sBadge=sPublished? '<span class="FilePublishedBadge">PUB</span>':'';
		let sLogoBadge=sPublished? '<img class="FileLogoBadge" src="../../images/OrcaSlicer_gradient_circle.svg" alt="" />':'';

		// ORCA: only the filename is focusable - a real <button>, so its accessible name
		// stays the filename alone instead of every text block in the card. The clipped
		// A11yOnly copy of the path sits between the name and the date so browse mode
		// reads name, path, date; aria-describedby repeats both for focus (Tab) mode.
		// The tooltip-only <a> is aria-hidden: its title would otherwise surface it as
		// a nameless line covering the whole card.
		let TmpHtml='<li class="FileItem" fpath="'+EscAttr(sPath)+'">'+
				'<a class="FileTip" title="'+EscAttr(sPath)+'" aria-hidden="true"></a>'+
				'<div class="FileImg"><img src="'+EscAttr(String(sImg))+'" onerror="this.onerror=null;this.src=\'img/d.png\';" alt="" />'+sLogoBadge+'</div>'+
				'<div class="FileNamePack">'+sBadge+'<button type="button" class="FileName TextS1 FocusRingInset" aria-describedby="RecentPath'+n+' RecentDate'+n+'">'+EscAttr(sName)+'</button></div>'+
				'<span class="A11yOnly" id="RecentPath'+n+'">'+EscAttr(sPath)+'</span>'+
				'<div class="FileDate" id="RecentDate'+n+'">'+EscAttr(sTime)+'</div>'+
			    '</li>';
		
		strHtml+=TmpHtml;
	}
	
	$("#FileList").html(strHtml);	
	
    Set_RecentFile_MouseRightBtn_Event();
	UpdateRecentClearBtnDisplay();
}

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

// ORCA: the keyboard close paths - Escape and Tab - both hand the focus back to the file
// the menu was opened from, since the menu itself is about to disappear from under it.
// The paths that pass false have somewhere better for the focus to be, or nowhere at all.
function CloseRecnetFileContextMenu( bRestoreFocus )
{
	$("#recnet_context_menu").hide();

	if (ContextMenuOpener && bRestoreFocus !== false)
		$(ContextMenuOpener).focus();

	ContextMenuOpener=null;
}

// ORCA: keyboard route to the same menu - anchor it to the item instead of the pointer
function OpenRecnetFileContextMenuAt( pItem )
{
	let Box=pItem.getBoundingClientRect();
	MousePosX=Box.left+$(window).scrollLeft();
	MousePosY=Box.bottom+$(window).scrollTop();

	ShowRecnetFileContextMenu();

	// the item itself is not focusable - the focus comes back to its filename button
	ContextMenuOpener=$(pItem).find(".FileName")[0];
	$("#recnet_context_menu [role='menuitem']:first").focus();
}

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
	CloseRecnetFileContextMenu(false);   // the opener item is about to be removed

	let AllFile=$(".FileItem");
	let nFile=AllFile.length;
	for(let p=0;p<nFile;p++)
	{
		let pp=AllFile[p].getAttribute("fpath");
		if(pp==RightBtnFilePath)
			$(AllFile[p]).remove();
	}	
	
	UpdateRecentClearBtnDisplay();
	
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

	CloseRecnetFileContextMenu();
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

// ORCA: the whole card stays the mouse target, delegated from the document like the
// recent-file cards - a click handler on the card itself would put it back in the
// accessibility tree as a clickable element wrapping its own button. The title
// button has no handler of its own: its activation, mouse or Enter/Space, bubbles here.
function Init_StaffPick_Events()
{
	$(document).on("click", ".HotModelPiece", function () {
		OpenOneStaffPickModel( $(this).attr("modelid") );
	});
}

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
		
		// ORCA: the same rework the recent-file cards got - the card was one role="button"
		// with three block children, which a screen reader read as three "button" lines,
		// and its accessible name ran the designer, the cover and the title together.
		// Only the model title is interactive now, as a real <button>, so the name is the
		// title alone; the designer line stays a plain line of the card and is handed to
		// Tab users with aria-describedby. The card keeps the mouse click through the
		// delegated handler in Init_StaffPick_Events. The slides make up a list, so the
		// wrapper is the list and each card an item.
		strPickHtml+='<div class="HotModelPiece swiper-slide" role="listitem" modelid="'+EscAttr(String(ModelID))+'">'+
			    '<div class="HotModel_Designer_Info" id="HotModelBy'+a+'"><img src="'+EscAttr(String(DesignerAvatar))+'" alt="" /><span class="TextS2">'+EscAttr(DesignerName)+'</span></div>'+
				'	<div class="HotModel_PrevBlock"><img class="HotModel_PrevImg" src="'+EscAttr(String(ModelCover))+'" alt="" /></div>'+
				'	<button type="button" class="HotModel_NameText TextS1 FocusRingInset" title="'+EscAttr(ModelName)+'" aria-describedby="HotModelBy'+a+'">'+EscAttr(ModelName)+'</button>'+
				'</div>';
	}

	$('#HotModelList').attr('role','list').html(strPickHtml);
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

