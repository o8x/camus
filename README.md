# Camus

<p align="center">
  <img src="example/assets/favicon.ico" alt="Camus" width="96" height="96">
</p>

Camus is a static site generator written in C++23. It reads Markdown posts and theme templates and renders them into a static HTML site, aimed at individuals and teams who want a lightweight publishing pipeline.

<a href="README_zh-CN.md">中文</a>

## Requirements

- CMake 3.28+
- Ninja (recommended) or Make
- A C++23 toolchain
- Optional: Chrome/Chromium/Edge for PDF output

## Build

```bash
git submodule update --init --recursive
make build          # output: build/release/bin/camus
```

## Quick start

The repository ships a complete example in `example/`:

```bash
cd example
../build/release/bin/camus --build
```

Or create a new site in an empty directory:

```bash
mkdir my-site && cd my-site
/path/to/camus -i      # install example camus.yaml, posts (with SUMMARY.md, plus posts/private/), .gitignore and Justfile
/path/to/camus --build # build
```

`camus` looks for Markdown sources in `src/` first, then `posts/`. The generated site is written to `dist/html/` (and PDF to `dist/pdf/` when configured). Building only happens when explicitly requested with `--build`, `-W` or `--check`; running `camus` without any option prints usage.

### Common commands

| Command | Description |
|---|---|
| `camus` | Print usage (does not build) |
| `camus --build` | Build the site |
| `camus -w <dir> --build` | Build a site in another workdir |
| `camus -W` | Watch for changes and serve locally (implies build) |
| `camus -D --build` | Dry run: parse and render without writing files |
| `camus --check` | Validate configuration, assets, PDF settings and run a dry build |
| `camus -i` | Install example `camus.yaml`, `posts` (with `SUMMARY.md`, plus `posts/private/`), `.gitignore` and `Justfile` |
| `camus --new <dir>` | Create a site directory and install example files |
| `camus --create <path>` | Create an article `.md` under `src/` or `posts/` with a starter template (missing directories are created; last segment is the file name) |
| `camus --repo [url]` | Run `git init`; set `origin` when a URL is provided |
| `camus --new <dir> --repo <url>` | Create, install and initialize git |
| `camus -c` | Copy minimal editable resources (`assets/css/style.css`, `assets/js/onload.js`, empty `assets/fonts/`) into `./assets` |
| `camus -c --full` | Copy the complete bundled resource tree into `./.camus` |
| `camus -c --theme` | Copy the whole `theme/` directory into `./theme`, plus minimal `assets/` |
| `camus -c --theme <name>` | Copy only `theme/<name>/` into `./theme/<name>`, plus minimal `assets/` |
| `camus -c -f` | Force re-copy the selected resources |
| `camus --open` | Open the workdir in VS Code (does not build) |
| `camus --open runtime` | Open the data directory in the file manager (does not build) |

## Configuration

A site uses one main config file plus optional per-directory metadata:

- `camus.yaml` — site, output, rendering, friends and transfer configuration
- `SUMMARY.md` — optional metadata for a directory (`title`, `subtitle`, `description`, `cover_image`)

### `camus.yaml`

```yaml
meta: true                 # generate meta.json
sitemap: true              # generate sitemap.xml
filename_case: keep        # keep | uuid

# Public URL of the site. It is split into protocol/domain_name/path_prefix:
# https://stdout.com.cn/camus -> https:// + stdout.com.cn + /camus
public_url: https://stdout.com.cn/camus

# Optional CDN base URL used by with_cdn(). Empty by default; when empty,
# with_cdn(path) returns with_prefix(path) (including path_prefix).
cdn_url: ''                # e.g. https://cdn.stdout.com.cn/camus

theme: terminal
pdf:
  engine: chrome           # use "disable" to skip PDF generation
  sort: file_first         # file_first / dir_first / mixed
  build:
    - --headless
    - --print-to-pdf={{dest_dir}}/index.pdf
  page:
    size: A4
    margin: 20mm 18mm
markdown:
  - DEFAULT
  - SMART
  - FOOTNOTES

friends:
  - title: Alex
    link_url: https://github.com/o8x/camus
    image_url: https://stdout.com.cn/logo.png
    desc: What's past is prologue.

# Move directory contents to another output path
build:
  - path: /introduction
    transfers:
      - source: .
        target: /intro/Well-known
```

