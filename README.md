Camus
=====

![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/o8x/camus/auto-release-tag.yml)
![GitHub License](https://img.shields.io/github/license/o8x/camus)

<a href="README.md">English</a> |
<a href="README_zh-CN.md" >中文</a>

Camus is a static site generator written in C++23. It converts Markdown posts into static HTML pages with theme support, file watching, catalog generation, and sitemap generation — all bundled into a single self-contained binary.

- **Self-contained**: The example site template is embedded in the binary. Use `-i` to bootstrap a new site instantly.
- **Fast**: Built with C++23, optimized for performance with release builds.
- **Watch mode**: Auto-rebuilds when source files change.
- **Multi-theme**: Choose from `default`, `accent`, `pixel`, and `terminal` themes, or write your own.
- **Catalog**: Generate a structured table of contents for multi-level directory hierarchies.
- **Sitemap**: Automatic `sitemap.xml` generation.

## Quick Start

```bash
camus -iw mysite    	# Install the example template into ./mysite
camus -Ww mysite       	# Watch and auto-rebuild on changes
camus -I              	# Inspect and display the current configuration
```

The built site is output to `html/` by default.

## Command-Line Options

| Short | Long        | Description                                         |
|-------|-------------|-----------------------------------------------------|
| `-h`  | `--help`    | Print help and exit                                 |
| `-v`  | `--version` | Print version, build info, and git hash             |
| `-i`  | `--install` | Install the example site template into the work dir |
| `-f`  | `--force`   | Force operation (ignore errors, overwrite existing) |
| `-d`  | `--debug`   | Enable debug logging                                |
| `-W`  | `--watch`   | Watch files and auto-rebuild on changes             |
| `-D`  | `--dryrun`  | Dry run (simulate without writing output)           |
| `-I`  | `--inspect` | Inspect and display the current configuration       |
| `-w`  | `--workdir` | Specify working directory (default: `.`)            |

## Project Structure

After running `camus -i`, your working directory will contain:

```
.
├── .env                  # Environment variables ({{env.KEY}} in templates)
├── .gitignore
├── camus.yaml            # Site configuration
├── .camus/               # Internal template cache
├── assets/               # Static assets (copied to output as-is)
├── posts/                # Markdown posts
└── theme/                # HTML templates
```

## Configuration

- see [example](example)

### .env

Template variables are replaced during rendering. Use `{{env.key}}` in your HTML templates:

## Post Front Matter

Each Markdown file starts with YAML front matter:

```yaml
---
short-path: my-post
display-name: My First Post
date: 2025-03-21
visibility: open
---
```

| Field          | Description                                                                  |
|----------------|------------------------------------------------------------------------------|
| `short-path`   | Custom output filename (overrides the source filename when set)              |
| `display-name` | Post title shown in the page and catalog                                     |
| `date`         | Publication date                                                             |
| `visibility`   | `open` (default), `hidden` (not rendered), `hidden-in-toc` (hidden from TOC) |

### Hidden Content

Wrap content with markers to exclude it from rendering:

```
--hidden-section-start
This content will not appear in the output.
--hidden-section-end
```

## Building from Source

### Prerequisites

- CMake 3.28.3+
- GCC or Clang with C++23 support
- Git (to clone submodules)

### Build

```bash
git clone --recurse-submodules https://github.com/o8x/camus.git camus
make -C camus build
```

The template is automatically embedded into the binary as a post-build step.

### Makefile Convenience Targets

```bash
make build       # Release build
make serve       # Build and serve on localhost:8000
make push        # Build and push to GitHub Pages
make package     # Build a release package
```

## Dependencies

All dependencies are included as Git submodules:

| Library       | Purpose                            |
|---------------|------------------------------------|
| cmark-gfm     | GitHub-Flavored Markdown rendering |
| yaml-cpp      | YAML configuration parsing         |
| cpp-httplib   | HTTP server for watch mode         |
| nlohmann/json | JSON output for TOC                |

## License

This project is distributed under the terms found in the [LICENSE](LICENSE) file.
