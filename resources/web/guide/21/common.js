var	pModel              = {};
var	ModelNozzleSelected = {};
let SearchBox;
let $content;

function InitGlobalVariables()
{
	SearchBox = document.querySelector('.searchTerm');
	$content  = $('#Content');
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
//	alert(strInput);
//	alert(JSON.stringify(strInput));
//	
//	let pVal=IsJson(strInput);
//	if(pVal==null)
//	{
//		alert("Msg Format Error is not Json");
//		return;
//	}
	
	let strCmd=pVal['command'];
	//alert(strCmd);
	
	if(strCmd=='response_userguide_profile')
	{
		HandleModelList(pVal['response']);
	}
	else if(strCmd=='check_new_printers_result')
	{
		// ORCA: aria-disabled (24.js) never took the button out of the tab order, so focus is
		// still on it here - nothing to restore, just clear the busy state.
		let button = document.getElementById("CheckNewPrintersBtn");
		if (button) {
			SetAriaState(button, "aria-disabled", false);
			button.style.opacity = "1";
		}

		let noticeBar = document.getElementById("NoticeBar");
		let noticeText = document.getElementById("NoticeText");
		let hasError = pVal.hasOwnProperty("error");
		noticeBar.classList.toggle("notice-error", hasError);
		if (hasError) {
			noticeBar.textContent = "Error";
			noticeText.textContent = pVal["error"];
		} else if (pVal["vendors"] && pVal["vendors"].length > 0) {
			noticeBar.textContent = "New printers found";
			noticeText.textContent = "New printer vendors installed: " + pVal["vendors"].join(", ");
		} else if (pVal["declined"]) {
			noticeBar.textContent = "Information";
			noticeText.textContent = "New printer vendors were found, but installation was cancelled.";
		} else {
			noticeBar.textContent = "Information";
			noticeText.textContent = "No new printers found.";
		}

		ShowNotice(1);
	}
}

function HandleModelList( pVal )
{
	if( !pVal.hasOwnProperty("model") )
		return;

	pModel=pVal['model'];

	// ORCA ensure list correctly ordered
	pModel = pModel.sort((a, b)=>(a["vendor"].localeCompare(b["vendor"])))
	pModel = [ // move custom printers to top
		...pModel.filter(i=>i.vendor === "Custom"),
		...pModel.filter(i=>i.vendor !== "Custom")
	];
	
	let nTotal=pModel.length;
	let ModelHtml={};
	for(let n=0;n<nTotal;n++)
	{
		let OneModel=pModel[n];
		
		let strVendor=OneModel['vendor'];
		
		//Add Vendor Html Node
		if($(".OneVendorBlock[vendor='"+strVendor+"']").length==0)
		{
			let HtmlNewVendor = CreateVendorBlock(strVendor);
			$('#Content').append(HtmlNewVendor);
		}
		
		let ModelName=OneModel['model'];
		
		//Collect Html Node Nozzel Html
		if( !ModelHtml.hasOwnProperty(strVendor))
			ModelHtml[strVendor]='';
			
		ModelHtml[strVendor] += CreatePrinterBlock(OneModel); // ORCA
	}
	
	//Update Nozzel Html Append
	// ORCA: HandleModelList can now be called more than once per dialog (e.g. after a new vendor
	// is installed via "check for new printers"). pModel always holds the full, current list, so
	// clear each vendor's printer area before repopulating instead of appending on top of a
	// previous render.
	for( let key in ModelHtml )
	{
		$(".OneVendorBlock[vendor='"+key+"'] .PrinterArea").empty().append( ModelHtml[key] );
	}
	
	//Update Checkbox
	for(let m=0;m<nTotal;m++)
	{
		let OneModel=pModel[m];

		let SelectList=OneModel['nozzle_selected'];
		if(SelectList!='') {
			ChooseModel(OneModel['vendor'], OneModel['model']);
		}
	}

	UpdateSidebarVendors();

	// let AlreadySelect=$(".ModelCheckBoxSelected");
	// let nSelect=AlreadySelect.length;
	// if(nSelect==0)
	// {
	//	$("div.OneVendorBlock[vendor='"+BBL+"'] .ModelCheckBox").addClass('ModelCheckBoxSelected');
	// }
	
	TranslatePage();
}