All scalar config values can be overridden by a `CAMUS_<PATH>` environment variable (e.g. `pdf.page.size` → `CAMUS_PDF_PAGE_SIZE`, `theme` → `CAMUS_THEME`). Arrays/lists such as `markdown`, `pdf.build`, `friends` and `build` cannot be injected.

`public_url` accepts with or without a protocol. Missing protocols default to `https://`; a trailing slash is normalized away. It drives absolute links, the sitemap, and PDF cover data.

`cdn_url` is optional and empty by default. When set, `with_cdn(path)` returns `cdn_url + with_prefix(path)`; when empty it returns `with_prefix(path)`, so the `public_url.path_prefix` is always applied. In watch mode (`-W`), both `public_url` and `cdn_url` are ignored so local builds use root-relative paths.

### Directory metadata (`SUMMARY.md`)

Every directory in the source tree may contain a `SUMMARY.md` that describes it. `SUMMARY.md` files are metadata only and are never rendered as articles.

```markdown
---
title: Well-known musicals
subtitle: Optional short summary
cover_image: assets/cover.jpg
---

Optional description shown on the directory page.
```

Parsing rules:

- `title`, `subtitle`, `description` and `cover_image` may be provided in an optional YAML front matter block.
- `cover_image` in the root `SUMMARY.md` sets the PDF cover. Relative paths are resolved from the directory containing `camus.yaml`, then fall back to the source directory and the site `assets` directory; absolute paths are used as-is.
- If `title` is missing, the first `# Heading` in the body is used.
- If `description` is missing, the remaining body is used.
- Unset fields inherit from the root route; the root `SUMMARY.md` (`posts/SUMMARY.md` or `src/SUMMARY.md`) defines the site title/subtitle/description.
- Newlines in metadata become `<br />`.

### Build rules

`build` in `camus.yaml` maps directory content to a different output location (the legacy key `transfer` is still accepted). Each item has a `path` plus a list of `transfers`:

| Field | Required | Description |
|---|---|---|
| `path` | yes | Directory path relative to the source directory. |
| `transfers[].source` | yes | Source path. `.` or `/` means the directory itself; anything else is joined under `path`. |
| `transfers[].target` | yes | Output directory used as the external path for that directory. |

Examples:

```yaml
build:
  # posts/introduction/** -> /intro/Well-known/**
  - path: /introduction
    transfers:
      - source: .
        target: /intro/Well-known

  # posts/Don Juan/** -> /M/**
  - path: /Don Juan
    transfers:
      - source: .
        target: /M

  # Move only a subdirectory:
  # posts/guide/advanced/** -> /docs/advanced/**
  - path: /guide
    transfers:
      - source: advanced
        target: /docs/advanced
```

Notes:

- Transfers are whole-directory mappings. All files (including nested subdirectories) under the source directory are written below `target`.
- Without a transfer rule, the directory keeps its original path.
- The root path `/` cannot use transfers.
- A path that contains a `private` segment is ignored (with a warning); `private` directories are excluded from the source tree.

## Posts

Markdown files live under `src/` or `posts/`. Optional front matter sits between `---` markers (the file must start with `---` to enable it):

```markdown
---
title: My article
create-time: 2026-01-01
tags: [ "notes", "cpp" ]
visibility: open
---
```

Supported fields include `title`, `short-path`, `create-time`, `visibility` (`open` / `hidden` / `hidden_in_toc`) and `tags`. `hidden` articles are not rendered; `hidden_in_toc` articles are rendered but omitted from TOC/navigation lists.

## Themes and template helpers

Themes contain `home.html`, `page.html`, `stats.html` and `friends.html`, loaded from `theme/{name}/`, then `.camus/theme/{name}/`, then the platform data directory. All built-in themes reference the shared `assets/css/style.css` and `assets/js/onload.js` entries (empty by default, override locally in `./assets`). Templates use [inja](https://github.com/pantor/inja) and expose the parsed configuration plus helpers:

| Helper | Description |
|---|---|
| `{{ with_prefix("/css/site.css") }}` | Prefix a path with `path_prefix` |
| `{{ with_cdn("/css/site.css") }}` | Prefix with `cdn_url` plus `path_prefix`; falls back to `with_prefix(path)` when `cdn_url` is empty |

## Output

```text
dist/
├── html/            # static HTML site (index pages, articles, stats, friends, sitemap, meta)
└── pdf/             # when pdf.engine is configured and not "disable"
```
