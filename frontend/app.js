
const state = {
  accountNo: localStorage.getItem("accountNo") || "",
  account: null
};

const $ = (q) => document.querySelector(q);

function toast(message, bad=false) {
  const t = $("#toast");
  t.textContent = message;
  t.style.borderColor = bad ? "#7d2d2d" : "#2a4f7d";
  t.classList.add("show");
  setTimeout(() => t.classList.remove("show"), 2600);
}

async function api(path, options = {}) {
  const res = await fetch(path, {
    headers: {"Content-Type":"application/x-www-form-urlencoded", ...(options.headers || {})},
    ...options
  });
  const data = await res.json();
  if (!res.ok || data.ok === false) throw new Error(data.message || "Request failed");
  return data;
}

function money(n) {
  return "₹" + Number(n || 0).toLocaleString("en-IN", {minimumFractionDigits:2, maximumFractionDigits:2});
}

function escapeHtml(s) {
  return String(s ?? "").replace(/[&<>"']/g, c => ({
    "&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#039;"
  }[c]));
}

function copyText(text) {
  navigator.clipboard?.writeText(text).then(() => toast("Copied to clipboard")).catch(() => toast(text));
}

function qrMatrix(seed) {
  const n = 21, grid = [];
  let h = 2166136261;
  for (const ch of seed) h = Math.imul(h ^ ch.charCodeAt(0), 16777619);

  for (let r=0;r<n;r++) {
    for (let c=0;c<n;c++) {
      let on = ((h + r*31 + c*17 + r*c) % 3) === 0;
      const finder = (rr,cc) => (rr<7&&cc<7)||(rr<7&&cc>=n-7)||(rr>=n-7&&cc<7);
      if (finder(r,c)) {
        const fr = r < 7 ? r : r-(n-7);
        const fc = c < 7 ? c : c-(n-7);
        on = fr===0 || fr===6 || fc===0 || fc===6 || (fr>=2&&fr<=4&&fc>=2&&fc<=4);
      }
      grid.push(on);
    }
  }
  return grid;
}

function renderQr(seed) {
  return `<div class="qr-grid">${qrMatrix(seed).map(x=>`<div class="qr-cell ${x?'on':''}"></div>`).join("")}</div>`;
}

function navActive(page) {
  document.querySelectorAll(".nav").forEach(b => b.classList.toggle("active", b.dataset.page === page));
  const titles = {
    dashboard:"Dashboard", accounts:"Accounts", deposit:"Deposit", withdraw:"Withdraw",
    transfer:"Transfer Money", transactions:"Transaction History", qr:"QR Cardless ATM",
    cards:"Cards", loans:"Loan Management", fd:"Fixed Deposit", investments:"Investments",
    passbook:"e-Passbook", profile:"Account Profile", admin:"Admin"
  };
  $("#pageTitle").textContent = titles[page] || "Dashboard";
  $("#sessionState").textContent = state.accountNo ? `Logged in • ${state.accountNo}` : "Not logged in";
}

async function refreshAccount() {
  if (!state.accountNo) return;
  try {
    const d = await api(`/api/account?accountNo=${encodeURIComponent(state.accountNo)}`, {method:"GET"});
    state.account = d.account;
  } catch {
    state.account = null;
  }
}

function dashboard() {
  navActive("dashboard");
  const a = state.account;
  if (!a) {
    $("#content").innerHTML = `
      <div class="hero">
        <h1>Welcome to Smart Bank</h1>
        <p>Create an account or login to start using the banking dashboard.</p>
        <div class="actions"><button class="btn primary" onclick="show('accounts')">Open Accounts</button></div>
      </div>
      <div class="grid grid4">
        <div class="card"><h3>Fast Search</h3><div class="metric">O(1)</div><div class="muted">Hash map account lookup</div></div>
        <div class="card"><h3>Account Index</h3><div class="metric">BST</div><div class="muted">Tree-based search</div></div>
        <div class="card"><h3>Transaction</h3><div class="metric">STACK</div><div class="muted">Recent transaction IDs</div></div>
        <div class="card"><h3>ATM Requests</h3><div class="metric">QUEUE</div><div class="muted">FIFO service flow</div></div>
      </div>`;
    return;
  }

  $("#content").innerHTML = `
    <div class="hero">
      <h1>Welcome back, ${escapeHtml(a.name)}</h1>
      <p>Smart Bank digital banking dashboard powered by a C++ backend.</p>
      <div class="actions">
        <button class="btn primary" onclick="show('deposit')">Deposit</button>
        <button class="btn" onclick="show('withdraw')">Withdraw</button>
        <button class="btn" onclick="show('transfer')">Transfer</button>
        <button class="btn success" onclick="show('qr')">QR Cardless ATM</button>
      </div>
    </div>

    <div class="grid grid4">
      <div class="card"><h3>Available Balance</h3><div class="metric green">${money(a.balance)}</div></div>
      <div class="card"><h3>Account Number</h3><div class="metric" style="font-size:19px">${a.accountNo}</div></div>
      <div class="card"><h3>Account Type</h3><div class="metric" style="font-size:19px">${escapeHtml(a.type)}</div></div>
      <div class="card"><h3>Account Created</h3><div class="metric" style="font-size:16px">${escapeHtml(a.createdAt || "Recently")}</div></div>
    </div>

    <div class="grid grid2" style="margin-top:18px">
      <div class="card">
        <h3>Project DSA</h3>
        <p class="muted">unordered_map + BST + queue + stack + vector + file handling</p>
      </div>
      <div class="card">
        <h3>Modern Banking</h3>
        <p class="muted">Investments, e-Passbook, profile updates, account creation date and quick account actions.</p>
      </div>
    </div>

    <div class="hero mini-hero">
      <h1 style="font-size:22px">Recently Created Account</h1>
      <p>Account number <b>${escapeHtml(a.accountNo)}</b> was created on <b>${escapeHtml(a.createdAt || "Recently")}</b>.</p>
      <div class="actions"><button class="btn" onclick="copyText('${escapeHtml(a.accountNo)}')">Copy Account Number</button><button class="btn" onclick="show('passbook')">Open e-Passbook</button></div>
    </div>`;
}

