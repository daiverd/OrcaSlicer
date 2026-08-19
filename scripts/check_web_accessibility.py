"""Static accessibility checker for resources/web (HTML + JS-templated HTML).

Limitations: heuristic only, no DOM/CSS awareness, no JS execution; the JS-string-
literal scan cannot resolve values built at runtime or reliably match nested tags of
the same name; "keyboard path" only checks a11y.js is *included*, not that a matching
role/handler actually reaches the element.
"""
import argparse
import re
import sys
from html.parser import HTMLParser
from pathlib import Path

VOID_TAGS = {"img", "br", "hr", "input", "meta", "link", "area", "base", "col", "embed", "source", "track", "wbr"}
ACTIONABLE_TAGS = {"div", "span", "img", "a"}
ACTIONABLE_ROLES = {"button", "tab", "menuitem", "option", "link"}
LABEL_ATTRS = ("aria-label", "aria-labelledby", "label-tid")

SKIP_DIR_PREFIXES = ("include/swiper/", "include/viewer/", "include/xterm/", "guide/swiper/", "elegoolink/")
SKIP_FILES = {"homepage/test.html", "include/jquery-2.1.1.min.js", "include/jquery-3.6.0.min.js", "include/json2.js"}

_keyboard_dir_cache = {}


def print_error(msg):
    print(f"\033[91m[ERROR]\033[0m {msg}")


def print_info(msg):
    print(f"\033[94m[INFO]\033[0m {msg}")


def is_skipped(path: Path, root: Path) -> bool:
    rel = path.relative_to(root).as_posix()
    return rel in SKIP_FILES or any(rel.startswith(p) for p in SKIP_DIR_PREFIXES)


def is_actionable(tag, attrs):
    if tag not in ACTIONABLE_TAGS:
        return False
    if tag == "a" and "href" in attrs:
        return False
    if attrs.get("aria-hidden", "").lower() == "true":
        return False
    return "onclick" in attrs or attrs.get("role", "").lower() in ACTIONABLE_ROLES


def has_label(attrs):
    return any(a in attrs for a in LABEL_ATTRS)


def img_alt_ok(attrs, ancestor_has_label):
    return ("alt" in attrs or attrs.get("aria-hidden", "").lower() == "true"
            or attrs.get("role", "").lower() == "presentation" or has_label(attrs) or ancestor_has_label)


def dir_has_keyboard_path(directory: Path, root: Path):
    """A sibling .html in this dir tree pulls in a11y.js, or a sibling .js has a keydown handler."""
    if directory not in _keyboard_dir_cache:
        found = False
        for f in list(directory.rglob("*.html")) + list(directory.rglob("*.js")):
            if is_skipped(f, root):
                continue
            try:
                content = f.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            if "a11y.js" in content or "keydown" in content:
                found = True
                break
        _keyboard_dir_cache[directory] = found
    return _keyboard_dir_cache[directory]


def keyboard_path_ok(path: Path, text: str, root: Path):
    return "a11y.js" in text or dir_has_keyboard_path(path.parent, root)


