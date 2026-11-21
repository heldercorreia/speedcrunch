/* SpeedCrunch website: the top bar's light/dark/system toggle, and opening and
   closing its menus (the menu on narrow windows and the manual's chapters). A button with aria-controls
   opens the element after it (see site.css). It listens on the document,
   so it works before the page has loaded. */
(() => {
  "use strict";

  const BUTTONS = "#navbar [aria-controls]";
  const setOpen = (button, open) => button.setAttribute("aria-expanded", String(open));
  const openButtons = () => document.querySelectorAll('#navbar [aria-controls][aria-expanded="true"]');
  const closeAll = () => openButtons().forEach((b) => setOpen(b, false));

  // Light, dark or system mode (not on the home page). layout.html sets
  // data-theme-choice and data-theme in <head>; the toggle goes light, dark,
  // system, light..., remembers light and dark for the next pages, and
  // forgets them for system. With system, the page follows the system's
  // changes at once. (The home page has no data-theme and stays as it is.)
  const root = document.documentElement;
  const NEXT = { light: "dark", dark: "system", system: "light" };
  const systemDark = window.matchMedia && matchMedia("(prefers-color-scheme: dark)");
  const applyTheme = () => {
    const choice = root.dataset.themeChoice;
    root.dataset.theme = choice !== "system" ? choice : systemDark && systemDark.matches ? "dark" : "light";
    document.querySelectorAll(".site-theme-toggle").forEach((b) => {
      const label = b.dataset["label" + choice[0].toUpperCase() + choice.slice(1)];
      b.setAttribute("aria-label", label);
      b.title = label;
    });
  };
  document.addEventListener("DOMContentLoaded", () => { if (root.dataset.themeChoice) applyTheme(); });
  if (systemDark && systemDark.addEventListener) {
    systemDark.addEventListener("change", () => {
      if (root.dataset.themeChoice === "system") applyTheme();
    });
  }

  document.addEventListener("click", (e) => {
    if (e.target.closest(".site-theme-toggle")) {
      const choice = NEXT[root.dataset.themeChoice] || "light";
      root.dataset.themeChoice = choice;
      try {
        if (choice === "system") localStorage.removeItem("sc-theme");
        else localStorage.setItem("sc-theme", choice);
      } catch (err) {}
      applyTheme();
      return;
    }
    const button = e.target.closest(BUTTONS);
    if (button) {
      setOpen(button, button.getAttribute("aria-expanded") !== "true");
    } else if (!e.target.closest("#navbar") || e.target.closest("#navbar a")) {
      // A click outside the bar, or on one of its links, closes the menus.
      closeAll();
    }
  });

  document.addEventListener("keydown", (e) => {
    if (e.key !== "Escape") return;
    const open = [...openButtons()];
    if (!open.length) return;
    closeAll();
    open[open.length - 1].focus();
  });
})();