function accounts() {
  navActive("accounts");
  $("#content").innerHTML = `
    <div class="grid grid2">
      <div class="card">
        <h3>Create New Account</h3>
        <form id="registerForm" class="form">
          <div class="field"><label>Full Name</label><input name="name" required></div>
          <div class="field"><label>Mobile</label><input name="mobile" required></div>
          <div class="field"><label>Email</label><input name="email"></div>
          <div class="field"><label>Address</label><input name="address"></div>
          <div class="field"><label>Username</label><input name="username" required></div>
          <div class="field"><label>Password</label><input name="password" type="password" minlength="4" required></div>
          <div class="field"><label>Opening Deposit</label><input name="openingDeposit" type="number" min="0" step="0.01" value="5000"></div>
          <button class="btn primary">Create Account</button>
        </form>
      </div>

      <div class="card">
        <h3>Customer Login</h3>
        <form id="loginForm" class="form">
          <div class="field"><label>Account Number</label><input name="accountNo" required></div>
          <div class="field"><label>Username</label><input name="username" required></div>
          <div class="field"><label>Password</label><input name="password" type="password" required></div>
          <button class="btn success">Login</button>
        </form>
        <div class="code-note" style="margin-top:18px">
          Create an account first, then use the generated account number for login.
        </div>
        <div id="createdAccountNote" class="code-note" style="margin-top:12px"></div>
      </div>
    </div>`;

  $("#registerForm").onsubmit = async e => {
    e.preventDefault();
    const data = new URLSearchParams(new FormData(e.target));
    try {
      const d = await api("/api/register",{method:"POST",body:data});
      localStorage.setItem("recentAccountNo", d.accountNo);
      toast(`Account created: ${d.accountNo}`);
      e.target.reset();
      const note = document.querySelector("#createdAccountNote");
      if (note) note.innerHTML = `<b>New account created:</b> ${escapeHtml(d.accountNo)} <button class="btn" onclick="copyText('${escapeHtml(d.accountNo)}')">Copy</button>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#loginForm").onsubmit = async e => {
    e.preventDefault();
    const data = new URLSearchParams(new FormData(e.target));
    try {
      const d = await api("/api/login",{method:"POST",body:data});
      state.accountNo = d.account.accountNo;
      localStorage.setItem("accountNo", state.accountNo);
      state.account = d.account;
      toast("Login successful");
      show("dashboard");
    } catch(err){ toast(err.message,true); }
  };
}

function transactionForm(type) {
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3><p class="muted">Go to Accounts and login first.</p></div>`;
    return;
  }

  const isDeposit = type === "deposit";
  navActive(type);
  $("#content").innerHTML = `
    <div class="grid grid2">
      <div class="card">
        <h3>${isDeposit ? "Deposit Money" : "Withdraw Money"}</h3>
        <div class="field"><label>Account</label><input value="${state.accountNo}" disabled></div>
        <div class="field" style="margin-top:14px"><label>Current Balance</label><input value="${money(state.account.balance)}" disabled></div>
        <div class="field" style="margin-top:14px"><label>Amount (₹)</label><input id="txAmount" type="number" min="0.01" step="0.01"></div>
        <div class="actions"><button id="txBtn" class="btn ${isDeposit?'success':'primary'}">${isDeposit?"Deposit Funds":"Withdraw Funds"}</button></div>
      </div>
    </div>`;

  $("#txBtn").onclick = async () => {
    const amount = $("#txAmount").value;
    try {
      const d = await api(`/api/${isDeposit?"deposit":"withdraw"}`,{
        method:"POST",
        body:new URLSearchParams({accountNo:state.accountNo, amount})
      });
      await refreshAccount();
      toast(d.message);
      dashboard();
    } catch(err){ toast(err.message,true); }
  };
}

function transfer() {
  navActive("transfer");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3><p class="muted">Please login first.</p></div>`;
    return;
  }

  $("#content").innerHTML = `
    <div class="card">
      <h3>Fund Transfer</h3>
      <div class="grid grid2">
        <div class="field"><label>From Account</label><input value="${state.accountNo}" disabled></div>
        <div class="field"><label>To Account</label><input id="toAcc"></div>
        <div class="field"><label>Amount (₹)</label><input id="transferAmount" type="number" min="0.01" step="0.01"></div>
      </div>
      <div class="actions"><button id="transferBtn" class="btn primary">Transfer Now</button></div>
    </div>`;

  $("#transferBtn").onclick = async () => {
    try {
      const d = await api("/api/transfer",{
        method:"POST",
        body:new URLSearchParams({
          from:state.accountNo, to:$("#toAcc").value, amount:$("#transferAmount").value
        })
      });
      await refreshAccount();
      toast(d.message);
      dashboard();
    } catch(err){ toast(err.message,true); }
  };
}

async function transactions() {
  navActive("transactions");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3></div>`;
    return;
  }
  try {
    const d = await api(`/api/transactions?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
    const rows = [...d.transactions].reverse().map(t=>`
      <tr>
        <td>${escapeHtml(t.time)}</td>
        <td>${escapeHtml(t.type)}</td>
        <td>${money(t.amount)}</td>
        <td>${money(t.balanceAfter)}</td>
        <td>${escapeHtml(t.remark)}</td>
      </tr>`).join("");
    $("#content").innerHTML = `
      <div class="card">
        <h3>Transaction History — Linked List / Vector Storage</h3>
        <div class="table-wrap" style="margin-top:14px">
          <table><thead><tr><th>Date & Time</th><th>Type</th><th>Amount</th><th>Balance</th><th>Remark</th></tr></thead>
          <tbody>${rows || `<tr><td colspan="5">No transactions found.</td></tr>`}</tbody></table>
        </div>
      </div>`;
  } catch(err) { toast(err.message,true); }
}

function qr() {
  navActive("qr");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3></div>`;
    return;
  }

  $("#content").innerHTML = `
    <div class="qr-layout">
      <div class="card">
        <h3>Step 1 — Generate Cardless Withdrawal</h3>
        <div class="field"><label>Account</label><input value="${state.accountNo}" disabled></div>
        <div class="field" style="margin-top:14px"><label>Amount (₹)</label><input id="qrAmount" type="number" min="0.01" step="0.01"></div>
        <div class="actions"><button id="qrGenerate" class="btn success">Generate QR + OTP</button></div>

        <div class="code-note" style="margin-top:18px">
          The QR/OTP flow is an academic simulation. The real-world version would use
          secure banking infrastructure and a standards-compliant payment QR.
        </div>
      </div>

      <div class="card">
        <h3>Step 2 — Cardless ATM</h3>
        <div class="field"><label>QR ID</label><input id="qrId"></div>
        <div class="field" style="margin-top:14px"><label>OTP</label><input id="qrOtp"></div>
        <div class="actions"><button id="qrComplete" class="btn primary">Verify & Dispense Cash</button></div>
        <div id="qrResult" style="margin-top:18px"></div>
      </div>
    </div>`;

  $("#qrGenerate").onclick = async () => {
    try {
      const d = await api("/api/qr/create",{
        method:"POST",
        body:new URLSearchParams({accountNo:state.accountNo,amount:$("#qrAmount").value})
      });
      $("#qrId").value = d.qrId;
      $("#qrOtp").value = d.otp;

      $("#qrResult").innerHTML = `
        <div class="qr-box">
          ${renderQr(d.qrId+"|"+d.amount)}
          <div class="qr-meta"><b>QR ID:</b> ${d.qrId}<br>
          <b>Amount:</b> ${money(d.amount)}<br>
          <b>OTP:</b> ${d.otp}<br>
          <b>Valid:</b> 5 minutes</div>
        </div>`;
      toast("QR transaction generated");
    } catch(err){ toast(err.message,true); }
  };

  $("#qrComplete").onclick = async () => {
    try {
      const d = await api("/api/qr/complete",{
        method:"POST",
        body:new URLSearchParams({
          qrId:$("#qrId").value,
          otp:$("#qrOtp").value,
          accountNo:state.accountNo
        })
      });
      await refreshAccount();
      toast(d.message);
      dashboard();
    } catch(err){ toast(err.message,true); }
  };
}

async function investments() {
  navActive("investments");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3><p class="muted">Please login first.</p></div>`;
    return;
  }

  const catalog = [
    {symbol:"SBX", name:"Smart Banking Index", price:250},
    {symbol:"TEC", name:"Technology Growth Fund", price:420},
    {symbol:"GRW", name:"Balanced Growth Fund", price:180},
    {symbol:"GLD", name:"Digital Gold Simulation", price:95}
  ];

  const d = await api(`/api/investments?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
  const rows = d.investments.map(inv => `<tr><td>${escapeHtml(inv.symbol)}</td><td>${escapeHtml(inv.name)}</td><td>${inv.units}</td><td>${money(inv.buyPrice)}</td><td>${money(inv.currentPrice)}</td><td class="${inv.gain>=0?'green':'red'}">${money(inv.gain)}</td><td><button class="btn danger sell-btn" data-id="${inv.id}">Sell</button></td></tr>`).join("");

  $("#content").innerHTML = `
    <div class="grid grid4">
      <div class="card"><h3>Portfolio Cost</h3><div class="metric">${money(d.cost)}</div></div>
      <div class="card"><h3>Portfolio Value</h3><div class="metric green">${money(d.value)}</div></div>
      <div class="card"><h3>Unrealized Gain</h3><div class="metric ${d.value-d.cost>=0?'green':'red'}">${money(d.value-d.cost)}</div></div>
      <div class="card"><h3>Available Balance</h3><div class="metric">${money(state.account.balance)}</div></div>
    </div>

    <div class="grid grid2" style="margin-top:18px">
      <div class="card">
        <h3>Simulated Investment Market</h3>
        <div class="form">
          <div class="field"><label>Asset</label><select id="invAsset">${catalog.map(x=>`<option value="${x.symbol}" data-name="${x.name}" data-price="${x.price}">${x.symbol} — ${x.name} @ ${money(x.price)}</option>`).join("")}</select></div>
          <div class="field"><label>Units</label><input id="invUnits" type="number" min="0.01" step="0.01" value="1"></div>
          <button id="buyInv" class="btn success">Buy Investment</button>
        </div>
        <div class="code-note" style="margin-top:14px">Investment values are simulated for this academic project and are not real market prices.</div>
      </div>
      <div class="card">
        <h3>Modern Feature</h3>
        <p class="muted">Buy/sell simulation updates the bank balance and creates a transaction entry automatically.</p>
      </div>
    </div>

    <div class="card" style="margin-top:18px">
      <h3>Your Investment Portfolio</h3>
      <div class="table-wrap" style="margin-top:14px"><table><thead><tr><th>Symbol</th><th>Name</th><th>Units</th><th>Buy Price</th><th>Current</th><th>Gain/Loss</th><th>Action</th></tr></thead><tbody>${rows || '<tr><td colspan="7">No investments yet.</td></tr>'}</tbody></table></div>
    </div>`;

  $("#buyInv").onclick = async () => {
    const sel = $("#invAsset");
    const opt = sel.options[sel.selectedIndex];
    try {
      const resp = await api("/api/investment/buy",{method:"POST",body:new URLSearchParams({
        accountNo:state.accountNo, symbol:opt.value, name:opt.dataset.name, units:$("#invUnits").value, price:opt.dataset.price
      })});
      await refreshAccount();
      toast(resp.message);
      investments();
    } catch(err){ toast(err.message,true); }
  };

  document.querySelectorAll(".sell-btn").forEach(btn => btn.onclick = async () => {
    const price = prompt("Enter simulated sell price:", "250");
    if (!price) return;
    try {
      const resp = await api("/api/investment/sell",{method:"POST",body:new URLSearchParams({accountNo:state.accountNo,investmentId:btn.dataset.id,sellPrice:price})});
      await refreshAccount();
      toast(resp.message);
      investments();
    } catch(err){ toast(err.message,true); }
  });
}

async function cards() {
  navActive("cards");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3><p class="muted">Please login first.</p></div>`;
    return;
  }

  await refreshAccount();
  const d = await api(`/api/cards?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
  const debit = d.debitCard;
  const credit = d.creditCard;

  const panel = (type, c, creditCard) => {
    const title = creditCard ? "Credit Card" : "Debit Card";
    if (!c.exists) {
      return `<div class="card card-${type}">
        <h3>${title}</h3>
        <p class="muted">No card linked to this account.</p>
        <button class="btn primary" onclick="createCard('${type}')">Generate ${title}</button>
      </div>`;
    }

    return `<div class="card card-${type}">
      <div class="card-head">
        <div>
          <h3>${title}</h3>
          <div class="masked-card">${escapeHtml(c.maskedNumber)}</div>
        </div>
        <span class="status ${c.blocked ? "warn" : "ok"}">${c.blocked ? "BLOCKED" : "ACTIVE"}</span>
      </div>

      <div class="grid grid2" style="margin-top:18px">
        <div class="card mini-card"><div class="muted">Online</div><strong>${c.onlineEnabled ? "ON" : "OFF"}</strong></div>
        <div class="card mini-card"><div class="muted">Contactless</div><strong>${c.contactlessEnabled ? "ON" : "OFF"}</strong></div>
        <div class="card mini-card"><div class="muted">ATM</div><strong>${c.atmEnabled ? "ON" : "OFF"}</strong></div>
        <div class="card mini-card"><div class="muted">International</div><strong>${c.internationalEnabled ? "ON" : "OFF"}</strong></div>
      </div>

      <div class="card mini-card">
        <div class="muted">Daily limit</div>
        <div class="metric" style="font-size:22px">${money(c.dailyLimit)}</div>
      </div>

      ${creditCard ? `
      <div class="grid grid2" style="margin-top:10px">
        <div class="card mini-card"><div class="muted">Credit limit</div><div class="metric" style="font-size:20px">${money(c.creditLimit)}</div></div>
        <div class="card mini-card"><div class="muted">Outstanding</div><div class="metric red" style="font-size:20px">${money(c.outstanding)}</div></div>
      </div>` : ""}

      <div class="actions">
        <button class="btn" onclick="toggleCard('${type}','online',${!c.onlineEnabled})">Online ${c.onlineEnabled ? "OFF" : "ON"}</button>
        <button class="btn" onclick="toggleCard('${type}','contactless',${!c.contactlessEnabled})">Contactless ${c.contactlessEnabled ? "OFF" : "ON"}</button>
        <button class="btn" onclick="toggleCard('${type}','atm',${!c.atmEnabled})">ATM ${c.atmEnabled ? "OFF" : "ON"}</button>
        <button class="btn" onclick="toggleCard('${type}','international',${!c.internationalEnabled})">International ${c.internationalEnabled ? "OFF" : "ON"}</button>
        <button class="btn" onclick="setCardLimit('${type}')">Set Limit</button>
        <button class="btn ${c.blocked ? "success" : "danger"}" onclick="blockCard('${type}',${!c.blocked})">${c.blocked ? "Unblock" : "Block"} Card</button>
      </div>

      ${creditCard ? `
      <div class="form" style="margin-top:18px">
        <div class="field"><label>Pay Credit Card Bill</label><input id="ccBillAmount" type="number" min="0.01" max="${c.outstanding}" step="0.01" placeholder="Amount"></div>
        <button id="payCcBill" class="btn success">Pay Outstanding</button>
      </div>` : ""}
    </div>`;
  };

  $("#content").innerHTML = `
    <div class="hero">
      <h1 style="font-size:24px">Card Centre</h1>
      <p>Manage debit and credit card access directly from Smart Bank.</p>
      <div class="code-note" style="margin-top:14px">
        Turn online, contactless, ATM or international usage on/off, set a daily limit,
        block/unblock the card instantly, and pay your credit-card bill.
      </div>
    </div>
    <div class="grid grid2">
      ${panel("debit", debit, false)}
      ${panel("credit", credit, true)}
    </div>`;

  const pay = $("#payCcBill");
  if (pay) {
    pay.onclick = async () => {
      try {
        const d = await api("/api/card/bill-payment",{
          method:"POST",
          body:new URLSearchParams({accountNo:state.accountNo, amount:$("#ccBillAmount").value})
        });
        await refreshAccount();
        toast(d.message);
        cards();
      } catch(err){ toast(err.message,true); }
    };
  }
}

async function createCard(type) {
  try {
    const d = await api(type === "debit" ? "/api/card/debit" : "/api/card/credit",{
      method:"POST",
      body:new URLSearchParams({accountNo:state.accountNo})
    });
    await refreshAccount();
    toast(d.message);
    cards();
  } catch(err){ toast(err.message,true); }
}

async function toggleCard(type, control, enabled) {
  try {
    const d = await api("/api/card/control",{
      method:"POST",
      body:new URLSearchParams({
        accountNo:state.accountNo,
        cardType:type,
        control,
        enabled:enabled ? "1" : "0"
      })
    });
    await refreshAccount();
    toast(d.message);
    cards();
  } catch(err){ toast(err.message,true); }
}

async function setCardLimit(type) {
  const limit = prompt("Enter new daily card limit (₹):", type === "credit" ? "50000" : "25000");
  if (!limit) return;
  try {
    const d = await api("/api/card/limit",{
      method:"POST",
      body:new URLSearchParams({accountNo:state.accountNo,cardType:type,limit})
    });
    await refreshAccount();
    toast(d.message);
    cards();
  } catch(err){ toast(err.message,true); }
}

async function blockCard(type, blocked) {
  try {
    const d = await api("/api/card/block",{
      method:"POST",
      body:new URLSearchParams({accountNo:state.accountNo,cardType:type,blocked:blocked ? "1" : "0"})
    });
    await refreshAccount();
    toast(d.message);
    cards();
  } catch(err){ toast(err.message,true); }
}

async function passbook() {
  navActive("passbook");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3></div>`;
    return;
  }
  const d = await api(`/api/transactions?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
  const a = state.account;
  const rows = d.transactions.map(t => `<tr><td>${escapeHtml(t.time)}</td><td>${escapeHtml(t.id)}</td><td>${escapeHtml(t.type)}</td><td>${escapeHtml(t.remark)}</td><td>${money(t.amount)}</td><td>${money(t.balanceAfter)}</td></tr>`).join("");
  $("#content").innerHTML = `
    <div class="hero">
      <h1 style="font-size:24px">Smart Bank e-Passbook</h1>
      <p>Account ${escapeHtml(a.accountNo)} • ${escapeHtml(a.name)} • Created ${escapeHtml(a.createdAt || "Recently")}</p>
      <div class="actions"><button class="btn primary" id="printPassbook">Print Statement</button><button class="btn" id="downloadPassbook">Download CSV</button></div>
    </div>
    <div class="card">
      <div class="table-wrap"><table id="passbookTable"><thead><tr><th>Date</th><th>Txn ID</th><th>Type</th><th>Description</th><th>Amount</th><th>Balance</th></tr></thead><tbody>${rows || '<tr><td colspan="6">No transactions found.</td></tr>'}</tbody></table></div>
    </div>`;

  $("#printPassbook").onclick = () => window.print();
  $("#downloadPassbook").onclick = () => {
    const header = ["Date","Transaction ID","Type","Description","Amount","Balance"];
    const data = d.transactions.map(t=>[t.time,t.id,t.type,t.remark,t.amount,t.balanceAfter]);
    const csv = [header,...data].map(r=>r.map(x=>`"${String(x).replaceAll('"','""')}"`).join(",")).join("\n");
    const blob = new Blob([csv],{type:"text/csv"});
    const url = URL.createObjectURL(blob);
    const aEl = document.createElement("a"); aEl.href=url; aEl.download=`SmartBank-Passbook-${state.accountNo}.csv`; aEl.click();
    URL.revokeObjectURL(url);
  };
}

function profile() {
  navActive("profile");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3></div>`;
    return;
  }
  const a = state.account;
  $("#content").innerHTML = `
    <div class="grid grid2">
      <div class="card">
        <h3>Account Identity</h3>
        <div class="profile-list">
          <div><span>Account Number</span><b>${escapeHtml(a.accountNo)}</b></div>
          <div><span>Account Created</span><b>${escapeHtml(a.createdAt || "Recently")}</b></div>
          <div><span>Account Type</span><b>${escapeHtml(a.type)}</b></div>
          <div><span>Status</span><b class="green">${a.frozen?"FROZEN":"ACTIVE"}</b></div>
        </div>
      </div>
      <div class="card">
        <h3>Update Contact Details</h3>
        <div class="form">
          <div class="field"><label>Mobile</label><input id="profileMobile" value="${escapeHtml(a.mobile)}"></div>
          <div class="field"><label>Email</label><input id="profileEmail" value="${escapeHtml(a.email)}"></div>
          <div class="field"><label>Address</label><input id="profileAddress" value="${escapeHtml(a.address)}"></div>
          <button id="saveProfile" class="btn primary">Update Account</button>
        </div>
      </div>
    </div>`;
  $("#saveProfile").onclick = async () => {
    try {
      const d = await api("/api/profile/update",{method:"POST",body:new URLSearchParams({
        accountNo:state.accountNo, mobile:$("#profileMobile").value, email:$("#profileEmail").value, address:$("#profileAddress").value
      })});
      await refreshAccount(); toast(d.message); profile();
    } catch(err){ toast(err.message,true); }
  };
}

function loans() {
  navActive("loans");
  $("#content").innerHTML = `
    <div class="card">
      <h3>Loan EMI Calculator</h3>
      <div class="grid grid2">
        <div class="field"><label>Principal (₹)</label><input id="loanP" type="number"></div>
        <div class="field"><label>Annual Rate (%)</label><input id="loanR" type="number" step="0.01"></div>
        <div class="field"><label>Tenure (months)</label><input id="loanN" type="number"></div>
      </div>
      <div class="actions"><button class="btn success" id="emiBtn">Calculate EMI</button></div>
      <div id="emiResult"></div>
    </div>`;

  $("#emiBtn").onclick = async () => {
    try {
      const d = await api("/api/emi",{
        method:"POST",
        body:new URLSearchParams({principal:$("#loanP").value,rate:$("#loanR").value,months:$("#loanN").value})
      });
      $("#emiResult").innerHTML = `<div class="hero" style="margin-top:18px"><h1>${money(d.emi)}</h1><p>Estimated monthly EMI</p></div>`;
    } catch(err){ toast(err.message,true); }
  };
}

function fd() {
  navActive("fd");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3></div>`;
    return;
  }

  $("#content").innerHTML = `
    <div class="card">
      <h3>Create Fixed Deposit</h3>
      <div class="grid grid2">
        <div class="field"><label>Amount (₹)</label><input id="fdAmount" type="number"></div>
        <div class="field"><label>Rate (%)</label><input id="fdRate" type="number" step="0.01"></div>
        <div class="field"><label>Tenure (months)</label><input id="fdMonths" type="number"></div>
      </div>
      <div class="actions"><button id="fdBtn" class="btn success">Create FD</button></div>
    </div>`;

  $("#fdBtn").onclick = async () => {
    try {
      const d = await api("/api/fd",{
        method:"POST",
        body:new URLSearchParams({
          accountNo:state.accountNo,
          amount:$("#fdAmount").value,
          rate:$("#fdRate").value,
          months:$("#fdMonths").value
        })
      });
      await refreshAccount();
      toast(d.message);
      dashboard();
    } catch(err){ toast(err.message,true); }
  };
}