class HTMLA11yChecker(HTMLParser):
    """Structural checks (a, b, d, e) for real HTML files."""

    def __init__(self, keyboard_ok):
        super().__init__(convert_charrefs=True)
        self.keyboard_ok = keyboard_ok
        self.stack = []  # frames: tag, attrs, line, text
        self.findings = []  # (line, message)

    def _ancestor_has_label(self):
        return any(has_label(f["attrs"]) for f in self.stack)

    def _open_checks(self, tag, attrs, line):
        if tag == "img" and not img_alt_ok(attrs, self._ancestor_has_label()):
            self.findings.append((line, "<img> missing alt attribute (add alt=\"\" if decorative)"))
        if is_actionable(tag, attrs):
            if "tabindex" not in attrs:
                self.findings.append((line, f"<{tag}> is actionable (onclick/role) but has no tabindex"))
            if "onkeydown" not in attrs and not self.keyboard_ok:
                self.findings.append((line, f"<{tag}> is actionable but page has no keyboard path (a11y.js/onkeydown/keydown handler)"))

    def _close_checks(self, tag, attrs, line, text):
        if is_actionable(tag, attrs) and not text.strip() and not has_label(attrs):
            self.findings.append((line, f"<{tag}> looks icon-only but has no aria-label/aria-labelledby/label-tid"))

    def _handle_open(self, tag, attrs, closed):
        line = self.getpos()[0]
        attrs = {k: (v if v is not None else "") for k, v in attrs}
        self._open_checks(tag, attrs, line)
        if closed or tag in VOID_TAGS:
            self._close_checks(tag, attrs, line, "")
        else:
            self.stack.append({"tag": tag, "attrs": attrs, "line": line, "text": ""})

    def handle_starttag(self, tag, attrs):
        self._handle_open(tag, attrs, False)

    def handle_startendtag(self, tag, attrs):
        self._handle_open(tag, attrs, True)

    def handle_data(self, data):
        for frame in self.stack:
            frame["text"] += data

    def handle_endtag(self, tag):
        idx = next((i for i in range(len(self.stack) - 1, -1, -1) if self.stack[i]["tag"] == tag), None)
        if idx is None:
            return
        for frame in self.stack[idx:]:
            self._close_checks(frame["tag"], frame["attrs"], frame["line"], frame["text"])
        del self.stack[idx:]

    def finish(self):
        for frame in self.stack:
            self._close_checks(frame["tag"], frame["attrs"], frame["line"], frame["text"])


def parse_attrs(s):
    attrs = {}
    for m in re.finditer(r"""([a-zA-Z_:][-\w:.]*)\s*(?:=\s*(?:"([^"]*)"|'([^']*)'))?""", s):
        key = m.group(1).lower()
        attrs[key] = m.group(2) if m.group(2) is not None else (m.group(3) or "")
    return attrs


def line_of(text, idx):
    return text.count("\n", 0, idx) + 1


def scan_js_templates(text, keyboard_ok):
    """Heuristic regex pass (checks a, b, d, e) over '<div'/'<img'/'<span'/'<a' literals in JS."""
    findings = []
    if "<img" in text:
        for m in re.finditer(r"<img\b([^>]*?)/?>", text, re.IGNORECASE):
            attrs = parse_attrs(m.group(1))
            line = line_of(text, m.start())
            if not img_alt_ok(attrs, False):
                findings.append((line, "<img> (JS template) missing alt attribute"))
            if is_actionable("img", attrs):
                if "tabindex" not in attrs:
                    findings.append((line, "<img> (JS template) is actionable but has no tabindex"))
                if "onkeydown" not in attrs and not keyboard_ok:
                    findings.append((line, "<img> (JS template) is actionable but page has no keyboard path"))
                if not has_label(attrs):
                    findings.append((line, "<img> (JS template) looks icon-only but has no aria-label/label-tid"))
    if any(t in text for t in ("<div", "<span", "<a ", "<a>")):
        for m in re.finditer(r"<(div|span|a)\b([^>]*?)>(.*?)</\1>", text, re.IGNORECASE | re.DOTALL):
            tag = m.group(1).lower()
            attrs = parse_attrs(m.group(2))
            line = line_of(text, m.start())
            if not is_actionable(tag, attrs):
                continue
            if "tabindex" not in attrs:
                findings.append((line, f"<{tag}> (JS template) is actionable but has no tabindex"))
            if "onkeydown" not in attrs and not keyboard_ok:
                findings.append((line, f"<{tag}> (JS template) is actionable but page has no keyboard path"))
            inner_text = re.sub(r"<[^>]*>", "", m.group(3)).strip()
            if not inner_text and not has_label(attrs):
                findings.append((line, f"<{tag}> (JS template) looks icon-only but has no aria-label/label-tid"))
    return findings