function SetModelSelect(vendor, model, checked) {
	if (!ModelNozzleSelected.hasOwnProperty(vendor) && !checked) {
		return;
	}

	if (!ModelNozzleSelected.hasOwnProperty(vendor) && checked) {
		ModelNozzleSelected[vendor] = {};
	}

	let oVendor = ModelNozzleSelected[vendor];
	if (oVendor.hasOwnProperty(model) || checked) {
		oVendor[model] = checked;
	}

	UpdateVendorCheckbox(vendor)
}

function GetModelSelect(vendor, model) {
	if (!ModelNozzleSelected.hasOwnProperty(vendor)) {
		return false;
	}

	let oVendor = ModelNozzleSelected[vendor];
	if (!oVendor.hasOwnProperty(model)) {
		return false;
	}

	return oVendor[model];
}

// ORCA the selection bar is the visual state, the card is the role="checkbox" - keep both in sync.
// ChooseVendor runs this over every card in the vendor, so the aria-checked write goes through
// SetAriaState (a11y.js): re-writing the value a card already carried made a screen reader
// announce the card the user had just clicked twice.
function SetModelChecked($cbs, checked)
{
	$cbs.toggleClass('ModelCheckBoxSelected', !!checked);
	SetAriaState($cbs.closest('.PrinterBlock'), 'aria-checked', !!checked);
}

function ChooseModel(vendor, ModelName)
{
	let ChooseItem=$(".ModelCheckBox[vendor='"+vendor+"'][model='"+ModelName+"']");

	if(ChooseItem.length > 0) {
		let checked = !ChooseItem.hasClass('ModelCheckBoxSelected');

		SetModelChecked(ChooseItem, checked);
		SetModelSelect(vendor, ModelName, checked);
	}
}

function FilterModelList(keyword) {

	//Save checkbox state
	let ModelSelect = $('.ModelCheckBox');
	for (let n = 0; n < ModelSelect.length; n++) {
		let OneItem = ModelSelect[n];

		let strModel = OneItem.getAttribute("model");

		let strVendor = OneItem.getAttribute("vendor");

		SetModelSelect(strVendor, strModel, $(OneItem).hasClass('ModelCheckBoxSelected'));
	}

	$('.search')[0].setAttribute("hasvalue", keyword ? "1" : "0")

	let nTotal = pModel.length;
	let ModelHtml = {};
	let kwSplit = keyword.toLowerCase().match(/\S+/g) || [];

	$('#Content').empty();
	for (let n = 0; n < nTotal; n++) {
		let OneModel = pModel[n];

		let strVendor = OneModel['vendor'];
		let search = (OneModel['name'] + '\0' + strVendor).toLowerCase();

		if (!kwSplit.every(s => search.includes(s)))
			continue;

		//Add Vendor Html Node
		if ($(".OneVendorBlock[vendor='" + strVendor + "']").length == 0) {
			let HtmlNewVendor = CreateVendorBlock(strVendor);
			$('#Content').append(HtmlNewVendor);
		}

		//Collect Html Node Nozzel Html
		if (!ModelHtml.hasOwnProperty(strVendor))
			ModelHtml[strVendor] = '';
			
		ModelHtml[strVendor] += CreatePrinterBlock(OneModel); // ORCA
	}

	//Update Nozzel Html Append
	for (let key in ModelHtml) {
		let obj = $(".OneVendorBlock[vendor='" + key + "'] .PrinterArea");
		obj.empty();
		obj.append(ModelHtml[key]);
	}

	UpdateSidebarVendors();

	$content.css("padding-right",  $content[0].scrollHeight > $content[0].clientHeight ? "10px" : "20px");

	// let AlreadySelect=$(".ModelCheckBoxSelected");
	// let nSelect=AlreadySelect.length;
	// if(nSelect==0)
	// {
	//	$("div.OneVendorBlock[vendor='"+BBL+"'] .ModelCheckBox").addClass('ModelCheckBoxSelected');
	// }

	// ORCA runs on every keystroke, so only the cards just rebuilt are re-translated -
	// the rest of the page is static chrome that the full TranslatePage on load handled
	TranslateNodes($content);
}

function textInput(obj) {
	FilterModelList(obj.value);
}

