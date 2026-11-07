#!/usr/bin/env python3
"""Validate collaboration workflow files. This is not an application test."""

from __future__ import annotations

import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

MARKDOWN_FILES = (
    "README.md",
    "AGENTS.md",
    "CLAUDE.md",
    "docs/COLLABORATION.md",
    ".github/PULL_REQUEST_TEMPLATE.md",
    ".github/ISSUE_TEMPLATE/task.md",
    "docs/PROTOTYPE.md",
    "docs/WEB_DEMO.md",
)

REQUIRED_FILES = MARKDOWN_FILES + (
    ".gitignore",
    ".github/workflows/workflow-check.yml",
    ".github/workflows/prototype-build.yml",
    ".github/workflows/pages.yml",
    "scripts/check_workflow.py",
)

REQUIRED_HEADINGS = {
    "docs/COLLABORATION.md": (
        "Roles",
        "Separate sessions",
        "Task ownership",
        "Branches and worktrees",
        "Pull requests",
        "Secrets",
        "Teammate access",
        "Documentation and enforcement",
        "Onboarding",
        "Instruction files",
    ),
    "AGENTS.md": ("Operating rules",),
    "CLAUDE.md": ("This session",),
    ".github/PULL_REQUEST_TEMPLATE.md": (
        "Summary",
        "Acceptance criteria",
        "How to review",
        "Isolation",
        "Secrets",
        "Merge",
    ),
    ".github/ISSUE_TEMPLATE/task.md": (
        "Goal",
        "Acceptance criteria",
        "Owner",
        "Branch",
        "Handoff",
    ),
}

GITIGNORE_ENTRIES = (
    ".env",
    "CLAUDE.local.md",
    ".claude/settings.local.json",
    "credentials.json",
)

ALLOWED_IMPORT = "docs/COLLABORATION.md"
# Assembled at runtime so this source file is not itself a key-marker match.
PRIVATE_KEY_MARKERS = (
    "-----BEGIN " + "PRIVATE KEY-----",
    "-----BEGIN " + "OPENSSH PRIVATE KEY-----",
    "-----BEGIN " + "RSA PRIVATE KEY-----",
)
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
LINK_RE = re.compile(r"\[[^]]*\]\(([^)]+)\)")
IMPORT_RE = re.compile(r"(?<![\w`])@([A-Za-z0-9_./\\-]+)")


class Report:
    def __init__(self) -> None:
        self.errors: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def finish(self) -> int:
        if self.errors:
            print("Workflow scaffolding check failed:")
            for message in self.errors:
                print(f"- {message}")
            return 1
        print("Workflow scaffolding OK")
        return 0


def unquote(value: str) -> str:
    if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
        return value[1:-1]
    return value


def parse_scalar(value: str) -> str | list[str]:
    if value.startswith("[") and value.endswith("]"):
        inner = value[1:-1].strip()
        if not inner:
            return []
        return [unquote(part.strip()) for part in inner.split(",")]
    return unquote(value)


def parse_yaml_subset(text: str) -> dict:
    """Parse the small indentation-based YAML subset used by this repo."""
    if "\t" in text:
        raise ValueError("tab character in YAML")
    lines: list[tuple[int, str]] = []
    for line_number, raw in enumerate(text.splitlines(), start=1):
        if "\r" in raw:
            raise ValueError(f"line {line_number}: carriage return")
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            continue
        indent = len(raw) - len(raw.lstrip(" "))
        if raw[:indent] != " " * indent:
            raise ValueError(f"line {line_number}: mixed indent")
        if indent % 2:
            raise ValueError(f"line {line_number}: indent is not a multiple of 2")
        lines.append((indent, stripped))

    def parse_block(index: int, indent: int) -> tuple[object, int]:
        if index >= len(lines) or lines[index][0] != indent:
            return {}, index
        if lines[index][1].startswith("- "):
            return parse_list(index, indent)
        return parse_map(index, indent)

    def parse_map(index: int, indent: int) -> tuple[dict, int]:
        result: dict = {}
        while index < len(lines) and lines[index][0] == indent:
            content = lines[index][1]
            if content.startswith("- "):
                break
            if ":" not in content:
                raise ValueError(f"expected key: value, found {content!r}")
            key, _, rest = content.partition(":")
            key = key.strip()
            rest = rest.strip()
            index += 1
            if rest == "":
                if index < len(lines) and lines[index][0] > indent:
                    child, index = parse_block(index, lines[index][0])
                    result[key] = child
                else:
                    result[key] = None
            else:
                result[key] = parse_scalar(rest)
        return result, index

    def parse_list(index: int, indent: int) -> tuple[list, int]:
        result: list = []
        while index < len(lines) and lines[index][0] == indent and lines[index][1].startswith("- "):
            item = lines[index][1][2:].strip()
            index += 1
            if ":" in item:
                key, _, rest = item.partition(":")
                node: dict = {key.strip(): None if rest.strip() == "" else parse_scalar(rest.strip())}
                if index < len(lines) and lines[index][0] > indent:
                    extra, index = parse_map(index, lines[index][0])
                    node.update(extra)
                result.append(node)
            else:
                result.append(parse_scalar(item))
        return result, index

    if not lines:
        raise ValueError("YAML document is empty")
    if lines[0][0] != 0:
        raise ValueError("YAML document must start at column 0")
    document, index = parse_map(0, 0)
    if index != len(lines):
        raise ValueError(f"unparsed YAML remains at {lines[index][1]!r}")
    if not isinstance(document, dict):
        raise ValueError("YAML root must be a mapping")
    return document


