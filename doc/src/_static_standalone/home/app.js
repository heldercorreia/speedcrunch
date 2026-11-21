/* SpeedCrunch home page (doc/src/_templates_standalone/home.html) */
(() => {
  "use strict";

  const SCHEMES = window.SC_SCHEMES || [];
  const BY_NAME = Object.fromEntries(SCHEMES.map((s) => [s.name, s]));
  const KEYS = ["background", "number", "unit", "function", "variable", "operator", "parens", "list", "separator", "comment", "result"];
  const FEATURED = ["Tokyo Night", "One Dark", "Catppuccin Macchiato", "Dracula", "Nord Dark", "Gruvbox Dark", "Monokai", "Tomorrow Night Blue", "Duskfox", "Solarized Light", "GitHub Light", "Rosé Pine Dawn"];
  // The hero demo picks these dark schemes in turn, starting from Tokyo Night.
  // Light ones stay in the picker only.
  const THEME_CYCLE = ["Tokyo Night", "Gruvbox Dark", "Catppuccin Macchiato", "One Dark", "Tomorrow Night Blue", "Duskfox"];
  const root = document.documentElement;
  const $ = (sel, el = document) => el.querySelector(sel);
  const $$ = (sel, el = document) => [...el.querySelectorAll(sel)];

  /* ------------------------------------------------------------ Highlighting */

  const esc = (s) => s.replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

  // A small approximation of SpeedCrunch's syntax highlighter.
  function tokenize(src) {
    const out = [];
    let i = 0, afterArrow = false;
    while (i < src.length) {
      const rest = src.slice(i);
      let m;
      if (rest[0] === "?") { out.push(["comment", rest]); break; }
      if ((m = rest.match(/^\s+/))) { out.push(["ws", m[0]]); i += m[0].length; continue; }
      if (afterArrow && (m = rest.match(/^[A-Za-zµμΩ°][A-Za-z0-9µμΩ°/·⁰¹²³⁴⁵⁶⁷⁸⁹⁻]*/))) {
        out.push(["unit", m[0]]); i += m[0].length; afterArrow = false; continue;
      }
      afterArrow = rest[0] === "→";
      if (rest[0] === "[") {
        const j = rest.indexOf("]");
        const v = j < 0 ? rest : rest.slice(0, j + 1);
        out.push(["unit", v]); i += v.length; continue;
      }
      if ((m = rest.match(/^(0x[0-9A-Fa-f]+|0b[01]+|0o[0-7]+|\d+(\.\d+)?([eE][+\-−]?\d+)?[ij]?|[⁰¹²³⁴⁵⁶⁷⁸⁹⁻]+)/))) {
        out.push(["number", m[0]]); i += m[0].length; continue;
      }
      if ((m = rest.match(/^[A-Za-zπ_][A-Za-z0-9_]*/))) {
        const after = src.slice(i + m[0].length);
        const type = /^\s*\(/.test(after) ? "function" : m[0] === "i" || m[0] === "j" ? "number" : "variable";
        out.push([type, m[0]]); i += m[0].length; continue;
      }
      const c = rest[0];
      if (c === "√" || c === "∑") out.push(["function", c]);
      else if (c === "(" || c === ")") out.push(["parens", c]);
      else if (c === "{" || c === "}") out.push(["list", c]);
      else if (c === ";") out.push(["separator", c]);
      else out.push(["operator", c]);
      i++;
    }
    return out;
  }

  const highlight = (src) => tokenize(src).map(([t, v]) => (t === "ws" ? v : `<span class="k-${t}">${esc(v)}</span>`)).join("");
  // As in the app, "=" takes the operator color and the value the result color.
  const resultLine = (r) => `<div class="h-line k-result"><span class="k-operator">=</span>\u00a0${esc(r)}</div>`;
  const entryHTML = (input, result) => `<div class="h-line">${highlight(input)}</div>${result ? resultLine(result) : ""}`;

  /* ------------------------------------------------------------ Color schemes */

  // A port of how SpeedCrunch derives its window colors from a scheme's
  // background (src/gui/oklchutils.cpp, and generatedSurfaceColorsForScheme in
  // src/gui/mainwindow.cpp): six OKLCH shades, a generated primary (accent)
  // color, and readable text for each shade.
  const Surfaces = (() => {
    const M1 = [[0.4122214708, 0.5363325363, 0.0514459929], [0.2119034982, 0.6806995451, 0.1073969566], [0.0883024619, 0.2817188376, 0.6299787005]];
    const M2 = [[0.2104542553, 0.7936177850, -0.0040720468], [1.9779984951, -2.4285922050, 0.4505937099], [0.0259040371, 0.7827717662, -0.8086757660]];
    const M3 = [[1, 0.3963377774, 0.2158037573], [1, -0.1055613458, -0.0638541728], [1, -0.0894841775, -1.2914855480]];
    const M4 = [[4.0767416621, -3.3077115913, 0.2309699292], [-1.2684380046, 2.6097574011, -0.3413193965], [-0.0041960863, -0.7034186147, 1.7076147010]];
    const mul = (m, v) => m.map((r) => r[0] * v[0] + r[1] * v[1] + r[2] * v[2]);
    const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
    const toLinear = (c) => (c <= 0.04045 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4);
    const toGamma = (c) => (c <= 0.0031308 ? 12.92 * c : 1.055 * c ** (1 / 2.4) - 0.055);
    const rgb = (hex) => { const n = parseInt(hex.slice(1), 16); return [(n >> 16) & 255, (n >> 8) & 255, n & 255].map((v) => v / 255); };
    const hexFromLinear = (v) => "#" + v.map((x) => Math.round(clamp(toGamma(clamp(x, 0, 1)), 0, 1) * 255).toString(16).padStart(2, "0")).join("");

    function oklch(hex) {
      const [l, a, b] = mul(M2, mul(M1, rgb(hex).map(toLinear)).map(Math.cbrt));
      const c = Math.hypot(a, b);
      return { l, c, h: c < 1e-12 ? 0 : ((Math.atan2(b, a) * 180) / Math.PI + 360) % 360 };
    }
    function linear({ l, c, h }) {
      const r = (h * Math.PI) / 180;
      return mul(M4, mul(M3, [l, c * Math.cos(r), c * Math.sin(r)]).map((x) => x * x * x));
    }
    const inGamut = (v) => v.every((x) => x >= -1e-7 && x <= 1 + 1e-7);
    // Keep hue and lightness; lower chroma until the color fits in sRGB.
    function fit(o) {
      const cand = { l: clamp(o.l, 0, 1), c: Math.max(0, o.c), h: ((o.h % 360) + 360) % 360 };
      if (inGamut(linear(cand))) return cand;
      let lo = 0, hi = cand.c;
      for (let i = 0; i < 32; i++) {
        const mid = (lo + hi) / 2;
        if (inGamut(linear({ ...cand, c: mid }))) lo = mid; else hi = mid;
      }
      return { ...cand, c: lo };
    }
    const hex = (o) => hexFromLinear(linear(fit(o)));
    const luminance = (h) => { const [r, g, b] = rgb(h).map(toLinear); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
    const contrast = (a, b) => { const x = luminance(a), y = luminance(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
    const blackOrWhite = (bg) => (contrast(bg, "#000000") >= contrast(bg, "#ffffff") ? "#000000" : "#ffffff");

    // Shade 100 (window), 200 (results, the scheme background), 300 (editor),
    // 400 (selected tab), 500 and 600.
    function shades(bg) {
      const a = oklch(bg), dark = a.l < 0.5, f = 0.09;
      return [0, 1, 2, 3, 4, 5].map((i) => {
        let l;
        if (i === 0) l = dark ? a.l * (1 - f) : a.l + (1 - a.l) * f;
        else if (i === 1) l = a.l;
        else { const p = clamp(f * (i - 1), 0, 1); l = dark ? a.l + (1 - a.l) * p : a.l * (1 - p); }
        const chroma = i === 0 ? 0.5 : i === 1 ? 1 : 0.95 ** (i - 1);
        return hex({ l: clamp(l, 0, 1), c: a.c * chroma, h: a.h });
      });
    }

    function primary(bg) {
      const a = oklch(bg), dark = a.l < 0.5;
      const p = {
        l: dark ? 0.74 : 0.52,
        c: dark ? clamp(Math.max(a.c * 2.5, 0.1), 0.1, 0.18) : clamp(Math.max(a.c * 2, 0.09), 0.09, 0.16),
        h: a.c < 0.02 ? 250 : a.h,
      };
      const cand = hex(p);
      if (contrast(cand, bg) >= 4.5) return cand;
      const limit = dark ? 0.86 : 0.36;
      const limitCand = hex({ ...p, l: limit });
      const target = contrast(limitCand, bg) >= 4.5 ? 4.5 : 3;
      if (contrast(limitCand, bg) >= target) {
        let fail = p.l, pass = limit, found = limitCand;
        for (let i = 0; i < 32; i++) {
          const l = (fail + pass) / 2, c = hex({ ...p, l });
          if (contrast(c, bg) >= target) { pass = l; found = c; } else fail = l;
        }
        return found;
      }
      const q = { ...p, l: limit };
      let best = limitCand, bestContrast = contrast(best, bg);
      for (let i = 0; i < 8; i++) {
        q.c *= 0.85;
        const c = hex(q), k = contrast(c, bg);
        if (k > bestContrast) { bestContrast = k; best = c; }
        if (k >= 3) return c;
      }
      if (bestContrast >= 3) return best;
      return blackOrWhite(bg) === "#ffffff" ? "#f5f5f5" : "#111111";
    }

    // Text tinted with the surface's own hue, at a contrast of at least 10:1.
    function foreground(bg, min = 10) {
      const tint = oklch(bg), fallback = blackOrWhite(bg);
      if (tint.c < 0.015) return fallback;
      const fitted = fit(tint), first = hexFromLinear(linear(fitted));
      if (fitted.c >= 0.015 && contrast(first, bg) >= min) return first;
      let fail = tint.l, pass = fallback === "#ffffff" ? 1 : 0, found = null, foundTint = tint;
      for (let i = 0; i < 32; i++) {
        const t = fit({ ...tint, l: (fail + pass) / 2 }), c = hexFromLinear(linear(t));
        if (contrast(c, bg) >= min) { pass = t.l; found = c; foundTint = t; } else fail = (fail + pass) / 2;
      }
      return found && foundTint.c >= 0.015 ? found : fallback;
    }

    return { shades, primary, foreground };
  })();

  let current = "Tokyo Night";

  // CSS variables for a scheme: its colors, the app's generated shades, text
  // for shades 100, 300 and 400, and the accent with text for it.
  function schemeVars(name) {
    const s = BY_NAME[name];
    const vars = {};
    KEYS.forEach((k) => { vars[`--sc-${k}`] = s[k]; });
    const sh = Surfaces.shades(s.background);
    sh.forEach((c, i) => { vars[`--sh-${i}`] = c; });
    [0, 2, 3].forEach((i) => { vars[`--fg-${i}`] = Surfaces.foreground(sh[i]); });
    const primary = s.primary || Surfaces.primary(s.background);
    vars["--primary"] = primary;
    vars["--on-primary"] = Surfaces.foreground(primary);
    return vars;
  }

  const setVars = (el, vars) => Object.entries(vars).forEach(([k, v]) => el.style.setProperty(k, v));

  // The page-wide scheme: the hero demo and the feature snippets.
  function applyScheme(name) {
    if (!BY_NAME[name]) return;
    current = name;
    setVars(root, schemeVars(name));
    $("#scheme-name").textContent = name;
    $$(".swatch").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.scheme === name)));
  }

  function swatchStyle(s) {
    return `background:${s.background};--a:${s.function};--b:${s.result};--c:${s.number}`;
  }

  function buildPicker() {
    const box = $("#swatches");
    box.innerHTML = FEATURED.filter((n) => BY_NAME[n]).map((n) =>
      `<button type="button" class="swatch" data-scheme="${esc(n)}" title="${esc(n)}" aria-label="${esc(n)}" style="${swatchStyle(BY_NAME[n])}"><i></i></button>`
    ).join("");
    box.addEventListener("click", (e) => {
      const b = e.target.closest(".swatch");
      if (!b) return;
      userPickedTheme = true;
      applyScheme(b.dataset.scheme);
    });
  }

  /* ------------------------------------------------------------ Live session demo */

  // Three renamed sessions. Science and Electro start as tabs of the left pane,
  // and Math has the right pane. Entries without "out" are definitions, which
  // the app shows without a result line. The demo types the final text, as the
  // editor's automatic input adjustments (superscripts, operators, spacing) make
  // it look while typing. Results come from real SpeedCrunch sessions.
  const SESSIONS = {
    science: {
      name: "Science",
      prefill: [
        { in: "g0 = 9.80665 [m/s²] ? standard gravity", out: "9.80665 m·s⁻²" },
        { in: "√(2 × 100 [m] / g0) ? fall time", out: "4.51600755751787519652 s" },
        { in: "v = 60 [mi/h]", out: "26.8224 m·s⁻¹" },
        { in: "v → [km/h]", out: "96.56064 km·h⁻¹" },
        { in: "molmass(C6H12O6) ? glucose", out: "180.156 g·mol⁻¹" },
        { in: "period(L) = 2 · π · √(L / g0) ? pendulum" },
        { in: "period(1 [m])", out: "2.0064092925890404509 s" },
      ],
      cycle: [
        { in: "1000 [kg/m³] · g0 · 10 [m] → [atm]", out: "0.96784110535405872193 atm" },
        { in: "1e51 [erg] → [J] ? supernova energy", out: "1 × 10⁴⁴ J" },
        { in: "period(25 [cm])", out: "1.00320464629452022545 s" },
        { in: "0.5 [G] → [µT] ? Earth's field", out: "50 µT" },
        { in: "14 [st] → [kg] ? stone", out: "88.90410452 kg" },
      ],
    },
    electro: {
      name: "Electro",
      prefill: [
        { in: "12 [V] × 2.5 [A] → [W]", out: "30 W" },
        { in: "10 · log10(1 [W] / 1 [mW]) ? 1 W in dBm", out: "30" },
        { in: "230 · √(2) ? peak of 230 V RMS", out: "325.26911934581186122439" },
        { in: "hex(255)", out: "0xFF" },
      ],
      cycle: [
        { in: "20 · log10(10 [V] / 1 [V]) ? voltage gain", out: "20" },
        { in: "3.7 [V] × 2000 [mA·h] → [W·h]", out: "7.4 W·h" },
        { in: "hex(ieee754_single_encode(1.5))", out: "0x3FC00000" },
        { in: "299792458 [m/s] / 2.4 [GHz] → [mm]", out: "124.91352416666666666667 mm" },
        { in: "2000 [mA·h] → [C]", out: "7200 C" },
        { in: "bin(0xA5)", out: "0b10100101" },
      ],
    },
    math: {
      name: "Math",
      prefill: [
        { in: "hyp(x; y) = √(x² + y²)" },
        { in: "hyp(3; 4)", out: "5" },
        { in: "A = {{2; 1}; {1; 3}}", out: "{{2; 1}; {1; 3}}" },
        { in: "det(A)", out: "5" },
        { in: "inv(A)", out: "{{0.6; −0.2}; {−0.2; 0.4}}" },
      ],
      cycle: [
        { in: "A · inv(A)", out: "{{1; 0}; {0; 1}}" },
        { in: "{{1; 2}; {3; 4}} · {{5; 6}; {7; 8}}", out: "{{19; 22}; {43; 50}}" },
        { in: "transpose({{1; 2; 3}; {4; 5; 6}})", out: "{{1; 4}; {2; 5}; {3; 6}}" },
        { in: "2¹⁰⁰", out: "1267650600228229401496703205376" },
        { in: "{1; 2; 3} · {{1}; {2}; {3}}", out: "14" },
        { in: "cross({1; 0; 0}; {0; 1; 0})", out: "{0; 0; 1}" },
        { in: "trace({{1; 2}; {3; 4}})", out: "5" },
        { in: "exp(i · π)", out: "−1" },
      ],
    },
  };

  const appwin = $("#appwin");
  const dragLayer = $(".drag-layer", appwin);
  const overlay = $(".drop-overlay", appwin);
  const heroDemo = $(".hero-demo");
  const cursorEl = $(".cursor-layer .cursor");
  const panes = [];

  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const nextFrame = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
  let visible = true;
  const waitVisible = async () => { while (!visible || document.hidden) await sleep(250); };

  // A pane is a tab bar, the history of its current session, and a Classic
  // input area.
  function createPane(col, tabs, current, before = null) {
    const el = document.createElement("div");
    el.className = "pane";
    el.innerHTML = '<div class="pane-tabs"></div><div class="pane-body"><div class="pane-history"></div>' +
      '<div class="pane-editor"><span class="ed-text"></span><span class="caret"></span></div></div>';
    col.insertBefore(el, before);
    const pane = {
      el, tabsEl: $(".pane-tabs", el), body: $(".pane-body", el), hist: $(".pane-history", el),
      editor: $(".pane-editor", el), edText: $(".ed-text", el), tabs: [...tabs], current,
    };
    panes.push(pane);
    appwin.dataset.panes = panes.length;
    scroller.observe(pane.hist);
    renderPane(pane);
    return pane;
  }

  function removePane(pane) {
    pane.el.remove();
    panes.splice(panes.indexOf(pane), 1);
    appwin.dataset.panes = panes.length;
  }

  // As in the app, only the current tab (or a hovered one) shows its close button.
  function renderPane(pane) {
    pane.tabsEl.innerHTML = pane.tabs.map((id) =>
      `<span class="tab${id === pane.current ? " selected" : ""}" data-id="${id}"><span>${esc(SESSIONS[id].name)}</span><i>×</i></span>`
    ).join("");
    const list = SESSIONS[pane.current].list;
    if (list.parentElement !== pane.hist) pane.hist.replaceChildren(list);
    pane.hist.scrollTop = pane.hist.scrollHeight;
  }

  // Like the app's result display, a history starts at the top and, once it
  // is full, stays scrolled to its last entry.
  const scroller = new ResizeObserver((entries) => entries.forEach(({ target }) => {
    const hist = target.classList.contains("h-list") ? target.parentElement : target;
    if (hist) hist.scrollTop = hist.scrollHeight;
  }));

  const paneOf = (id) => panes.find((p) => p.tabs.includes(id));
  const tabOf = (id) => $(`.tab[data-id="${id}"]`, paneOf(id).tabsEl);
  const shown = (pane) => pane.el.offsetParent !== null;

  // The active pane gets the accent line under its current tab and above its
  // input area.
  let activePane = null;
  function setActive(pane) {
    activePane = pane;
    panes.forEach((p) => p.el.toggleAttribute("data-active", p === pane));
  }

  const trim = (list) => { while (list.children.length > 14) list.firstElementChild.remove(); };

  function addEntry(list, e) {
    const div = document.createElement("div");
    div.className = "h-entry";
    div.innerHTML = `<div><div class="h-gap"></div>${entryHTML(e.in, e.out)}</div>`;
    list.append(div);
    trim(list);
  }

  // Type the next expression of a session in its pane's input area, then
  // press Enter: the expression and its result join the history.
  async function typeNext(id) {
    const s = SESSIONS[id], pane = paneOf(id);
    const e = s.cycle[s.n];
    s.n = (s.n + 1) % s.cycle.length;
    if (!shown(pane)) return;
    await waitVisible();
    setActive(pane);
    await sleep(600);
    pane.editor.classList.add("typing");
    for (let i = 1; i <= e.in.length; i++) {
      await waitVisible();
      pane.edText.innerHTML = highlight(e.in.slice(0, i));
      await sleep(38 + Math.random() * 55);
    }
    await sleep(650);
    pane.edText.innerHTML = "";
    pane.editor.classList.remove("typing");
    const div = document.createElement("div");
    div.className = "h-entry entering";
    div.innerHTML = `<div><div class="h-gap"></div>${entryHTML(e.in, e.out)}</div>`;
    s.list.append(div);
    trim(s.list);
    await nextFrame();
    div.classList.remove("entering");
    await sleep(2200);
    await maybePickTheme();
  }

  /* Mouse pointer */

  const cur = { x: 0, y: 0 };
  const rel = (el) => {
    const a = heroDemo.getBoundingClientRect(), r = el.getBoundingClientRect();
    return { left: r.left - a.left, top: r.top - a.top, right: r.right - a.left, bottom: r.bottom - a.top, width: r.width, height: r.height };
  };
  const inside = (r, x, y) => x >= r.left && x < r.right && y >= r.top && y < r.bottom;
  const placeCursor = () => { cursorEl.style.transform = `translate(${cur.x}px, ${cur.y}px)`; };
  const setShape = (shape) => { cursorEl.dataset.shape = shape; };

  async function showCursorIn(el, fx, fy) {
    const r = rel(el);
    cur.x = r.left + r.width * fx;
    cur.y = r.top + r.height * fy;
    placeCursor();
    setShape("arrow");
    cursorEl.classList.add("on");
    await sleep(300);
  }

  async function hideCursor() {
    setShape("arrow");
    await moveCursor(cur.x + 34, cur.y + 46, 450);
    cursorEl.classList.remove("on");
    await sleep(450);
  }

  // Move along a slight curve, calling onFrame with each new position.
  async function moveCursor(x, y, ms, onFrame) {
    await waitVisible();
    const sx = cur.x, sy = cur.y, dx = x - sx, dy = y - sy;
    const t0 = performance.now();
    await new Promise((done) => {
      const step = (now) => {
        const t = Math.min(1, (now - t0) / ms);
        const e = t < 0.5 ? 4 * t * t * t : 1 - (-2 * t + 2) ** 3 / 2;
        const arc = Math.sin(Math.PI * e) * 0.08;
        cur.x = sx + dx * e - dy * arc;
        cur.y = sy + dy * e + dx * arc;
        placeCursor();
        if (onFrame) onFrame(cur.x, cur.y);
        if (t < 1) requestAnimationFrame(step); else done();
      };
      requestAnimationFrame(step);
    });
  }

  const pressFeedback = () => cursorEl.animate([{ scale: 1 }, { scale: 0.84 }, { scale: 1 }], { duration: 220, easing: "ease-out" });

  // Point at a tab, hover it, and press it: the pane switches to that session
  // and becomes the active pane. Returns the press point.
  async function pressTab(id) {
    const pane = paneOf(id);
    const t = rel(tabOf(id));
    const px = t.left + t.width * 0.4, py = t.top + t.height * 0.55;
    await moveCursor(px, py, 850);
    const tab = tabOf(id);
    if (!tab.classList.contains("selected")) tab.classList.add("hovered");
    setShape("hand");
    await sleep(380);
    pressFeedback();
    pane.current = id;
    renderPane(pane);
    setActive(pane);
    return { px, py, t: rel(tabOf(id)) };
  }

  // Every 4 or 5 entries the pointer picks the next color scheme in the
  // cycle, until the visitor picks one themselves.
  let userPickedTheme = false;
  // The first change comes after 2 or 3 entries, the later ones after 4 or 5.
  let themeCountdown = 2 + Math.round(Math.random());
  async function maybePickTheme() {
    if (userPickedTheme || --themeCountdown > 0) return;
    themeCountdown = 4 + Math.round(Math.random());
    const next = THEME_CYCLE[(THEME_CYCLE.indexOf(current) + 1) % THEME_CYCLE.length];
    const swatch = $(`.swatch[data-scheme="${next}"]`);
    if (!swatch || !activePane) return;
    await showCursorIn(activePane.body, 0.6, 0.6);
    const r = rel(swatch);
    await moveCursor(r.left + r.width / 2, r.top + r.height * 0.55, 1000);
    swatch.classList.add("hovered");
    setShape("hand");
    await sleep(380);
    pressFeedback();
    if (!userPickedTheme) applyScheme(next);
    await sleep(500);
    swatch.classList.remove("hovered");
    await hideCursor();
  }

  async function clickTab(id) {
    const pane = paneOf(id);
    if (!shown(pane)) return;
    await showCursorIn(pane.body, 0.62, 0.5);
    await pressTab(id);
    await sleep(600);
    await hideCursor();
  }

  /* Dragging tabs, as in SpeedCrunch's SessionTabBar and SessionPane */

  // The drag image is a grab of the tab's rectangle, strip color included.
  function makeGhost(id, hotX, hotY) {
    const g = document.createElement("div");
    g.className = "drag-ghost";
    g.append(tabOf(id).cloneNode(true));
    g.style.display = "none";
    g.hot = { x: hotX + 2, y: hotY + 3 };
    dragLayer.append(g);
    return g;
  }
  const moveGhost = (g, x, y) => { g.style.transform = `translate(${x - g.hot.x}px, ${y - g.hot.y}px)`; };

  // The zone whose anchor (center or the middle of an edge) is nearest wins.
  function zoneAt(r, x, y) {
    const cx = r.left + r.width / 2, cy = r.top + r.height / 2;
    const anchors = [["center", cx, cy], ["top", cx, r.top], ["bottom", cx, r.bottom], ["left", r.left, cy], ["right", r.right, cy]];
    let zone = "center", best = Infinity;
    anchors.forEach(([name, ax, ay]) => {
      const d = (x - ax) ** 2 + (y - ay) ** 2;
      if (d < best) { best = d; zone = name; }
    });
    return zone;
  }

  // A dimmed overlay over the pane area that the drop would fill.
  function showZone(pane, x, y) {
    if (!pane) { overlay.style.display = "none"; return; }
    const r = rel(pane.body);
    const hw = Math.floor(r.width / 2), hh = Math.floor(r.height / 2);
    const box = {
      center: [r.left, r.top, r.width, r.height],
      top: [r.left, r.top, r.width, hh],
      bottom: [r.left, r.top + hh, r.width, r.height - hh],
      left: [r.left, r.top, hw, r.height],
      right: [r.left + hw, r.top, r.width - hw, r.height],
    }[zoneAt(r, x, y)];
    Object.assign(overlay.style, { display: "block", left: `${box[0]}px`, top: `${box[1]}px`, width: `${box[2]}px`, height: `${box[3]}px` });
  }


  // The drag starts once the pointer leaves the tab bar by Qt's start-drag
  // distance (above or below it), and then lasts until the drop.
  function dragStarter(bar) {
    let started = false;
    return (y) => (started = started || y < bar.top - 10 || y > bar.bottom + 10);
  }

  // Drag a tab into its own pane, to the half named by `zone` ("right" or
  // "bottom"). Dropping it there splits the pane; place() creates the new pane
  // for the session.
  async function dragToSplit(id, zone, place) {
    const pane = paneOf(id);
    if (!shown(pane)) return;
    await showCursorIn(pane.body, 0.7, 0.4);
    const { px, py, t } = await pressTab(id);
    await sleep(320);
    const ghost = makeGhost(id, px - t.left, py - t.top);
    const dragging = dragStarter(rel(pane.tabsEl));
    const onFrame = (x, y) => {
      const on = dragging(y);
      ghost.style.display = on ? "" : "none";
      moveGhost(ghost, x, y);
      showZone(on && inside(rel(pane.body), x, y) ? pane : null, x, y);
    };
    setShape("arrow");
    const b = rel(pane.body);
    if (zone === "right") {
      await moveCursor(b.left + b.width * 0.55, b.top + b.height * 0.42, 750, onFrame);
      await moveCursor(b.left + b.width * 0.9, b.top + b.height * 0.5, 750, onFrame);
    } else {
      await moveCursor(px + 16, b.top + b.height * 0.45, 650, onFrame);
      await moveCursor(px + 28, b.top + b.height * 0.86, 750, onFrame);
    }
    await sleep(500);
    ghost.remove();
    showZone(null);
    pane.tabs = pane.tabs.filter((x) => x !== id);
    if (pane.current === id) pane.current = pane.tabs[0];
    renderPane(pane);
    setActive(place(pane));
    await sleep(450);
    await hideCursor();
  }

  // Drag a tab from its own pane to the middle of another pane, as in the app:
  // the whole target pane dims, and dropping there moves the session into it
  // as its current tab. The empty pane closes.
  async function dragToMerge(id, target) {
    const pane = paneOf(id);
    if (!shown(pane)) return;
    await showCursorIn(pane.body, 0.6, 0.5);
    const { px, py, t } = await pressTab(id);
    await sleep(320);
    const ghost = makeGhost(id, px - t.left, py - t.top);
    const dragging = dragStarter(rel(pane.tabsEl));
    const onFrame = (x, y) => {
      const on = dragging(y);
      ghost.style.display = on ? "" : "none";
      moveGhost(ghost, x, y);
      // The source pane shows no zone: it only holds this one session.
      showZone(on && inside(rel(target.body), x, y) ? target : null, x, y);
    };
    setShape("arrow");
    const b = rel(target.body);
    await moveCursor(b.left + b.width / 2, b.top + b.height / 2, 1100, onFrame);
    await sleep(500);
    ghost.remove();
    showZone(null);
    removePane(pane);
    target.tabs.push(id);
    target.current = id;
    renderPane(target);
    setActive(target);
    await sleep(450);
    await hideCursor();
  }

  async function runDemo() {
    Object.values(SESSIONS).forEach((s) => {
      s.n = 0;
      s.list = document.createElement("div");
      s.list.className = "h-list";
      s.prefill.forEach((e) => addEntry(s.list, e));
      scroller.observe(s.list);
    });
    // All three sessions start as tabs of one pane.
    const left = createPane($("#col-left"), ["science", "electro", "math"], "science");
    setActive(left);
    // Math goes to a new pane on the right (below, on phones, where the right
    // column stacks under the left one). Electro splits the left pane.
    const mathSide = () => (matchMedia("(max-width: 560px)").matches ? "bottom" : "right");
    const toRightColumn = () => createPane($("#col-right"), ["math"], "math");
    const belowSource = (pane) => createPane(pane.el.parentElement, ["electro"], "electro", pane.el.nextSibling);
    await sleep(1400);
    for (;;) {
      await typeNext("science");
      await dragToSplit("math", mathSide(), toRightColumn);
      await typeNext("math");
      await clickTab("electro");
      await typeNext("electro");
      await dragToSplit("electro", "bottom", belowSource);
      for (const id of ["science", "electro", "math", "science", "electro", "math"]) await typeNext(id);
      await dragToMerge("electro", left);
      await typeNext("math");
      await dragToMerge("math", left);
      await typeNext("math");
      await clickTab("science");
    }
  }

  /* ------------------------------------------------------------ Field examples */

  // Result displays for the field tabs, from real SpeedCrunch sessions
  // (the macOS screenshots); Programming adds integer and IEEE 754 examples.
  const FIELD_DISPLAYS = {
    physics: [
      { in: "qe = 1.602176634e-19 [C] ? elementary charge", out: "160.2176634 zC" },
      { in: "kB = 1.380649e-23 [J/K] ? Boltzmann constant", out: "1.380649 × 10⁻²³ J·K⁻¹" },
      { in: "eps0 = 8.8541878188e-12 [F/m] ? vacuum permittivity", out: "8.8541878188 × 10⁻¹² F·m⁻¹" },
      { in: "mu0 = 1.25663706127e-6 [N/A²] ? vacuum permeability", out: "0.00000125663706127 H·m⁻¹" },
      { in: "1 / √(eps0 · mu0) → m/s ? gives c back", out: "299792457.99982110615633801078 m·s⁻¹" },
      { in: "g0 = 9.80665 [m/s²] ? standard gravity", out: "9.80665 m·s⁻²" },
      { in: "√(2 × 100 [m] / g0) ? fall time from 100 m", out: "4.51600755751787519652 s" },
      { in: "period(L) = 2 · π · √(L / g0) ? pendulum" },
      { in: "period(1 [m])", out: "2.0064092925890404509 s" },
      { in: "period(25 [cm])", out: "1.00320464629452022545 s" },
    ],
    chemistry: [
      { in: "mass(0.1 [mol];KMnO4)", out: "15.8032 g" },
      { in: "10 [g] / molmass(NaCl) → [mol]", out: "0.17111567419575633128 mol" },
      { in: "1 [kg] / molmass(H2O) ? moles in 1 kg of water", out: "55.50929780738273660838 mol" },
      { in: "NA = 6.02214076e23 [mol⁻¹] ? Avogadro constant", out: "6.02214076 × 10²³ mol⁻¹" },
      { in: "1 [mg] / molmass(C8H10N4O2) · NA ? caffeine molecules in 1 mg", out: "3.10109517286836874466 × 10¹⁸" },
      { in: "? pH and equilibria" },
      { in: "pH(c) = −log10(c)" },
      { in: "14 − pH(0.001) ? pH of 0.001 M NaOH", out: "11" },
      { in: "Ka = 1.8e-5 ? acetic acid", out: "0.000018" },
      { in: "√(Ka · 0.1) ? [H+] in 0.1 M acetic acid", out: "0.00134164078649987382" },
    ],
    electronics: [
      { in: "? AC impedance with complex numbers" },
      { in: "w = 2 · π · 50 ? 50 Hz mains", out: "314.15926535897932384626" },
      { in: "ZR = 100", out: "100" },
      { in: "ZL = j · w · 0.5 ? 0.5 H", out: "157.07963267948966192313i" },
      { in: "ZC = 1 / (j · w · 10e-6) ? 10 µF", out: "−318.30988618379067153777i" },
      { in: "Z = ZR + ZL + ZC ? series RLC", out: "100 − 161.23025350430100961464i" },
      { in: "abs(Z)", out: "189.72399596535270106203" },
      { in: "? Decibels" },
      { in: "20 · log10(0.5)", out: "−6.02059991327962390427" },
      { in: "10^(−3 / 20) ? -3 dB as a voltage ratio", out: "0.7079457843841379108" },
      { in: "1 [mW] · (10^(20 / 10)) → [mW] ? +20 dBm", out: "100 mW" },
    ],
    cgs: [
      { in: "? Magnetism" },
      { in: "3 [T] → [kG] ? MRI scanner", out: "30 kG" },
      { in: "13.2 [kG] → [T] ? N52 magnet remanence", out: "1.32 T" },
      { in: "12 [kOe] → [kA/m] ? NdFeB coercivity", out: "954.9296585513720146133 kA·m⁻¹" },
      { in: "4 · π · 1e-7 [N/A²] × 1 [Oe] → [G] ? B = µ0·H in vacuum", out: "1 G" },
      { in: "? Viscosity" },
      { in: "1.002 [cP] → [mPa·s] ? water at 20 °C", out: "1.002 mPa·s" },
      { in: "46 [cSt] → [mm²/s] ? ISO VG 46 hydraulic oil", out: "46 mm²·s⁻¹" },
      { in: "6 · π · 1 [P] × 0.1 [cm] × 1 [cm/s] → [dyn] ? Stokes drag on a sphere", out: "1.88495559215387594308 dyn" },
      { in: "? Gravimetry" },
      { in: "9.80665 [m/s²] → [Gal] ? standard gravity", out: "980.665 Gal" },
    ],
    programming: [
      { in: "0b10101100", out: "172" },
      { in: "dec(0xBEEF)", out: "48879" },
      { in: "hex(xor(0xFF;0x0F)) ? XOR", out: "0xF0" },
      { in: "0x80 >> 3", out: "16" },
      { in: "bin(shl(1;5)|shl(1;3)) ? set bits 5 and 3 of a register", out: "0b101000" },
      { in: "popcount(0xFF)", out: "8" },
      { in: "ieee754_single_decode(0x40533333) ? 3.3 is not exact as a float", out: "3.29999995231628417969" },
      { in: "hex(ieee754_single_encode(3.3))", out: "0x40533333" },
    ],
  };

  function setupFieldExamples() {
    $$(".field[data-scheme]").forEach((field) => setVars(field, schemeVars(field.dataset.scheme)));
    $$(".rd[data-display]").forEach((el) => {
      el.innerHTML = FIELD_DISPLAYS[el.dataset.display]
        .map((e) => `<div class="h-entry"><div><div class="h-gap"></div>${entryHTML(e.in, e.out)}</div></div>`)
        .join("");
    });
    $$(".bitfield").forEach(setupBitfield);
  }

  const SUPERSCRIPT = "⁰¹²³⁴⁵⁶⁷⁸⁹";
  const sup = (n) => String(n).replace(/\d/g, (d) => SUPERSCRIPT[d]);

  // The Bitfield dock: 64 bits of the last result, in nibble boxes, two bytes
  // per row. As in the app, a click flips a bit, and the buttons clear,
  // invert and shift all of them.
  function setupBitfield(el) {
    const MASK = (1n << 64n) - 1n;
    let value = BigInt(el.dataset.value);
    let rows = "";
    for (let row = 0; row < 4; row++) {
      rows += '<div class="bf-row">';
      for (let byte = 0; byte < 2; byte++) {
        rows += '<div class="bf-byte">';
        for (let nibble = 0; nibble < 2; nibble++) {
          rows += '<div class="bf-nibble">';
          for (let k = 0; k < 4; k++) {
            const n = 63 - row * 16 - byte * 8 - nibble * 4 - k;
            rows += `<button type="button" class="bit" data-bit="${n}" title="2${sup(n)} = ${1n << BigInt(n)}">${n}</button>`;
          }
          rows += "</div>";
        }
        rows += "</div>";
      }
      rows += "</div>";
    }
    el.innerHTML = `<div class="bf-field">${rows}</div><div class="bf-buttons">` +
      '<button type="button" data-op="clear">0</button><button type="button" data-op="invert">~</button>' +
      '<button type="button" data-op="shl">&lt;&lt;</button><button type="button" data-op="shr">&gt;&gt;</button></div>';
    const bits = $$(".bit", el);
    const render = () => bits.forEach((b) => b.setAttribute("aria-pressed", String(((value >> BigInt(b.dataset.bit)) & 1n) === 1n)));
    el.addEventListener("click", (e) => {
      const bit = e.target.closest(".bit"), op = e.target.closest("[data-op]");
      if (bit) value ^= 1n << BigInt(bit.dataset.bit);
      else if (op) value = { clear: 0n, invert: ~value & MASK, shl: (value << 1n) & MASK, shr: value >> 1n }[op.dataset.op];
      else return;
      render();
    });
    render();
  }

  // Zoom from an element on the page to the full image in an overlay. With a
  // crop, the cropped part starts exactly over the element and the rest of the
  // window grows out around it.
  function openZoom(fromEl, src, iw, ih, crop) {
    // The (translated) texts come from the page.
    const { zoomClose, zoomHint } = $("#stage").dataset;
    const ov = document.createElement("div");
    ov.className = "zoom";
    ov.innerHTML = `<div class="zoom-backdrop"></div><img class="zoom-img" alt="" src="${esc(src)}">` +
      `<button class="zoom-close" type="button" aria-label="${esc(zoomClose)}"><i class="fa-solid fa-xmark" aria-hidden="true"></i></button>` +
      `<p class="zoom-hint">${esc(zoomHint)}</p>`;
    document.body.append(ov);
    document.body.classList.add("noscroll");
    const img = $(".zoom-img", ov);
    const backdrop = $(".zoom-backdrop", ov);

    const k = Math.min((innerWidth * 0.94) / iw, (innerHeight * 0.86) / ih);
    const fw = iw * k, fh = ih * k;
    const fx = (innerWidth - fw) / 2, fy = (innerHeight - fh) / 2;
    Object.assign(img.style, { left: `${fx}px`, top: `${fy}px`, width: `${fw}px`, height: `${fh}px` });

    const startFrame = () => {
      const L = fromEl.getBoundingClientRect();
      const [cx, cy, cw, ch] = crop || [0, 0, iw, ih];
      const s = L.width / (cw * k);
      const tx = L.left - fx - s * cx * k;
      const ty = L.top - fy - s * cy * k;
      const r = 12 / s;
      return {
        transform: `translate(${tx}px, ${ty}px) scale(${s})`,
        clipPath: `inset(${cy * k}px ${fw - (cx + cw) * k}px ${fh - (cy + ch) * k}px ${cx * k}px round ${r}px)`,
      };
    };
    const endFrame = { transform: "translate(0px, 0px) scale(1)", clipPath: `inset(0px 0px 0px 0px round ${10}px)` };
    const timing = { duration: 560, easing: "cubic-bezier(0.2, 0.8, 0.2, 1)", fill: "both" };

    img.animate([startFrame(), endFrame], timing);
    backdrop.animate([{ opacity: 0 }, { opacity: 1 }], timing);

    let closing = false;
    const close = () => {
      if (closing) return;
      closing = true;
      const a = img.animate([endFrame, startFrame()], { ...timing, duration: 460 });
      backdrop.animate([{ opacity: 1 }, { opacity: 0 }], { ...timing, duration: 460 });
      a.finished.then(() => {
        ov.remove();
        document.body.classList.remove("noscroll");
        document.removeEventListener("keydown", onKey);
      });
    };
    const onKey = (e) => { if (e.key === "Escape") close(); };
    ov.addEventListener("click", close);
    document.addEventListener("keydown", onKey);
    $(".zoom-close", ov).focus({ preventScroll: true });
  }

  /* ------------------------------------------------------------ Auto-advancing tabs */

  // Field tabs and the layout gallery move to their next item on a timer.
  const FIELD_INTERVAL = 8000;
  const LAYOUT_INTERVAL = 6000;

  // Selects the next item on a timer and shows the progress on the active one
  // (--p, from 0 to 1). Returns restart(): call it when the user chooses an
  // item, so that item gets its full interval before the automation carries
  // on. The timer waits while the pointer is over `list` (where a click is
  // about to happen), while keyboard focus is in it, while a screenshot is
  // zoomed, and while `box` is off screen, that is, outside the middle half of
  // the window (a share of the box itself would fail for boxes taller than
  // the window).
  function autoAdvance({ box, list, items, interval, active, next }) {
    let elapsed = 0, hovered = false, onScreen = false, last = performance.now();
    list.addEventListener("pointerenter", (e) => { if (e.pointerType === "mouse") hovered = true; });
    list.addEventListener("pointerleave", () => { hovered = false; });
    new IntersectionObserver(([e]) => { onScreen = e.isIntersecting; }, { rootMargin: "-25% 0px -25% 0px" }).observe(box);
    const tick = (now) => {
      const dt = Math.min(100, now - last);
      last = now;
      const waiting = hovered || !onScreen || document.hidden || $(".zoom") || list.querySelector(":focus-visible");
      if (!waiting) {
        elapsed += dt;
        if (elapsed >= interval) { elapsed = 0; next(); }
      }
      items[active()].style.setProperty("--p", (elapsed / interval).toFixed(4));
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
    return () => { elapsed = 0; };
  }

  // On narrow screens a tab list scrolls sideways; keep the active item in view.
  function keepInView(list, item) {
    if (list.scrollWidth <= list.clientWidth) return;
    const l = list.getBoundingClientRect(), r = item.getBoundingClientRect();
    list.scrollTo({ left: list.scrollLeft + r.left - l.left - (l.width - r.width) / 2, behavior: "smooth" });
  }

  function setupFieldTabs() {
    const list = $("#field-list");
    const tabs = $$('[role="tab"]', list);
    let active = 0;
    const select = (i) => {
      active = i;
      tabs.forEach((t, j) => {
        const on = j === i;
        t.setAttribute("aria-selected", String(on));
        t.tabIndex = on ? 0 : -1;
        t.style.removeProperty("--p");
        $("#" + t.getAttribute("aria-controls")).hidden = !on;
      });
      keepInView(list, tabs[i]);
    };
    const restart = autoAdvance({
      box: $("#fields .wrap"), list, items: tabs, interval: FIELD_INTERVAL,
      active: () => active, next: () => select((active + 1) % tabs.length),
    });
    tabs.forEach((t, i) => {
      t.addEventListener("click", () => { select(i); restart(); });
      t.addEventListener("keydown", (e) => {
        const d = e.key === "ArrowDown" || e.key === "ArrowRight" ? 1 : e.key === "ArrowUp" || e.key === "ArrowLeft" ? -1 : 0;
        if (d) e.preventDefault();
        if (!d) return;
        const n = (i + d + tabs.length) % tabs.length;
        select(n);
        restart();
        tabs[n].focus();
      });
    });
  }

  function setupLayouts() {
    const list = $("#layout-list");
    const buttons = $$("button", list);
    const imgs = $$("#stage img");
    let active = 0;
    const show = (i) => {
      active = i;
      buttons.forEach((b, j) => {
        b.setAttribute("aria-pressed", String(j === i));
        b.style.removeProperty("--p");
      });
      imgs.forEach((im, j) => im.classList.toggle("active", j === i));
      keepInView(list, buttons[i]);
    };
    const restart = autoAdvance({
      box: $(".layouts"), list, items: buttons, interval: LAYOUT_INTERVAL,
      active: () => active, next: () => show((active + 1) % buttons.length),
    });
    buttons.forEach((b, i) => b.addEventListener("click", () => { show(i); restart(); }));
    const zoomActive = () => {
      const im = imgs[active];
      // The image box letterboxes the picture; zoom from the picture itself.
      const box = im.getBoundingClientRect();
      const k = Math.min(box.width / im.naturalWidth, box.height / im.naturalHeight);
      const w = im.naturalWidth * k, h = im.naturalHeight * k;
      const rect = { left: box.left + (box.width - w) / 2, top: box.top + (box.height - h) / 2, width: w, height: h };
      openZoom({ getBoundingClientRect: () => rect }, im.currentSrc || im.src, im.naturalWidth, im.naturalHeight, null);
    };
    imgs.forEach((im) => im.addEventListener("click", zoomActive));
    $(".stage-zoom").addEventListener("click", zoomActive);
  }

  /* ------------------------------------------------------------ Download button */

  // The page has the (translated) texts for each system in data attributes.
  function setupDownload() {
    const p = ((navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || navigator.userAgent).toLowerCase();
    const os = p.includes("mac") ? "macos" : p.includes("win") ? "windows" : p.includes("linux") || p.includes("x11") ? "linux" : null;
    if (!os) return;
    const main = $("#dl-main"), other = $("#dl-other");
    $("span", main).textContent = main.dataset[os];
    other.textContent = other.dataset[os];
  }

  /* ------------------------------------------------------------ Start */

  buildPicker();
  applyScheme(current);
  setupFieldExamples();
  setupFieldTabs();
  setupLayouts();
  setupDownload();
  new IntersectionObserver(([e]) => { visible = e.isIntersecting; }).observe(appwin);
  runDemo();
})();