function CreateVendorBlock(vendorName)
{
	let alt = vendorName;
	if( alt == "BBL" )
		alt = "Bambu Lab";
	if( alt == "Custom")
		alt = "Custom Printer";
	if( alt == "Other")
		alt = "Orca colosseum";

	let altAttr = EscAttr(alt);
	let titleId = 'VendorTitle_' + vendorName.replace(/[^\w-]/g, '_');

	return 	`<div class="OneVendorBlock" Vendor="${vendorName}" role="group" aria-labelledby="${titleId}">` +
			'	<div class="BlockBanner">' +
			`		<h2 id="${titleId}">` + alt + '</h2>' +
			'		<label class="BannerBtns">'+
			// the "0 / 5" on its own read as "0 slash 5", two numbers with no unit. The
			// visible text is unchanged and the clipped twin beside it says what they count.
			'			<div class="modelCount" aria-hidden="true"></div>' +
			'			<span class="A11yOnly modelCountText"></span>' +
			// the row is the box's label, so the name belongs on the box - the label's own
			// text is the "3 / 12" count, which is state rather than a name
			`			<input type="checkbox" class="VendorCheckbox" aria-label="${GetTransText('a11y_select_all_printers','Select all {0} printers').replace('{0}',altAttr)}" onChange="ChooseVendor('${vendorName}')"/>`+
			'		</label>'+
			'	</div>' +
			'	<div class="PrinterArea">	' +
			'	</div>' +
			'</div>';
}

// the hover card needs an id for aria-describedby to point at, and the cards are rebuilt
// on every search - a running number is the cheapest id that is unique across rebuilds
let PrinterInfoSeq = 0;

function CreatePrinterBlock(OneModel)
{
	let vendor = OneModel['vendor']
	let vendorName = vendor=="BBL" ? "Bambu Lab" : vendor=="Custom" ? "Generic Printer" : vendor;
	let modelName  = OneModel['name'];

	// Most of it unneeded. this can be applied in profiles
	if( vendor=="Custom")					
		modelName = modelName.split(" ")[1];
	// these uses different case in name; seckit, ratrig, blocks
	// the strip leaves the separating space behind - trim it, or the aria-label built
	// from the name below starts with one ("Afinia H+1(HS)" reads as " H+1(HS)")
	else if (modelName.toLowerCase().startsWith(vendorName.toLowerCase()))  
		modelName = modelName.slice(vendorName.length).trim();
	// these not matches. have to fix in profiles to reduce conditions in here;
	else if (vendor == "MagicMaker" && modelName.startsWith("MM"))
		modelName = modelName.slice(("MM").length).trim();
	else if (vendor == "OrcaArena")
		modelName = modelName.slice(("Orca Arena").length).trim();
	else if (vendor == "RolohaunDesign" && modelName.startsWith("Rolohaun"))
		modelName = modelName.slice(("Rolohaun").length).trim();

	// the card is a multi-select toggle, so it is a checkbox; the model name is its explicit
	// accessible name so it is announced first, and the hover card the "?" mark reveals is
	// its description. The card stays aria-hidden - it is a pointer-only duplicate anywhere
	// else in the tree - but a node named by aria-describedby is read even when hidden.
	let nozzles = OneModel['nozzle_diameter'].replaceAll(";", " · ");
	let infoId  = 'PrinterInfo' + (++PrinterInfoSeq);
	// the selection bar and aria-checked are the visual and announced halves of one state
	// (SetModelChecked keeps them in sync later) - emitting both here saves the search
	// rebuild a second pass over every card it just created
	let checked = !!GetModelSelect(vendor, OneModel['model']);

	return	`<div class="PrinterBlock" tabindex="0" role="checkbox" aria-checked="${checked}" aria-label="${EscAttr(modelName)}" aria-describedby="${infoId}" onClick="ChooseModel('${vendor}','${OneModel['model']}')">`+
			'	<div class="PImg">'+
			'		<img class="ModelThumbnail" src="' + OneModel['cover'] + '" alt="" />'+
			'	</div>'+
			'	<div class="PrinterInfoMark" aria-hidden="true">?</div>'+
			`	<div class="PrinterInfo" id="${infoId}" aria-hidden="true">`+
			'		<div class="title trans">Nozzle</div>'+
			'		<div class="value">' + nozzles + '</div>'+
			'	</div>'+
			'	<div style="display: flex;">'+
			`		<div class="ModelCheckBox${checked ? ' ModelCheckBoxSelected' : ''}" vendor="${vendor}" model="${OneModel['model']}"></div>`+
			'		<div class="PName">'+ modelName +'</div>'+ // ><p>'+ vendorName +'</p>
			'	</div>'+
			'</div>';
}