def walk(node: object):
    if isinstance(node, dict):
        for key, value in node.items():
            yield key, value
            yield from walk(value)
    elif isinstance(node, list):
        for item in node:
            yield from walk(item)


def validate_workflow(text: str, report: Report) -> None:
    try:
        data = parse_yaml_subset(text)
    except ValueError as exc:
        report.error(f".github/workflows/workflow-check.yml: {exc}")
        return
    if data.get("name") != "Workflow scaffolding":
        report.error("workflow name must be 'Workflow scaffolding'")
    permissions = data.get("permissions")
    if not isinstance(permissions, dict) or permissions.get("contents") != "read":
        report.error("workflow permissions.contents must be read")
    trigger = data.get("on")
    if not isinstance(trigger, dict) or "pull_request" not in trigger:
        report.error("workflow must trigger on pull_request")
    push = trigger.get("push") if isinstance(trigger, dict) else None
    branches = push.get("branches") if isinstance(push, dict) else None
    if branches != ["main"]:
        report.error("workflow push trigger must list branches: [main]")
    jobs = data.get("jobs")
    job = jobs.get("scaffolding") if isinstance(jobs, dict) else None
    if not isinstance(job, dict):
        report.error("workflow must define jobs.scaffolding")
        return
    if job.get("name") != "Check workflow scaffolding":
        report.error("workflow job name must be 'Check workflow scaffolding'")
    if job.get("runs-on") != "ubuntu-latest":
        report.error("workflow must run on ubuntu-latest")
    steps = job.get("steps")
    if not isinstance(steps, list):
        report.error("workflow job must define steps")
        return
    uses = [value for key, value in walk(data) if key == "uses"]
    runs = [value for key, value in walk(data) if key == "run"]
    if uses != ["actions/checkout@v4"]:
        report.error(f"workflow must use only actions/checkout@v4, found {uses}")
    if runs != ["python3 scripts/check_workflow.py", "make -C prototype test"]:
        report.error(f"workflow must run the scaffolding checker and host logic tests, found {runs}")
    flat = text
    if "secrets." in flat:
        report.error("workflow must not reference GitHub secrets")
    for token in ("npm ", "pip ", "pip3", "curl ", "wget ", "apt-get", "docker "):
        if token in flat:
            report.error(f"workflow must not install or fetch dependencies ({token.strip()})")


def validate_build_workflow(text: str, report: Report) -> None:
    forbidden = ("pull_request_target", "secrets.", "workflow_run:")
    for token in forbidden:
        if token in text:
            report.error(f"prototype build workflow must not contain {token}")
    required = (
        "permissions:",
        "contents: read",
        "runs-on: macos-latest",
        "runs-on: ubuntu-latest",
        "actions/checkout@v4",
        "CODE_SIGNING_ALLOWED=NO",
        "arduino:renesas_uno@1.6.0",
        "ArduinoBLE@2.1.0",
        "28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b",
        "generic/platform=iOS",
        "arduino:renesas_uno:unor4wifi",
    )
    for item in required:
        if item not in text:
            report.error(f"prototype build workflow is missing {item}")
    if text.count("uses: actions/checkout@v4") != 2:
        report.error("prototype build workflow must check out once per job and use no other action")


def validate_pages_workflow(text: str, report: Report) -> None:
    forbidden = ("pull_request_target", "secrets.", "workflow_run:", "pull_request:")
    for token in forbidden:
        if token in text:
            report.error(f"pages workflow must not contain {token}")
    required = (
        "branches: [main]",
        "contents: read",
        "pages: write",
        "id-token: write",
        "actions/checkout@v4",
        "actions/upload-pages-artifact@v5",
        "actions/deploy-pages@v5",
        "path: web",
        "refs/heads/main",
    )
    for item in required:
        if item not in text:
            report.error(f"pages workflow is missing {item}")


