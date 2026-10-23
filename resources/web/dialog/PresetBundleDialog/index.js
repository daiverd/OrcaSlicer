// ========= Data Stores =========
const bundlesById = new Map();        // bundleId -> bundle object
const printersByBundle = new Map();   // bundleId -> Map(index -> printerName)
const filamentsByBundle = new Map();  // bundleId -> Map(index -> filamentName)
const processesByBundle = new Map();    // bundleId -> Map(index -> presetName)
const UPDATE_TOOLTIP = T("update_available", "Update available");
const UNAUTHORIZED_TOOLTIP = T("unauthorized_bundle", "Unauthorized bundle");
// Display names for the bundle types the native side sends (also kept raw in data-bundle-type).
const BUNDLE_TYPE_LABELS = {
  Subscribed: T("type_subscribed", "Subscribed"),
  Local: T("type_local", "Local"),
  Default: T("type_default", "Default")
};

// ========= DOM =========
let topList = null;
let bottomList = null;

let ctxMenu = null;
let contextRow = null;
let ctxMenuSubscribed = null;

let ctxMenuDelete = null;

let selectedBundleId = null;

// ========= Init =========
function OnInit() {

   topList = document.getElementById("topList");
   bottomList = document.getElementById("bottomList");
   ctxMenu = document.getElementById("ctxMenu");
   ctxMenuSubscribed = document.getElementById("unsubscribe_btn");
   ctxMenuDelete = document.getElementById("delete_btn");
  const closeBtn = document.getElementById("close_btn");
   const exportbtn = document.getElementById("export_btn");
   const refreshBtn = document.getElementById("refresh_btn");                                                                                                                                                                                                                           
  const autoUpdateToggle = document.getElementById("auto_update_toggle");

  if (!topList || !bottomList) return;
  TranslatePage();

  // If wx side needs to request bundles after page load:
    RequestBundles();


    refreshBtn?.addEventListener("click", () => {                                                                                                                                                                                                                                        
    const tSend = {                                                                                                                                                                                                                                                                    
      sequence_id: Math.round(Date.now() / 1000),                                                                                                                                                                                                                                      
      command: "refresh_bundles"                                                                                                                                                                                                                                                       
    };                                                                                                                                                                                                                                                                                 
    SendWXMessage(JSON.stringify(tSend));                                                                                                                                                                                                                                              
  });                                                                                                                                                                                                                                                                                  
                                                                                                                                                                                                                                                                                       
  autoUpdateToggle?.addEventListener("change", () => {                                                                                                                                                                                                                                 
    const tSend = {                                                                                                                                                                                                                                                                    
      sequence_id: Math.round(Date.now() / 1000),                                                                                                                                                                                                                                      
      command: "set_auto_update",                                                                                                                                                                                                                                                      
      enabled: !!autoUpdateToggle.checked                                                                                                                                                                                                                                              
    };                                                                                                                                                                                                                                                                                 
    SendWXMessage(JSON.stringify(tSend));                                                                                                                                                                                                                                              
  });
  // Hook selection on top list
  topList.addEventListener("click", (e) => {                                                                                                                                                                                                                                                                                                                     
    const cloudLink = e.target.closest(".bundle-cloud-link");
    if (cloudLink) {
      e.preventDefault();
      e.stopPropagation();

      const row = cloudLink.closest(".row");
      if (!row) return;

      selectBundleRow(row);
      sendOpenBundleOnCloud(selectedBundleId);
      return;
    }

    const updateBtn = e.target.closest(".bundle-update-btn");                                                                                                                                                                                                                                                                                                    
    if (updateBtn) {                                                                                                                                                                                                                                                                                                                                             
      e.stopPropagation();                                                                                                                                                                                                                                                                                                                                       
      if (updateBtn.disabled) return;                                                                                                                                                                                                                                                                                                                            
                                                                                                                                                                                                                                                                                                                                                                 
      const row = updateBtn.closest(".row");                                                                                                                                                                                                                                                                                                                     
      if (!row) return;                                                                                                                                                                                                                                                                                                                                          
                                                                                                                                                                                                                                                                                                                                                                 
      selectBundleRow(row);
      sendUpdateBundleCommand(selectedBundleId);                                                                                                                                                                                                                                                                                                                 
      return;                                                                                                                                                                                                                                                                                                                                                    
    }                                                                                                                                                                                                                                                                                                                                                            
                                                                                                                                                                                                                                                                                                                                                                 
    const row = e.target.closest(".row");                                                                                                                                                                                                                                                                                                                        
    if (!row) return;                                                                                                                                                                                                                                                                                                                                            
                                                                                                                                                                                                                                                                                                                                                                 
    selectBundleRow(row);
  });

  // for top list rows if right click open context menu
  topList.addEventListener("contextmenu", (e) => {
    const row = e.target.closest(".row");
    if (!row) return; // top rows only

    e.preventDefault();
    openRowMenu(row, e.clientX, e.clientY);
  });

  // Enter/Space on a focused row selects it, like a click; its button and link keep their own keys
  topList.addEventListener("keydown", (e) => {
    const row = e.target.closest(".row");
    if (!row || e.target !== row || (e.key !== "Enter" && e.key !== " ")) return;

    e.preventDefault();
    selectBundleRow(row);
  });

  ctxMenu?.addEventListener("click", (e) => {
    const btn = e.target.closest("[data-action]");
    if (!btn || !contextRow) return;

    const tSend = {
      sequence_id: Math.round(Date.now() / 1000),
      command: "top_row_menu_action",
      action: String(btn.dataset.action || ""),
      bundle_id: String(contextRow.dataset.id || "")
    };
    SendWXMessage(JSON.stringify(tSend));
  });

  closeBtn?.addEventListener("click", () => {
    const tSend = {
      sequence_id: Math.round(Date.now() / 1000),
      command: "close_page"
    };
    SendWXMessage(JSON.stringify(tSend));
  });

  exportbtn?.addEventListener("click", () => {
    const tSend = {
      sequence_id: Math.round(Date.now() / 1000),
      command: "export_page"
    };
    SendWXMessage(JSON.stringify(tSend));
  });
}
// ========= wx bridge requests =========