function SetSidebarOpen(nOpen) {
	let sidebar = document.getElementById('SidebarContainer');
	sidebar.setAttribute('open', nOpen ? '1' : '0');
	sidebar.inert = !nOpen;   // keep the offscreen sidebar out of the Tab order
	if (nOpen)
		sidebar.querySelector('.SidebarItem')?.focus();
	else if (sidebar.contains(document.activeElement))
		document.querySelector('.SidebarBtn')?.focus();
}

function scrollToVendor(vendor) {
	const el = $(".OneVendorBlock[vendor='"+vendor+"']")[0];
	if (el){
		SetSidebarOpen(0);
		document.getElementById('Content').scrollTo({top: el.offsetTop, behavior: "smooth"});
	}
}

function UpdateSidebarVendors()
{
	let SidebarHTML = "";
	$(`.OneVendorBlock`).each((i, el)=>{
		UpdateVendorCheckbox(el.getAttribute("vendor"));
		SidebarHTML +=`<button type="button" class="SidebarItem" onclick="scrollToVendor(this.textContent)">${el.getAttribute('vendor')}</button>`;
	});
	$('#SidebarVendors').html(SidebarHTML)
}

function ChooseVendor(sVendor) { // automatically selects / unselects all
	const $cbs = $(`.OneVendorBlock[vendor='${sVendor}'] .ModelCheckBox`);
	const sel  = $cbs.length && $cbs.not('.ModelCheckBoxSelected').length;

	SetModelChecked($cbs, sel);

	$cbs.each((i, el)=>{SetModelSelect(sVendor, el.getAttribute('model'), sel)});
}

function UpdateVendorCheckbox(sVendor) {
	const $vb  = $(`.OneVendorBlock[vendor='${sVendor}']`);
	const $cbs = $vb.find(`.ModelCheckBox`);
	const $vcb = $vb.find(`.VendorCheckbox`);

	const selCount = $cbs.filter('.ModelCheckBoxSelected').length;
	const allSel   = selCount === $cbs.length && selCount > 0;
	const nonSel   = selCount === 0;

	// the native `indeterminate` property is the mixed state - the box maps it to
	// aria-checked="mixed" itself, so there is nothing to mirror onto the row.
	// Both halves are written only on a real change, for the reason in SetModelChecked.
	const mixed = !allSel && !nonSel;
	SetCheckedState($vcb, allSel);
	if ($vcb.prop('indeterminate') !== mixed)
		$vcb.prop('indeterminate', mixed);

	$vb.find(".modelCount").text(selCount + " / " + $cbs.length);
	$vb.find(".modelCountText").text(
		GetTransText('a11y_printers_selected', '{0} of {1} printers selected')
			.replace('{0}', selCount).replace('{1}', $cbs.length));
}

function OnExit()
{	
	let ModelAll={};
	
	let ModelSelect=$(".ModelCheckBoxSelected");
	let nTotal=ModelSelect.length;

	if( nTotal==0 ) {
		let noticeBar = document.getElementById("NoticeBar");
		noticeBar.classList.add("notice-error");
		noticeBar.textContent = "Error";
		ShowNotice(1);
		return 0;
	}
	
	for(let n=0;n<nTotal;n++)
	{
	    let OneItem=ModelSelect[n];
		
		let strModel=OneItem.getAttribute("model");
			
		//alert(strModel+strVendor+strNozzel);
		
		if(!ModelAll.hasOwnProperty(strModel))
		{
			//alert("ADD: "+strModel);
			
			ModelAll[strModel]={};
		
			ModelAll[strModel]["model"]=strModel;
		}
	}
		
	var tSend={};
	tSend['sequence_id']=Math.round(new Date() / 1000);
	tSend['command']="save_userguide_models";
	tSend['data']=ModelAll;
	
	SendWXMessage( JSON.stringify(tSend) );

    return nTotal;
}

function OnExitFilter() {
	let nTotal = 0;
	let ModelAll = {};
	for (let vendor in ModelNozzleSelected) {
		for (let model in ModelNozzleSelected[vendor]) {
			if (!ModelNozzleSelected[vendor][model])
				continue;

			if (!ModelAll.hasOwnProperty(model)) {
				//alert("ADD: "+strModel);

				ModelAll[model] = {};

				ModelAll[model]["model"] = model;
			}

			nTotal++;
		}
	}

	var tSend = {};
	tSend['sequence_id'] = Math.round(new Date() / 1000);
	tSend['command'] = "save_userguide_models";
	tSend['data'] = ModelAll;

	SendWXMessage(JSON.stringify(tSend));

	return nTotal;
}

