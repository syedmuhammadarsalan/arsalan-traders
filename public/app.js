const API = ""; // same origin as the page
const app = document.getElementById("app");
const statusDot = document.getElementById("statusDot");
const toastEl = document.getElementById("toast");
let currentTab = "dashboard";

// ---------- helpers ----------
function toast(msg) {
  toastEl.textContent = msg;
  toastEl.classList.add("show");
  setTimeout(() => toastEl.classList.remove("show"), 2000);
}

async function api(path, options = {}) {
  const res = await fetch(API + path, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  statusDot.classList.add("online");
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || "Request failed");
  return data;
}

function money(n) {
  return "Rs " + Number(n).toLocaleString(undefined, { maximumFractionDigits: 2 });
}

function gaugeInfo(p) {
  // Reference max = 3x the low-stock threshold, so the bar has headroom above "healthy"
  const ref = Math.max(p.low_stock_threshold * 3, 1);
  const pct = Math.min(100, (p.quantity / ref) * 100);
  let level = "ok";
  if (p.quantity <= 0) level = "critical";
  else if (p.quantity <= p.low_stock_threshold) level = "low";
  return { pct, level };
}

function gaugeHTML(p) {
  const { pct, level } = gaugeInfo(p);
  const label = p.quantity <= 0 ? "OUT" : `${p.quantity} ${p.unit}`;
  return `
    <div class="gauge-row">
      <div class="gauge"><div class="gauge-fill ${level}" style="width:${pct}%"></div></div>
      <div class="gauge-reading ${level}">${label}</div>
    </div>`;
}

// ---------- tab wiring ----------
document.querySelectorAll(".tab").forEach(btn => {
  btn.addEventListener("click", () => switchTab(btn.dataset.tab));
});

function switchTab(tab) {
  currentTab = tab;
  document.querySelectorAll(".tab").forEach(b => b.classList.toggle("active", b.dataset.tab === tab));
  render();
}

function render() {
  if (currentTab === "dashboard") renderDashboard();
  else if (currentTab === "inventory") renderInventory();
  else if (currentTab === "add") renderAddForm();
  else if (currentTab === "stock") renderStockForm();
}

// ---------- Dashboard ----------
async function renderDashboard() {
  app.innerHTML = `<div class="eyebrow">Overview</div><h1 class="page-title">Dashboard</h1><div id="dashBody"></div>`;
  const body = document.getElementById("dashBody");
  try {
    const [d, txns] = await Promise.all([api("/api/dashboard"), api("/api/transactions?limit=8")]);
    let html = `
      <div class="stat-row">
        <div class="stat-card"><div class="label">Total stock on hand</div><div class="value">${d.total_stock_units}</div></div>
        <div class="stat-card"><div class="label">Inventory value</div><div class="value">${money(d.total_inventory_value)}</div></div>
      </div>`;

    html += `<div class="section-title">Low stock <span class="count-pill">${d.low_stock_count}</span></div>`;
    if (d.low_stock_items.length === 0) {
      html += `<div class="empty-state">Everything is above its low-stock line.</div>`;
    } else {
      html += d.low_stock_items.map(p => `
        <div class="item-card">
          <div class="item-top">
            <div><div class="item-name">${esc(p.name)}</div><div class="item-cat">${esc(p.category || "Uncategorized")}</div></div>
            <div class="item-price">${money(p.price)}</div>
          </div>
          ${gaugeHTML(p)}
        </div>`).join("");
    }

    html += `<div class="section-title">Recent activity</div>`;
    if (txns.length === 0) {
      html += `<div class="empty-state">No stock movements logged yet.</div>`;
    } else {
      html += `<div class="item-card">` + txns.map(t => `
        <div class="txn-row">
          <div><span class="txn-tag ${t.type}">${t.type}</span>${esc(t.pesticide_name)}</div>
          <div class="txn-meta">${t.quantity} · ${formatDate(t.created_at)}</div>
        </div>`).join("") + `</div>`;
    }

    body.innerHTML = html;
  } catch (e) {
    body.innerHTML = errorState(e);
  }
}

// ---------- Inventory ----------
async function renderInventory(search = "") {
  app.innerHTML = `
    <div class="eyebrow">Stock room</div><h1 class="page-title">Inventory</h1>
    <div class="search-bar">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="7"/><path d="M21 21l-4.3-4.3"/></svg>
      <input id="searchInput" placeholder="Search by name or category" value="${esc(search)}">
    </div>
    <div id="invList"></div>`;

  document.getElementById("searchInput").addEventListener("input", debounce(e => renderInventory(e.target.value), 250));

  const list = document.getElementById("invList");
  try {
    const items = await api("/api/pesticides" + (search ? "?search=" + encodeURIComponent(search) : ""));
    if (items.length === 0) {
      list.innerHTML = `<div class="empty-state">No pesticides found. Add one from the Add tab.</div>`;
      return;
    }
    list.innerHTML = items.map(p => `
      <div class="item-card">
        <div class="item-top">
          <div><div class="item-name">${esc(p.name)}</div><div class="item-cat">${esc(p.category || "Uncategorized")}</div></div>
          <div class="item-price">${money(p.price)}</div>
        </div>
        ${gaugeHTML(p)}
        <div class="item-actions">
          <button class="btn" onclick="editItem(${p.id})">Edit</button>
          <button class="btn btn-danger-text" onclick="removeItem(${p.id}, '${escJs(p.name)}')">Delete</button>
        </div>
      </div>`).join("");
  } catch (e) {
    list.innerHTML = errorState(e);
  }
}

async function removeItem(id, name) {
  if (!confirm(`Delete "${name}"? This cannot be undone.`)) return;
  try {
    await api(`/api/pesticides/${id}`, { method: "DELETE" });
    toast("Deleted");
    renderInventory();
  } catch (e) { toast(e.message); }
}