function RequestBundles() {
    var tSend={};
	  tSend['sequence_id']=Math.round(new Date() / 1000);
	  tSend['command']="request_bundles";

    SendWXMessage(JSON.stringify(tSend));
}

function HandleStudio(pVal) {

  const msg = (typeof pVal === "string") ? safeJsonParse(pVal) : pVal;
  if (!msg || typeof msg !== "object") return;

  const strCmd = msg.command;
  if (strCmd === "list_bundles") {
    unpackPayload(msg);
    renderTop();
    // auto-select first bundle if none selected
    autoSelectFirstBundle();

    const autoUpdateToggle = document.getElementById("auto_update_toggle");                                                                                                                                                                                                            
    if (autoUpdateToggle) {                                                                                                                                                                                                                                                            
      autoUpdateToggle.checked = !!msg.auto_update_enabled;
    }
  }
}

// ========= Parse / store =========
function unpackPayload(payload) {
  bundlesById.clear();
  printersByBundle.clear();
  filamentsByBundle.clear();
  processesByBundle.clear();

  const list = payload?.data || [];
  for (const bundle of list) {
    const id = String(bundle.id ?? "");
    if (!id) continue;

    bundlesById.set(id, {                                                                                                                                                                                                                                                                                                                                          
    id,                                                                                                                                                                                                                                                                                                                                                          
    name: bundle.name ?? "",
    type: bundle.type ?? "",                                                                                                                                                                                                                                                                                                                                     
    version: bundle.version ?? "",                                                                                                                                                                                                                                                                                                                               
    path: bundle.path ?? "",                                                                                                                                                                                                                                                                                                                                     
    update_available: Boolean(bundle.update_available) ,   
    unauthorized: Boolean(bundle.unauthorized)                                                                                                                                                                                                                                                                                                           
  });  

    printersByBundle.set(id, new Map((bundle.printers || []).map((name, i) => [i, name])));
    filamentsByBundle.set(id, new Map((bundle.filaments || []).map((name, i) => [i, name])));
    processesByBundle.set(id, new Map((bundle.processes || []).map((name, i) => [i, name])));
  }
}