// SNAPPY SCROLLING WITHOUT LAGS
const SNAP_DELAY			= 600;
const SNAP_DURATION			= 200;
const SNAP_CORR             = 8; // error correction / tolerance

let scrollTimer				= null;
let lastScrollTop			= 0;
let scrollDir				= 'down';
let isSnapping				= false;
let snapRafId				= null;
let lastSnapTarget			= null;
let waitingForUserScroll	= false;

function findSnap(cur, dir) {
	if (lastSnapTarget !== null && Math.abs(cur - lastSnapTarget) < SNAP_CORR) return null;

	const savedScroll = cur;

	$content[0].scrollTop = 0; // Temporarily scroll to 0 so getBoundingClientRect can get absolute positions

	let bcTop = el=>(el.getBoundingClientRect().top);

	const contentTop = bcTop($content[0]);
	const bannerH    = ($content.find('.BlockBanner')[0] || {}).offsetHeight || 0;

	const firstCard  = $content.find('.PrinterBlock')[0];
	const firstArea  = $content.find('.PrinterArea')[0];
	const cardGap    = (firstCard && firstArea) ? (bcTop(firstCard) - bcTop(firstArea)) : 0;

	const candidates = $content.find('.BlockBanner, .PrinterBlock').get();
	if (dir === 'up') candidates.reverse();

	let result = lastSeen = null;

	for (const el of candidates) {
		const snapTo = Math.round(
			el.classList.contains('BlockBanner')
				? (bcTop(el.closest('.OneVendorBlock')) - contentTop)
				: Math.max(0, bcTop(el) - contentTop - bannerH - cardGap)
		);
		if (snapTo != lastSeen){
			lastSeen = snapTo;
			if (dir === 'down' && snapTo > cur + SNAP_CORR) { result = snapTo; break; }
			if (dir === 'up'   && snapTo < cur - SNAP_CORR) { result = snapTo; break; }
		}
	}

	$content[0].scrollTop = savedScroll; // Restore scroll position

	return result;
}

function smoothScrollTo(target) {
	if (snapRafId) {
		cancelAnimationFrame(snapRafId);
		snapRafId = null;
	}

	const el   = $content[0];
	const from = el.scrollTop;
	const dist = target - from;
	const t0   = performance.now();
	const ease = t => t < 0.5 ? 2*t*t : -1 + (4 - 2*t)*t;

	function onDone() {
		el.scrollTop         = target;
		lastScrollTop        = lastSnapTarget = target;
		waitingForUserScroll = true;
		clearTimeout(scrollTimer);
		scrollTimer = null;
		snapRafId   = null;
		isSnapping  = false;
	}

	if (Math.abs(dist) < 2)
		return onDone();

	snapRafId = requestAnimationFrame(function step(now) {
		const p = Math.min((now - t0) / SNAP_DURATION, 1);
		el.scrollTop = from + dist * ease(p);
		if (p < 1)
			snapRafId = requestAnimationFrame(step);
		else
			onDone();
	});
}

function armSnap() {
	waitingForUserScroll = false;
	lastSnapTarget       = null;
}