async function editItem(id) {
  try {
    const p = await api(`/api/pesticides/${id}`);
    app.innerHTML = formHTML("Edit Pesticide", p);
    wireForm(async (data) => {
      await api(`/api/pesticides/${id}`, { method: "PUT", body: JSON.stringify(data) });
      toast("Saved");
      switchTab("inventory");
    }, "Save Changes");
  } catch (e) { toast(e.message); }
}

// ---------- Add ----------
function renderAddForm() {
  app.innerHTML = formHTML("Add Pesticide", {});
  wireForm(async (data) => {
    await api("/api/pesticides", { method: "POST", body: JSON.stringify(data) });
    toast("Added to inventory");
    switchTab("inventory");
  }, "Add to Inventory");
}

function formHTML(title, p) {
  return `
    <div class="eyebrow">${p.id ? "Update record" : "New record"}</div>
    <h1 class="page-title">${title}</h1>
    <form id="itemForm" style="margin-top:16px">
      <div class="field"><label>Name</label><input name="name" required value="${esc(p.name || "")}" placeholder="e.g. Cypermethrin 10% EC"></div>
      <div class="field"><label>Category</label><input name="category" value="${esc(p.category || "")}" placeholder="e.g. Insecticide"></div>
      <div class="field-row">
        <div class="field"><label>Price per unit</label><input name="price" type="number" step="0.01" min="0" required value="${p.price ?? ""}"></div>
        <div class="field"><label>Unit</label><input name="unit" value="${esc(p.unit || "bottle")}" placeholder="bottle, kg, liter..."></div>
      </div>
      <div class="field-row">
        <div class="field"><label>Quantity in stock</label><input name="quantity" type="number" step="1" min="0" required value="${p.quantity ?? ""}"></div>
        <div class="field"><label>Low-stock alert at</label><input name="low_stock_threshold" type="number" step="1" min="0" value="${p.low_stock_threshold ?? 10}"></div>
      </div>
      <button type="submit" class="btn btn-primary btn-full">Submit</button>
    </form>`;
}

function wireForm(onSubmit) {
  document.getElementById("itemForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    const fd = new FormData(e.target);
    const data = {
      name: fd.get("name").trim(),
      category: fd.get("category").trim(),
      price: parseFloat(fd.get("price")) || 0,
      unit: fd.get("unit").trim() || "unit",
      quantity: parseInt(fd.get("quantity")) || 0,
      low_stock_threshold: parseInt(fd.get("low_stock_threshold")) || 0,
    };
    try {
      await onSubmit(data);
    } catch (err) { toast(err.message); }
  });
}

// ---------- Stock in/out ----------
let stockType = "IN";

async function renderStockForm() {
  app.innerHTML = `
    <div class="eyebrow">Log movement</div><h1 class="page-title">Stock In / Out</h1>
    <div class="type-toggle">
      <button id="btnIn" class="selected in">STOCK IN</button>
      <button id="btnOut" class="out">STOCK OUT</button>
    </div>
    <form id="stockForm">
      <div class="field">
        <label>Pesticide</label>
        <select name="pesticide_id" id="pesticideSelect" required></select>
      </div>
      <div class="field"><label>Quantity</label><input name="quantity" type="number" min="1" step="1" required placeholder="e.g. 20"></div>
      <div class="field"><label>Note (optional)</label><input name="note" placeholder="e.g. Supplier delivery, or sold to customer"></div>
      <button type="submit" class="btn btn-primary btn-full">Log Stock Movement</button>
    </form>`;

  stockType = "IN";
  document.getElementById("btnIn").addEventListener("click", () => setStockType("IN"));
  document.getElementById("btnOut").addEventListener("click", () => setStockType("OUT"));

  const select = document.getElementById("pesticideSelect");
  try {
    const items = await api("/api/pesticides");
    if (items.length === 0) {
      select.innerHTML = `<option value="">No pesticides yet — add one first</option>`;
    } else {
      select.innerHTML = items.map(p => `<option value="${p.id}">${esc(p.name)} (${p.quantity} ${esc(p.unit)} in stock)</option>`).join("");
    }
  } catch (e) { toast(e.message); }

  document.getElementById("stockForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    const fd = new FormData(e.target);
    const id = fd.get("pesticide_id");
    if (!id) { toast("Add a pesticide first"); return; }
    try {
      await api(`/api/pesticides/${id}/stock`, {
        method: "POST",
        body: JSON.stringify({
          type: stockType,
          quantity: parseInt(fd.get("quantity")) || 0,
          note: fd.get("note").trim(),
        }),
      });
      toast(stockType === "IN" ? "Stock added" : "Stock removed");
      switchTab("dashboard");
    } catch (err) { toast(err.message); }
  });
}

function setStockType(type) {
  stockType = type;
  document.getElementById("btnIn").classList.toggle("selected", type === "IN");
  document.getElementById("btnOut").classList.toggle("selected", type === "OUT");
}

// ---------- utils ----------
function esc(s) {
  return String(s).replace(/[&<>"']/g, c => ({ "&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;" }[c]));
}
function escJs(s) { return String(s).replace(/'/g, "\\'"); }
function debounce(fn, ms) {
  let t;
  return (...args) => { clearTimeout(t); t = setTimeout(() => fn(...args), ms); };
}
function formatDate(iso) {
  if (!iso) return "";
  const d = new Date(iso.replace(" ", "T") + "Z");
  return d.toLocaleString(undefined, { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" });
}
function errorState(e) {
  return `<div class="empty-state">Couldn't reach the server.<br>${esc(e.message)}</div>`;
}

render();