async function admin() {
  navActive("admin");
  $("#content").innerHTML = `
    <div class="grid grid2">
      <div class="card">
        <h3>Admin Access</h3>
        <p class="muted">Default academic credentials:</p>
        <div class="code-note">Username: admin<br>Password: admin123</div>
        <div class="actions"><button id="adminBtn" class="btn primary">Load Statistics</button></div>
      </div>
      <div id="adminStats" class="card">
        <h3>System Statistics</h3>
        <p class="muted">Click Load Statistics.</p>
      </div>
    </div>`;

  $("#adminBtn").onclick = async () => {
    const data = await api("/api/admin/stats",{method:"GET"});
    $("#adminStats").innerHTML = `
      <h3>System Statistics</h3>
      <div class="metric">${data.accounts}</div><div class="muted">Total accounts</div>
      <div class="metric green">${money(data.balance)}</div><div class="muted">Total customer balances</div>
      <div class="metric">${data.pendingRequests}</div><div class="muted">Pending service requests</div>`;
  };
}


async function smart() {
  navActive("smart");
  if (!state.account) {
    $("#content").innerHTML = `<div class="card"><h3>Login Required</h3><p class="muted">Login first to use the Smart Features Lab.</p></div>`;
    return;
  }

  $("#content").innerHTML = `
    <div class="hero">
      <h1 style="font-size:25px">Smart Features Lab</h1>
      <p>Advanced banking simulations designed to demonstrate uncommon product ideas plus DSA concepts.</p>
    </div>

    <div class="grid grid2">
      <div class="card">
        <h3>1. Banking Digital Twin</h3>
        <p class="muted">Estimate your future balance using recent income and spending behavior.</p>
        <button id="twinBtn" class="btn primary">Run Digital Twin</button>
        <div id="twinOut" style="margin-top:14px"></div>
      </div>

      <div class="card">
        <h3>2. What‑If Finance Simulator</h3>
        <div class="grid grid2">
          <div class="field"><label>Extra Spend (₹)</label><input id="whatSpend" type="number" value="0"></div>
          <div class="field"><label>Extra Income (₹)</label><input id="whatIncome" type="number" value="0"></div>
        </div>
        <button id="whatBtn" class="btn success" style="margin-top:14px">Simulate</button>
        <div id="whatOut" style="margin-top:14px"></div>
      </div>

      <div class="card">
        <h3>3. Transaction Explainability</h3>
        <p class="muted">Get a human-readable reason for each balance change.</p>
        <button id="explainBtn" class="btn primary">Explain Transactions</button>
        <div id="explainOut" style="margin-top:14px"></div>
      </div>

      <div class="card">
        <h3>4. Financial Stress Indicator</h3>
        <p class="muted">Rule-based score from 0–100 based on balance and recent spending.</p>
        <button id="stressBtn" class="btn primary">Calculate Stress</button>
        <div id="stressOut" style="margin-top:14px"></div>
      </div>

      <div class="card">
        <h3>5. Emergency Banking Mode</h3>
        <p class="muted">Simulation control for rapidly restricting risky activity.</p>
        <div class="actions">
          <button id="emergencyOn" class="btn danger">Enable Emergency Mode</button>
          <button id="emergencyOff" class="btn success">Disable</button>
        </div>
        <div id="emergencyOut" style="margin-top:12px"></div>
      </div>

      <div class="card">
        <h3>6. Beneficiary Time‑Lock</h3>
        <div class="field"><label>Beneficiary Account</label><input id="benAcc"></div>
        <div class="field" style="margin-top:10px"><label>Cooling Period (minutes)</label><input id="benMins" type="number" value="5"></div>
        <div class="actions">
          <button id="benAdd" class="btn primary">Add Beneficiary</button>
          <button id="benStatus" class="btn">Check Status</button>
        </div>
        <div id="benOut" style="margin-top:12px"></div>
      </div>

      <div class="card">
        <h3>7. Subscription Detector</h3>
        <p class="muted">Find recurring subscription-like descriptions from transaction records.</p>
        <button id="subBtn" class="btn primary">Scan Transactions</button>
        <div id="subOut" style="margin-top:14px"></div>
      </div>

      <div class="card">
        <h3>8. Family Approval Wallet</h3>
        <div class="grid grid2">
          <div class="field"><label>Amount (₹)</label><input id="famAmount" type="number" value="1000"></div>
          <div class="field"><label>Purpose</label><input id="famPurpose" value="High-value transfer"></div>
        </div>
        <div class="actions">
          <button id="famReq" class="btn primary">Request Approval</button>
          <button id="famApprove" class="btn success">Approve</button>
        </div>
        <div id="famOut" style="margin-top:12px"></div>
      </div>

      <div class="card">
        <h3>9. Fraud Investigation Graph</h3>
        <p class="muted">Runs BFS on the simulated transfer network starting from your account.</p>
        <button id="fraudBtn" class="btn primary">Run BFS Analysis</button>
        <div id="fraudOut" style="margin-top:14px"></div>
      </div>

      <div class="card">
        <h3>10. Replay the Month</h3>
        <p class="muted">Replay transaction events chronologically like a timeline.</p>
        <button id="replayBtn" class="btn primary">Load Replay</button>
        <div id="replayOut" style="margin-top:14px"></div>
      </div>

      <div class="card">
        <h3>11. Programmable Card</h3>
        <p class="muted">Save a human-readable card rule for this demo session.</p>
        <div class="field"><label>Rule</label><input id="cardRule" value="Allow international payments only after 18:00"></div>
        <div class="actions">
          <button id="ruleSave" class="btn success">Save Rule</button>
          <button id="ruleLoad" class="btn">Load Rule</button>
        </div>
        <div id="ruleOut" style="margin-top:12px"></div>
      </div>

      <div class="card">
        <h3>12. Inactivity / Dead‑Man Workflow</h3>
        <p class="muted">Demo-only inactivity monitor for a future notification workflow.</p>
        <button id="inactiveBtn" class="btn primary">Check Inactivity</button>
        <div id="inactiveOut" style="margin-top:14px"></div>
      </div>
    </div>`;

  $("#twinBtn").onclick = async () => {
    try {
      const d = await api(`/api/smart/digital-twin?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      $("#twinOut").innerHTML = `
        <div class="grid grid2">
          <div class="card mini-card"><div class="muted">Current</div><div class="metric">${money(d.balance)}</div></div>
          <div class="card mini-card"><div class="muted">30-day</div><div class="metric green">${money(d.projected30)}</div></div>
          <div class="card mini-card"><div class="muted">90-day</div><div class="metric">${money(d.projected90)}</div></div>
          <div class="card mini-card"><div class="muted">Monthly Spend</div><div class="metric red">${money(d.monthlySpend)}</div></div>
        </div>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#whatBtn").onclick = async () => {
    try {
      const d = await api("/api/smart/what-if",{
        method:"POST",
        body:new URLSearchParams({
          accountNo:state.accountNo,
          extraSpend:$("#whatSpend").value,
          extraIncome:$("#whatIncome").value
        })
      });
      $("#whatOut").innerHTML = `<div class="code-note">Projected balance: <b>${money(d.after)}</b><br>Risk level: <b class="${d.risk==='HIGH'?'red':d.risk==='MEDIUM'?'yellow':'green'}">${d.risk}</b></div>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#explainBtn").onclick = async () => {
    try {
      const d = await api(`/api/smart/explain?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      $("#explainOut").innerHTML = d.items.slice(0,6).map(x=>`<div class="code-note" style="margin-bottom:8px"><b>${escapeHtml(x.type)}</b> • ${money(x.amount)}<br>${escapeHtml(x.explanation)}</div>`).join("") || "<span class='muted'>No transactions.</span>";
    } catch(err){ toast(err.message,true); }
  };

  $("#stressBtn").onclick = async () => {
    try {
      const d = await api(`/api/smart/stress?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      const label = d.score >= 70 ? "HIGH" : d.score >= 40 ? "MEDIUM" : "LOW";
      $("#stressOut").innerHTML = `<div class="metric ${label==='HIGH'?'red':label==='MEDIUM'?'yellow':'green'}">${d.score.toFixed(1)} / 100</div><div class="muted">${label} financial stress</div>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#emergencyOn").onclick = async () => {
    try {
      const d = await api("/api/smart/emergency",{method:"POST",body:new URLSearchParams({accountNo:state.accountNo,enabled:"1"})});
      $("#emergencyOut").innerHTML = `<span class="status warn">EMERGENCY MODE ON</span><p class="muted">${escapeHtml(d.message)}</p>`;
    } catch(err){ toast(err.message,true); }
  };
  $("#emergencyOff").onclick = async () => {
    try {
      const d = await api("/api/smart/emergency",{method:"POST",body:new URLSearchParams({accountNo:state.accountNo,enabled:"0"})});
      $("#emergencyOut").innerHTML = `<span class="status ok">NORMAL MODE</span><p class="muted">${escapeHtml(d.message)}</p>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#benAdd").onclick = async () => {
    try {
      const d = await api("/api/smart/beneficiary/add",{method:"POST",body:new URLSearchParams({
        accountNo:state.accountNo, beneficiary:$("#benAcc").value, coolingMinutes:$("#benMins").value
      })});
      toast(d.message);
    } catch(err){ toast(err.message,true); }
  };
  $("#benStatus").onclick = async () => {
    try {
      const d = await api(`/api/smart/beneficiary/status?accountNo=${encodeURIComponent(state.accountNo)}&beneficiary=${encodeURIComponent($("#benAcc").value)}`,{method:"GET"});
      $("#benOut").innerHTML = `<div class="code-note">Status: <b>${d.status}</b><br>Remaining: ${d.remainingSeconds || 0} seconds</div>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#subBtn").onclick = async () => {
    try {
      const d = await api(`/api/smart/subscriptions?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      $("#subOut").innerHTML = d.patterns.length
        ? d.patterns.map(x=>`<div class="code-note">${escapeHtml(x.pattern)} — ${x.count} match(es)</div>`).join("")
        : `<span class="muted">No subscription-like patterns found in current data.</span>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#famReq").onclick = async () => {
    try {
      const d = await api("/api/smart/family/request",{method:"POST",body:new URLSearchParams({
        accountNo:state.accountNo, amount:$("#famAmount").value, purpose:$("#famPurpose").value
      })});
      $("#famOut").innerHTML = `<span class="status warn">PENDING</span> ${escapeHtml(d.message)}`;
    } catch(err){ toast(err.message,true); }
  };
  $("#famApprove").onclick = async () => {
    try {
      const d = await api("/api/smart/family/approve",{method:"POST",body:new URLSearchParams({accountNo:state.accountNo})});
      $("#famOut").innerHTML = `<span class="status ok">APPROVED</span> ${escapeHtml(d.message)}`;
    } catch(err){ toast(err.message,true); }
  };

  $("#fraudBtn").onclick = async () => {
    try {
      const d = await api(`/api/smart/fraud?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      $("#fraudOut").innerHTML = `<div class="code-note">BFS path:<br><b>${d.bfs.map(escapeHtml).join(" → ") || state.accountNo}</b></div>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#replayBtn").onclick = async () => {
    try {
      const d = await api(`/api/smart/replay?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      $("#replayOut").innerHTML = d.events.slice(-8).map(x=>`<div class="code-note" style="margin-bottom:7px"><b>Step ${x.step}</b> • ${escapeHtml(x.type)} • ${money(x.amount)} • Balance ${money(x.balanceAfter)}</div>`).join("") || "<span class='muted'>No events.</span>";
    } catch(err){ toast(err.message,true); }
  };

  $("#ruleSave").onclick = async () => {
    try {
      const d = await api("/api/smart/card-rule",{method:"POST",body:new URLSearchParams({
        accountNo:state.accountNo, rule:$("#cardRule").value
      })});
      $("#ruleOut").innerHTML = `<span class="status ok">SAVED</span> ${escapeHtml(d.message)}`;
    } catch(err){ toast(err.message,true); }
  };
  $("#ruleLoad").onclick = async () => {
    try {
      const d = await api(`/api/smart/card-rule?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      $("#ruleOut").innerHTML = `<div class="code-note">${escapeHtml(d.rule || "No rule saved.")}</div>`;
    } catch(err){ toast(err.message,true); }
  };

  $("#inactiveBtn").onclick = async () => {
    try {
      const d = await api(`/api/smart/inactivity?accountNo=${encodeURIComponent(state.accountNo)}`,{method:"GET"});
      const cls = d.status==="INACTIVE" ? "red" : d.status==="WATCH" ? "yellow" : "green";
      $("#inactiveOut").innerHTML = `<div class="metric ${cls}">${escapeHtml(d.status)}</div><div class="muted">${d.daysInactive} day(s) since last recorded transaction.</div>`;
    } catch(err){ toast(err.message,true); }
  };
}

async function show(page) {
  await refreshAccount();
  ({
    dashboard,
    accounts,
    deposit:()=>transactionForm("deposit"),
    withdraw:()=>transactionForm("withdraw"),
    transfer,
    transactions,
    qr,
    cards,
    loans,
    fd,
    investments,
    passbook,
    profile,
    smart,
    admin
  }[page] || dashboard)();
}

document.querySelectorAll(".nav").forEach(b => b.onclick = () => show(b.dataset.page));

show("dashboard");