// ========= Render: top =========
function renderTop() {                                                                                                                                                                                                                                                                                                                                         
    const bundles = Array.from(bundlesById.values());                                                                                                                                                                                                                                                                                                            

    // rows are tab stops without a widget role: option/button would hide the Update button and link
    topList.innerHTML = bundles.map(b => `                                                                                                                                                                                                                                                                                                                       
      <div class="row" data-id="${EscAttr(b.id)}" data-bundle-type="${EscAttr(String(b.type || "").toLowerCase())}" tabindex="0">
        <div class="cell bundle-name-cell" title="${EscAttr(b.name)}">                                                                                                                                                                                                                                                                                        
          ${b.unauthorized
            ? `<span class="bundle-status-icon bundle-status-icon-unauthorized" title="${EscAttr(UNAUTHORIZED_TOOLTIP)}" aria-label="${EscAttr(UNAUTHORIZED_TOOLTIP)}">!</span>`
            : b.update_available
              ? `<span class="bundle-status-icon bundle-status-icon-update" title="${EscAttr(UPDATE_TOOLTIP)}" aria-label="${EscAttr(UPDATE_TOOLTIP)}">&uarr;</span>`
              : `<span class="bundle-status-icon-spacer" aria-hidden="true"></span>`}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          
          ${
            b.type === "Subscribed" ?
            `<a href="#" class="bundle-name-text bundle-cloud-link" title="${EscAttr(T("open_in_browser", "Open this bundle in your browser"))}">${escapeHtml(b.name)}</a>`
            : `<span class="bundle-name-text">${escapeHtml(b.name)}</span>`
          }                                                                                                                                                                                                                                                                                
        </div>                                                                                                                                                                                                                                                                                                                                                   
        <span title="${EscAttr(BundleTypeLabel(b.type))}">${escapeHtml(BundleTypeLabel(b.type))}</span>                                                                                                                                                                                                                                                                                         
        <span title="${EscAttr(b.version)}">${escapeHtml(b.version)}</span>                                                                                                                                                                                                                                                                                   
        <div class="cell bundle-update-cell">                                                                                                                                                                                                                                                                                                                    
            <button                                                                                                                                                                                                                                                               
            type="button"                                                                                                                                                                                                                                                       
            class="bundle-update-btn ${(!b.unauthorized && b.update_available) ? "is-enabled" : "is-disabled"}"                                                                                                                                                                 
            ${(!b.unauthorized && b.update_available) ? "" : "disabled"}                                                                                                                                                                                                        
            data-id="${EscAttr(b.id)}"                                                                                                                                                                                                                                       
          >${escapeHtml(T("update", "Update"))}</button>                                                                                                                                                                                                                                                                                                                                       
        </div>                                                                                                                                                                                                                                                                                                                                                   
      </div>                                                                                                                                                                                                                                                                                                                                                     
    `).join("");                                                                                                                                                                                                                                                                                                                                                 
  }

function sendOpenBundleOnCloud(bundleId) {
  const bundle = bundlesById.get(String(bundleId || ""));
  if (!bundle) return;

  const tSend = {
    sequence_id: Math.round(Date.now() / 1000),
    command: "open_bundle_on_cloud",
    bundle_id: String(bundle.id || "")
  };
  SendWXMessage(JSON.stringify(tSend));
}

function sendUpdateBundleCommand(bundleId) {                                                                                                                                                                                                                                                                                                                   
    const bundle = bundlesById.get(String(bundleId || ""));                                                                                                                                                                                                                                                                                                      
    if (!bundle || bundle.unauthorized || !bundle.update_available) return;                                                                                                                                                                                                                                                                                                             
                                                                                                                                                                                                                                                                                                                                                                 
    const tSend = {                                                                                                                                                                                                                                                                                                                                              
      sequence_id: Math.round(Date.now() / 1000),                                                                                                                                                                                                                                                                                                                
      command: "update_bundle",                                                                                                                                                                                                                                                                                                                                  
      bundle_id: String(bundle.id || "")                                                                                                                                                                                                                                                                                                                         
    };                                                                                                                                                                                                                                                                                                                                                           
    SendWXMessage(JSON.stringify(tSend));                                                                                                                                                                                                                                                                                                                        
  }