def split_front_matter(text: str) -> tuple[str, str]:
    if not text.startswith("---\n"):
        raise ValueError("issue template is missing opening front matter")
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("issue template front matter is not closed")
    return text[4:end], text[end + 5 :]


def validate_issue_template(text: str, report: Report) -> None:
    try:
        raw, _body = split_front_matter(text)
        data = parse_yaml_subset(raw + "\n")
    except ValueError as exc:
        report.error(f".github/ISSUE_TEMPLATE/task.md: {exc}")
        return
    if data.get("name") != "Task":
        report.error("issue template front matter name must be Task")
    about = data.get("about")
    if not isinstance(about, str) or not about.strip():
        report.error("issue template front matter must include a non-empty about field")
    for forbidden in ("assignees", "labels"):
        if forbidden in data and data[forbidden] not in (None, "", []):
            report.error(f"issue template must not guess {forbidden}")


def blank_inline_code(line: str) -> str:
    return re.sub(r"`[^`]*`", lambda match: " " * len(match.group(0)), line)


def visible_lines(text: str) -> list[tuple[int, str]]:
    visible: list[tuple[int, str]] = []
    fenced = False
    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.strip().startswith("```"):
            fenced = not fenced
            continue
        if fenced:
            continue
        visible.append((line_number, line))
    return visible


def imports_in(text: str) -> list[tuple[int, str]]:
    found: list[tuple[int, str]] = []
    for line_number, line in visible_lines(text):
        found.extend((line_number, match) for match in IMPORT_RE.findall(blank_inline_code(line)))
    return found


def headings_in(text: str) -> list[str]:
    headings: list[str] = []
    seen: dict[str, int] = {}
    for _line_number, line in visible_lines(text):
        match = HEADING_RE.match(line)
        if not match:
            continue
        title = match.group(2).strip()
        slug = github_slug(title)
        count = seen.get(slug, 0)
        seen[slug] = count + 1
        headings.append(slug if count == 0 else f"{slug}-{count}")
    return headings


def heading_titles(text: str) -> list[str]:
    titles: list[str] = []
    for _line_number, line in visible_lines(text):
        match = HEADING_RE.match(line)
        if match:
            titles.append(match.group(2).strip())
    return titles


def github_slug(text: str) -> str:
    text = text.strip().lower()
    text = text.replace("`", "")
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE)
    text = re.sub(r"\s+", "-", text)
    return text.strip("-")


def check_markdown(path: Path, report: Report) -> None:
    text = path.read_text(encoding="utf-8")
    relative = path.relative_to(ROOT).as_posix()
    if "\r" in text:
        report.error(f"{relative}: use LF line endings")
    titles = heading_titles(text)
    for title in REQUIRED_HEADINGS.get(relative, ()):
        if title not in titles:
            report.error(f"{relative}: missing heading {title!r}")
    found_imports = imports_in(text)
    if relative == "CLAUDE.md":
        paths = [item for _line, item in found_imports]
        if paths != [ALLOWED_IMPORT]:
            report.error(
                "CLAUDE.md must contain exactly one unfenced import of "
                f"{ALLOWED_IMPORT}, found {paths}"
            )
    elif found_imports:
        rendered = ", ".join(f"line {line}: @{target}" for line, target in found_imports)
        report.error(f"{relative}: unexpected Claude import syntax ({rendered})")
    slug_cache: dict[Path, list[str]] = {path: headings_in(text)}
    for line_number, line in visible_lines(text):
        for raw_url in LINK_RE.findall(line):
            check_link(relative, line_number, raw_url, path, slug_cache, report)


def check_link(
    relative: str,
    line_number: int,
    raw_url: str,
    source: Path,
    slug_cache: dict[Path, list[str]],
    report: Report,
) -> None:
    url = raw_url.strip()
    if url.startswith("<") and ">" in url:
        url = url[1 : url.index(">")].strip()
    path_part, _, fragment = url.partition("#")
    path_part = path_part.strip()
    if " " in path_part:
        path_part = path_part.split()[0]
    if path_part.startswith(("http://", "https://", "mailto:")):
        return
    if not path_part and not fragment:
        report.error(f"{relative}:{line_number}: empty link")
        return
    target = source if not path_part else (source.parent / path_part).resolve()
    if path_part:
        if not target.is_relative_to(ROOT):
            report.error(f"{relative}:{line_number}: link escapes the repository ({raw_url})")
            return
        if not target.is_file():
            report.error(f"{relative}:{line_number}: missing file for ({raw_url})")
            return
    if not fragment:
        return
    if target not in slug_cache:
        slug_cache[target] = headings_in(target.read_text(encoding="utf-8"))
    if fragment.lower() not in slug_cache[target]:
        report.error(
            f"{relative}:{line_number}: missing heading #{fragment} in {target.relative_to(ROOT).as_posix()}"
        )