function initScrollEvents() {
	$content.on('scroll', function() {
		if (isSnapping) return;

		if      (this.scrollTop > lastScrollTop + 1) scrollDir = 'down';
		else if (this.scrollTop < lastScrollTop - 1) scrollDir = 'up';

		lastScrollTop = this.scrollTop;

		if (waitingForUserScroll) return;

		clearTimeout(scrollTimer);
		scrollTimer = setTimeout(()=>{
			if (isSnapping) return;

			const target = findSnap($content[0].scrollTop, scrollDir);
			if (target){
				isSnapping = true;
				smoothScrollTo(target);
			}
		}, SNAP_DELAY);
	});

	let touchY = 0;
	$content[0].addEventListener('touchstart', e => {
		touchY = e.touches[0].clientY;
		armSnap();
	}, { passive: true });

	$content[0].addEventListener('touchmove', e => {
		const dy = touchY - e.touches[0].clientY;
		if (Math.abs(dy) > 3)
			scrollDir = dy > 0 ? 'down' : 'up';
	}, { passive: true });

	// Re-arm snap system on user scroll
	$content[0].addEventListener('wheel', armSnap, { passive: true });

	// Re-arm on after scrollbar usage
	$content[0].addEventListener('pointerdown', e => {
		if (e.target === $content[0])
			armSnap();
	});

	// Re-arm on keyboard scroll or focus changes
	document.addEventListener('keydown', e => {
		if (document.activeElement != SearchBox){
			let scrollKeys = ['ArrowUp','ArrowDown','PageUp','PageDown',' '];
			let hasFocus    = $content[0].contains(document.activeElement);
			if(scrollKeys.includes(e.key) || (hasFocus && e.which == 9))
				armSnap();
		}
	});

	// ORCA unfocus search bar while scrolling and its content empty
	$content[0].addEventListener("scroll", () => {
		if (document.activeElement === SearchBox && SearchBox.value == "")
			SearchBox.blur();
	});
}

document.addEventListener('DOMContentLoaded', initScrollEvents);

// LAYOUT SELECTOR
function LayoutMode(value) {
	let LayoutSelector = document.querySelector('.LayoutSelector > .TabGroup');
	let LayoutBtns     = Array.from(LayoutSelector.children);
	let LayoutTypes    = ["compact-list","compact-cover","large-cover"];

	if($content[0].getAttribute("layout") === value)
		return;

	// find current visible vendor and scroll to it after layout change
	let target = null;
	for (const el of $content.find('.OneVendorBlock')) {
		if (el.getBoundingClientRect().bottom - $content[0].getBoundingClientRect().top >= -1) {
			target = el.getAttribute("vendor");
			break;
		}
	}

	LayoutBtns.forEach(el => { el.classList.remove('selected'); el.setAttribute('aria-pressed', 'false'); });
	LayoutBtns[LayoutTypes.indexOf(value)].classList.add('selected');
	LayoutBtns[LayoutTypes.indexOf(value)].setAttribute('aria-pressed', 'true');
	$content[0].setAttribute("layout", value);

	if (target) scrollToVendor(target);
}

document.addEventListener('DOMContentLoaded', () => {
	LayoutMode("large-cover");
	SetSidebarOpen(0);

	// ORCA the open sidebar covers the page with a pointer-blocking scrim, so focus must
	// never land behind it: Tab off the last vendor would otherwise leave the keyboard on
	// dimmed, unclickable controls. A null relatedTarget is left alone on purpose: it means
	// focus left the document (window switch) or a click hit something unfocusable, and the
	// scrim already closes itself on click, so the only clicks it would catch are ones
	// inside the panel's own dead space.
	let sidebar = document.getElementById('SidebarContainer');
	sidebar.addEventListener('focusout', e => {
		if (sidebar.getAttribute('open') != "1" || !e.relatedTarget)
			return;
		// the hamburger is excluded so re-opening by click is not undone by the focus it moved
		if (!sidebar.contains(e.relatedTarget) && !e.relatedTarget.closest('.SidebarBtn'))
			SetSidebarOpen(0);
	});
});

// KEY EVENTS
function initKeyEvents(closeOnESC) {
	document.onkeydown = function (event) {
		var e = event || window.event || arguments.callee.caller.arguments[0];

		let sidebar = document.getElementById('SidebarContainer');

		if (e.keyCode == 27){
			if($("#NoticeBody").is(":visible")) { // the notice dialog is topmost, so it goes first
				ShowNotice(0);
			}
			else if(sidebar.getAttribute('open') == "1") { // prefer to close sidebar first if its open
				SetSidebarOpen(0);
			}
			else if (closeOnESC){
				ClosePage();
			}
		}

		// ORCA focus search bar on key input
		// writable character, no modifier, not already typing in a field,
		// not Space on a focused control (Space activates it instead),
		// and not while the notice dialog is topmost
		let a = document.activeElement || document.body;
		if (a != SearchBox && e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey
			&& !a.matches('input, textarea, select')
			&& !(e.key === ' ' && a.matches('button, a, [role], [tabindex]'))
			&& !$("#NoticeBody").is(":visible")) {
			SearchBox.focus();
			SetSidebarOpen(0); // typing implies leaving the sidebar
		}
	};
}