// ========= Render: bottom (for a selected bundle) =========
function renderBottomForBundle(bundleId) {
  const key = String(bundleId || "");
  const printers = printersByBundle.get(key) || new Map();
  const filaments = filamentsByBundle.get(key) || new Map();
  const processes = processesByBundle.get(key) || new Map();

  // Convert to a flat list of rows { typeLabel, name }
  const rows = [];

  for (const [, name] of printers) rows.push({ type: T("printer", "Printer"), name });
  for (const [, name] of filaments) rows.push({ type: T("filament", "Filament"), name });
  for (const [, name] of processes) rows.push({ type: T("process", "Process"), name });

  bottomList.innerHTML = rows.map((r, idx) => `
    <div class="row" data-id="${EscAttr(bundleId)}" data-idx="${idx}">
      <span>${escapeHtml(r.name)}</span>
      <span title="${EscAttr(r.type)}">${escapeHtml(r.type)}</span>
    </div>
  `).join("");
}

// ========= Selection helpers =========
function clearSelection() {
  document.querySelectorAll(".row.selected").forEach(r => r.classList.remove("selected"));
}

function selectTopRow(rowEl) {
  // only clear selection in top list, not bottom
  topList.querySelectorAll(".row.selected").forEach(r => r.classList.remove("selected"));
  rowEl.classList.add("selected");
  // aria-current: aria-selected needs a listbox the list is not
  topList.querySelectorAll(".row").forEach(r => SetAriaState(r, "aria-current", r === rowEl));
}

function selectBundleRow(rowEl) {
  selectTopRow(rowEl);
  selectedBundleId = String(rowEl.dataset.id || "");
  renderBottomForBundle(selectedBundleId);
}

function autoSelectFirstBundle() {
  if (selectedBundleId && bundlesById.has(selectedBundleId)) {
    // reselect existing
    const el = topList.querySelector(`.row[data-id="${cssEscape(selectedBundleId)}"]`);
    if (el) selectTopRow(el);
    renderBottomForBundle(selectedBundleId);
    return;
  }

  const first = topList.querySelector(".row");
  if (!first) {
    bottomList.innerHTML = "";
    selectedBundleId = null;
    return;
  }

  selectBundleRow(first);
}

function openRowMenu(row, x, y) {
  selectTopRow(row);
  contextRow = row;

  if (String(row.dataset.bundleType || "").toLowerCase() === "subscribed")
    showSubscribedMenu(x, y);
  else
    showMenu(x, y);
  A11yMenu(ctxMenu, row, hideMenu);
}

function showSubscribedMenu(x, y) {
  if (!ctxMenu) return;
  ctxMenu.style.left = `${x}px`;
  ctxMenu.style.top = `${y}px`;
  ctxMenu.hidden = false;
  ctxMenuDelete.hidden = true;
  ctxMenuSubscribed.hidden = false;
}

function showMenu(x, y) {
  if (!ctxMenu) return;
  ctxMenu.style.left = `${x}px`;
  ctxMenu.style.top = `${y}px`;
  ctxMenu.hidden = false;
  ctxMenuDelete.hidden = false;
  ctxMenuSubscribed.hidden = true;
}

function hideMenu() {
  if (!ctxMenu) return;
  ctxMenu.hidden = true;
  ctxMenuSubscribed.hidden = true;
  contextRow = null;
}
// ========= Utilities =========
function BundleTypeLabel(type) {
  return BUNDLE_TYPE_LABELS[type] || type;
}

function safeJsonParse(s) {
  try { return JSON.parse(s); } catch { return null; }
}

function escapeHtml(str) {
  return String(str)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function cssEscape(str) {
  // basic css escape for attribute selectors
  return String(str).replaceAll('"', '\\"');
}