def check_secret_markers(report: Report) -> None:
    for path in ROOT.rglob("*"):
        if not path.is_file() or ".git" in path.parts:
            continue
        relative = path.relative_to(ROOT).as_posix()
        name = path.name
        if name in {".env", "id_rsa", "id_rsa.pub", "credentials.json"} or name.endswith((".pem", ".key")):
            report.error(f"secret-like file is tracked in the worktree: {relative}")
            continue
        if path.suffix.lower() in {".png", ".jpg", ".jpeg", ".gif", ".webp", ".pdf", ".zip"}:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeError:
            continue
        for marker in PRIVATE_KEY_MARKERS:
            if marker in text:
                report.error(f"{relative}: contains a private-key marker")
                break


def external_urls() -> list[str]:
    found: list[str] = []
    for relative in MARKDOWN_FILES:
        text = (ROOT / relative).read_text(encoding="utf-8")
        for _line_number, line in visible_lines(text):
            for raw_url in LINK_RE.findall(line):
                url = raw_url.strip().split()[0]
                if url.startswith(("http://", "https://")):
                    found.append(url.split("#", 1)[0])
    return sorted(set(found))


def check_external(report: Report) -> None:
    for url in external_urls():
        request = urllib.request.Request(url, headers={"User-Agent": "workflow-scaffolding-check"})
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                status = response.status
                final = response.geturl()
        except urllib.error.HTTPError as exc:
            report.error(f"external link {url} returned HTTP {exc.code}")
            continue
        except urllib.error.URLError as exc:
            report.error(f"external link {url} failed: {exc.reason}")
            continue
        if status != 200:
            report.error(f"external link {url} returned HTTP {status}")
        elif final.rstrip("/") != url.rstrip("/"):
            print(f"redirect: {url} -> {final}")


def self_test() -> None:
    sample = """
name: Workflow scaffolding
on:
  pull_request:
  push:
    branches: [main]
permissions:
  contents: read
jobs:
  scaffolding:
    name: Check workflow scaffolding
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      - name: Validate scaffolding
        run: python3 scripts/check_workflow.py
"""
    data = parse_yaml_subset(sample)
    assert data["on"]["push"]["branches"] == ["main"]
    assert data["jobs"]["scaffolding"]["steps"][1]["run"] == "python3 scripts/check_workflow.py"
    try:
        parse_yaml_subset("name:\tbad\n")
    except ValueError:
        pass
    else:
        raise AssertionError("tabbed YAML should fail")
    fenced = "```\n[missing](does-not-exist.md)\n```\n[ok](README.md)\n"
    assert LINK_RE.findall("\n".join(line for _n, line in visible_lines(fenced))) == ["README.md"]
    assert imports_in("Use `@docs/NOPE.md` in prose.\n") == []
    assert imports_in("@docs/COLLABORATION.md\n") == [(1, "docs/COLLABORATION.md")]
    assert github_slug("Branches and worktrees") == "branches-and-worktrees"
    assert github_slug("Documentation and enforcement") == "documentation-and-enforcement"


def main(argv: list[str]) -> int:
    self_test()
    report = Report()
    for relative in REQUIRED_FILES:
        if not (ROOT / relative).is_file():
            report.error(f"missing required file {relative}")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    if not readme.startswith("# agent-workflow\nMakeUC Hackathon 2026\n"):
        report.error("README.md must keep the original title and hackathon line")
    gitignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
    for entry in GITIGNORE_ENTRIES:
        if entry not in gitignore.splitlines():
            report.error(f".gitignore is missing {entry}")
    workflow = ROOT / ".github/workflows/workflow-check.yml"
    if workflow.is_file():
        validate_workflow(workflow.read_text(encoding="utf-8"), report)
    build_workflow = ROOT / ".github/workflows/prototype-build.yml"
    if build_workflow.is_file():
        validate_build_workflow(build_workflow.read_text(encoding="utf-8"), report)
    pages_workflow = ROOT / ".github/workflows/pages.yml"
    if pages_workflow.is_file():
        validate_pages_workflow(pages_workflow.read_text(encoding="utf-8"), report)
    issue = ROOT / ".github/ISSUE_TEMPLATE/task.md"
    if issue.is_file():
        validate_issue_template(issue.read_text(encoding="utf-8"), report)
    for relative in MARKDOWN_FILES:
        path = ROOT / relative
        if path.is_file():
            check_markdown(path, report)
    check_secret_markers(report)
    if "--external" in argv:
        check_external(report)
    return report.finish()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