def strip_js_comments(text):
    """Blank out // and /* */ comments (keeping newlines/offsets) so dead code in
    comments isn't mistaken for live code. Quote-tracking is shared with HTML attribute
    quoting, so it also leaves things like href="http://..." alone."""
    out = list(text)
    i, n, in_string = 0, len(text), None
    while i < n:
        c = text[i]
        if in_string:
            if c == "\\" and i + 1 < n:
                i += 2
                continue
            if c == in_string:
                in_string = None
            i += 1
            continue
        if c in ("\"", "'", "`"):
            in_string = c
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j == -1 else j
            for k in range(i, j):
                out[k] = " "
            i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            for k in range(i, j):
                if text[k] != "\n":
                    out[k] = " "
            i = j
            continue
        i += 1
    return "".join(out)


def scan_keydown_swallow(text):
    """Check c: flag keyCode=0 / returnValue=false inside a keydown handler body only."""
    text = strip_js_comments(text)
    findings = []
    patterns = (
        re.compile(r"\.onkeydown\s*=\s*function\s*\([^)]*\)\s*\{"),
        re.compile(r"""addEventListener\(\s*['"]keydown['"]\s*,\s*function\s*\([^)]*\)\s*\{"""),
    )
    for pat in patterns:
        for m in pat.finditer(text):
            brace = m.end() - 1
            depth, body_end = 0, None
            for i in range(brace, min(len(text), brace + 20000)):
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                    if depth == 0:
                        body_end = i
                        break
            if body_end is None:
                continue
            body = text[brace + 1:body_end]
            if re.search(r"keyCode\s*=\s*0", body) or re.search(r"returnValue\s*=\s*false", body):
                findings.append((line_of(text, m.start()), "keydown handler swallows all keys (keyCode=0 / returnValue=false)"))
    return findings


def scan_positive_tabindex(text):
    findings = []
    for m in re.finditer(r"""tabindex\s*=\s*["']?(\d+)""", text, re.IGNORECASE):
        if int(m.group(1)) >= 1:
            findings.append((line_of(text, m.start()), f'positive tabindex="{m.group(1)}" (breaks natural tab order, use "0")'))
    return findings


def scan_file(path: Path, root: Path):
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as e:
        return [(1, f"could not read file: {e}")]

    kb_ok = keyboard_path_ok(path, text, root)
    findings = []
    if path.suffix.lower() == ".html":
        checker = HTMLA11yChecker(kb_ok)
        try:
            checker.feed(text)
            checker.finish()
        except Exception as e:
            findings.append((1, f"HTML parse error: {e}"))
        findings.extend(checker.findings)
    else:
        findings.extend(scan_js_templates(text, kb_ok))
    findings.extend(scan_keydown_swallow(text))
    findings.extend(scan_positive_tabindex(text))
    return findings


def main():
    parser = argparse.ArgumentParser(description="Heuristic accessibility checker for resources/web")
    parser.add_argument("directory", nargs="?", default="resources/web", help="Directory to scan")
    args = parser.parse_args()

    root = Path(args.directory)
    if not root.is_dir():
        print_error(f"Directory not found: {root}")
        sys.exit(1)

    print_info(f"Checking web accessibility in {root} ...")
    total = 0
    files_checked = 0
    for path in sorted(root.rglob("*")):
        if path.is_dir() or path.suffix.lower() not in (".html", ".js") or is_skipped(path, root):
            continue
        files_checked += 1
        for line, msg in sorted(scan_file(path, root)):
            print_error(f"{path.as_posix()}:{line}: {msg}")
            total += 1

    print("\n==================== SUMMARY ====================")
    print_info(f"Files checked : {files_checked}")
    if total:
        print_error(f"Findings      : {total}")
    else:
        print_info("Findings      : 0")
    print("=================================================")
    sys.exit(1 if total else 0)


if __name__ == "__main__":
    main()
