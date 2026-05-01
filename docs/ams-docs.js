(function () {
  var root = document.documentElement;
  var THEME_KEY = "ams-docs-theme";
  var searchInput = document.getElementById("doc-search");
  var searchResults = document.getElementById("search-results");
  var themeBtn = document.getElementById("theme-toggle");

  function currentPath() {
    var p = window.location.pathname.replace(/\/+/g, "/");
    if (p.endsWith("/")) return p + "index.md";
    if (!p.endsWith(".md")) return p + "/index.md";
    return p;
  }

  function normalize(path) {
    return path.replace(/\/+/g, "/").replace(/\/$/, "");
  }

  function setTheme(theme) {
    root.setAttribute("data-theme", theme);
    if (themeBtn) themeBtn.textContent = theme === "dark" ? "Light" : "Dark";
    localStorage.setItem(THEME_KEY, theme);
  }

  function initTheme() {
    var stored = localStorage.getItem(THEME_KEY);
    if (stored === "dark" || stored === "light") {
      setTheme(stored);
      return;
    }
    var prefersDark = window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches;
    setTheme(prefersDark ? "dark" : "light");
  }

  function activateSidebarLink() {
    var path = normalize(currentPath());
    var links = document.querySelectorAll(".sidebar a");
    links.forEach(function (a) {
      var href = a.getAttribute("href");
      if (!href) return;
      var url = new URL(href, window.location.origin);
      var target = normalize(url.pathname.endsWith("/") ? url.pathname + "index.md" : url.pathname);
      if (target === path) a.classList.add("active");
    });
  }

  function getBasePrefix() {
    var parts = window.location.pathname.split("/").filter(Boolean);
    var mdIndex = parts.findIndex(function (p) { return p.endsWith(".md"); });
    if (mdIndex === -1) return "/";
    return "/" + parts.slice(0, mdIndex).join("/") + "/";
  }

  function pageIndex() {
    var base = getBasePrefix().replace(/\/+/g, "/");
    var isEn = /\/en\//.test(window.location.pathname);
    if (isEn) {
      return [
        { t: "Home", u: base + "en/index.md" },
        { t: "Quick Start", u: base + "en/01-quick-start.md" },
        { t: "Build and Run", u: base + "en/02-build-and-run.md" },
        { t: "System Architecture", u: base + "en/03-system-architecture.md" },
        { t: "Userspace, ELF and Syscalls", u: base + "en/04-userspace-elf-and-syscalls.md" },
        { t: "Applications and Tools", u: base + "en/05-apps-and-tools.md" },
        { t: "Debugging", u: base + "en/06-debugging.md" },
        { t: "GitHub Pages", u: base + "en/07-github-pages.md" },
        { t: "Boot Sequence", u: base + "en/08-boot-sequence-step-by-step.md" },
        { t: "Memory", u: base + "en/09-memory-step-by-step.md" },
        { t: "VFS and ELF", u: base + "en/10-vfs-ext2-elf.md" },
        { t: "Interrupts and Syscall Path", u: base + "en/11-interrupts-and-syscall-path.md" },
        { t: "Glossary", u: base + "en/12-glossary.md" },
        { t: "Getting Started", u: base + "en/13-getting-started.md" },
        { t: "Architecture Overview", u: base + "en/14-architecture-overview.md" },
        { t: "Internals", u: base + "en/15-internals.md" },
        { t: "Operations", u: base + "en/16-operations.md" },
        { t: "Contributing", u: base + "en/17-contributing.md" },
        { t: "Release Notes", u: base + "en/18-release-notes.md" }
      ];
    }
    return [
      { t: "Home", u: base + "index.md" },
      { t: "Szybki Start", u: base + "01-szybki-start.md" },
      { t: "Budowanie i uruchamianie", u: base + "02-budowanie-i-uruchamianie.md" },
      { t: "Architektura systemu", u: base + "03-architektura-systemu.md" },
      { t: "Użytkownik, ELF i syscalle", u: base + "04-uzytkownik-i-syscalls.md" },
      { t: "Aplikacje i narzędzia", u: base + "05-aplikacje-i-narzedzia.md" },
      { t: "Debugowanie i testy", u: base + "06-debugowanie.md" },
      { t: "GitHub Pages i publikacja", u: base + "07-github-pages.md" },
      { t: "Sekwencja bootowania", u: base + "08-sekwencja-bootowania.md" },
      { t: "Pamięć krok po kroku", u: base + "09-pamiec-krok-po-kroku.md" },
      { t: "VFS, EXT2 i ELF", u: base + "10-vfs-ext2-i-elf.md" },
      { t: "Przerwania i syscall path", u: base + "11-przerwania-i-syscall.md" },
      { t: "Glosariusz", u: base + "12-glosariusz.md" },
      { t: "Getting Started", u: base + "13-getting-started.md" },
      { t: "Architecture Overview", u: base + "14-architecture-overview.md" },
      { t: "Internals", u: base + "15-internals.md" },
      { t: "Operations", u: base + "16-operations.md" },
      { t: "Contributing", u: base + "17-contributing.md" },
      { t: "Release Notes", u: base + "18-release-notes.md" }
    ];
  }

  function initSearch() {
    if (!searchInput || !searchResults) return;
    var pages = pageIndex();
    searchInput.addEventListener("input", function () {
      var q = searchInput.value.trim().toLowerCase();
      if (!q) {
        searchResults.hidden = true;
        searchResults.innerHTML = "";
        return;
      }
      var hits = pages.filter(function (p) { return p.t.toLowerCase().indexOf(q) >= 0; }).slice(0, 8);
      if (!hits.length) {
        searchResults.hidden = false;
        searchResults.innerHTML = '<div class="search-empty">No results</div>';
        return;
      }
      searchResults.hidden = false;
      searchResults.innerHTML = hits
        .map(function (h) {
          return '<a class="search-item" href="' + h.u + '">' + h.t + "</a>";
        })
        .join("");
    });
    document.addEventListener("click", function (e) {
      if (!searchResults.contains(e.target) && e.target !== searchInput) {
        searchResults.hidden = true;
      }
    });
  }

  initTheme();
  activateSidebarLink();
  initSearch();

  if (themeBtn) {
    themeBtn.addEventListener("click", function () {
      var current = root.getAttribute("data-theme") || "light";
      setTheme(current === "light" ? "dark" : "light");
    });
  }
})();
