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
../build/release/bin/camus
```

Or create a new site in an empty directory:

```bash
mkdir my-site && cd my-site
/path/to/camus -i      # install example camus.yaml, posts (with catalog.yaml) and .gitignore
/path/to/camus         # build
```

`camus` looks for Markdown sources in `src/` first, then `posts/`. The generated site is written to `dist/html/` (and PDF to `dist/pdf/` when configured).

### Common commands

| Command | Description |
|---|---|
| `camus` | Build the site |
| `camus -w <dir>` | Build a site in another workdir |
| `camus -W` | Watch for changes and serve locally |
| `camus -D` | Dry run: parse and render without writing files |
| `camus -I` | Inspect the parsed configuration and catalog tree |
| `camus -i` | Install example `camus.yaml`, `posts` (with `catalog.yaml`) and `.gitignore` |
| `camus -c` | Copy the complete bundled resources into `./.camus` |
| `camus -c -f` | Force re-copy `./.camus` |

## Configuration

A site uses two files:

- `camus.yaml` — site, output, rendering and friends configuration
- `catalog.yaml` — directory routes and file transfer rules (inside `src/` or `posts/`)

### `camus.yaml`

```yaml
meta: true                 # generate meta.json
sitemap: true              # generate sitemap.xml
filename_case: keep        # keep | uuid

# Public URL of the site. It is split into protocol/domain_name/path_prefix:
# https://stdout.com.cn/camus -> https:// + stdout.com.cn + /camus
public_url: https://stdout.com.cn/camus

theme: terminal
pdf_engine: chrome         # use "disable" to skip PDF generation
pdf_options:
  - --headless
  - --print-to-pdf={{dest_dir}}/index.pdf
markdown_options:
  - DEFAULT
  - SMART
  - FOOTNOTES

friends:
  - title: Alex
    link_url: https://github.com/o8x/camus
    image_url: https://stdout.com.cn/logo.png
    desc: What's past is prologue.
```

`public_url` accepts with or without a protocol. Missing protocols default to `https://`; a trailing slash is normalized away. It drives absolute links, the sitemap, PDF cover data, and the `with_cdn()` helper.

### `catalog.yaml`

Place `catalog.yaml` in the source directory (`src/` or `posts/`).

`catalog.yaml` lives in the source directory (`src/` or `posts/`) and is a YAML sequence. Each item describes a directory route:

```yaml
- path: /                     # required; route to the source directory
  title: Musicals             # directory page title
  subtitle: Musical theatre   # directory page subtitle
  description: |              # directory page description
    Musicals combine songs, dialogue, acting and dance.
  transfers: []               # ignored for the root route

- path: /introduction
  transfers:
    - source: .
      target: /intro/Well-known
  title: Well-known musicals
```

Fields:

| Field | Required | Description |
|---|---|---|
| `path` | yes | Directory path relative to the source directory. Items without `path` are ignored. |
| `title` | no | Display name / page title. Empty values inherit from the root route. |
| `subtitle` | no | Page subtitle. Empty values inherit from the root route. |
| `description` | no | Page description. Empty values inherit from the root route. |
| `transfers` | no | File transfer rules. Only effective when `path != "/"`. |

### Transfer rules

`transfers` maps a directory's content to a different output location. Each rule has:

| Field | Required | Description |
|---|---|---|
| `source` | yes | Source path. `.` or `/` means the route directory itself; anything else is joined under the route path. |
| `target` | yes | Output directory used as the external path for this route. |

Examples:

```yaml
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
- The root route `path: "/"` defines the site title/subtitle/description and cannot use transfers.
- A route whose path contains a `private` segment is ignored (with a warning); `private` directories are excluded from the source tree.

## Posts

Markdown files live under `src/` or `posts/`. Optional front matter sits between `---` markers (the file must start with `---` to enable it):

```markdown
---
display-name: My article
date: 2026-01-01
tags: [ "notes", "cpp" ]
visibility: open
---
```

Supported fields include `display-name`, `short-path`, `date`, `visibility` (`open` / `hidden` / `hidden_in_toc`) and `tags`. `hidden` articles are not rendered; `hidden_in_toc` articles are rendered but omitted from TOC/navigation lists.

## Themes and template helpers

Themes contain `home.html`, `page.html`, `stats.html` and `friends.html`, loaded from `theme/{name}/`, then `.camus/theme/{name}/`, then the platform data directory. All built-in themes reference the shared `assets/css/style.css` and `assets/js/onload.js` entries (empty by default, override locally in `./assets`). Templates use [inja](https://github.com/pantor/inja) and expose the parsed configuration plus helpers:

| Helper | Description |
|---|---|
| `{{ with_prefix("/css/site.css") }}` | Prefix a path with `path_prefix` |
| `{{ with_cdn("/css/site.css") }}` | Build an absolute URL from `protocol + domain_name + path_prefix` |

## Output

```text
dist/
├── html/            # static HTML site (index pages, articles, stats, friends, sitemap, meta)
└── pdf/             # when pdf_engine is configured and not "disable"
```
