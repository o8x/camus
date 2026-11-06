# Camus

<p align="center">
  <img src="example/assets/favicon.ico" alt="Camus" width="96" height="96">
</p>

Camus 是一个使用 C++23 编写的静态站点生成器，读取 Markdown 文章与主题模板并渲染为静态 HTML 站点，面向希望拥有轻量发布流程的个人与团队。

<a href="README.md">English</a>

## 环境要求

- CMake 3.28+
- Ninja（推荐）或 Make
- 支持 C++23 的工具链
- 可选：Chrome/Chromium/Edge（用于生成 PDF）

## 构建

```bash
git submodule update --init --recursive
make build          # 产物：build/release/bin/camus
```

## 快速开始

仓库自带完整示例 `example/`：

```bash
cd example
../build/release/bin/camus
```

在空目录中新建站点：

```bash
mkdir my-site && cd my-site
/path/to/camus -i      # 安装示例 camus.yaml、posts（含 SUMMARY.md 与 posts/private/）与 .gitignore
/path/to/camus         # 构建
```

`camus` 优先从 `src/` 读取 Markdown，不存在时回退 `posts/`。生成的站点写入 `dist/html/`（配置 PDF 时另写入 `dist/pdf/`）。

### 常用命令

| 命令 | 说明 |
|---|---|
| `camus` | 构建站点 |
| `camus -w <dir>` | 构建指定工作目录中的站点 |
| `camus -W` | 监听文件变化并启动本地服务 |
| `camus -D` | 干跑：解析与渲染，但不写文件 |
| `camus -I` | 打印解析后的配置与目录树 |
| `camus -i` | 安装示例 `camus.yaml`、`posts`（含 `SUMMARY.md` 与 `posts/private/`）与 `.gitignore` |
| `camus -c` | 把最小可编辑资源（`assets/css/style.css`、`assets/js/onload.js`）复制到 `./.camus` |
| `camus -c --full` | 把完整内置资源树复制到 `./.camus` |
| `camus -c -f` | 强制重装所选 `./.camus` 资源 |
| `camus --open` | 构建后在 VS Code 中打开工作目录 |
| `camus --open runtime` | 构建后用文件管理器打开数据目录 |

## 配置

站点由一个主配置文件与可选的目录元信息驱动：

- `camus.yaml`：站点、输出、渲染、友链与搬运配置
- `SUMMARY.md`：可选的目录元信息（`title` / `subtitle` / `description`）

### `camus.yaml`

```yaml
meta: true                 # 生成 meta.json
sitemap: true              # 生成 sitemap.xml
filename_case: keep        # keep | uuid

# 站点对外访问地址，会自动拆解为 protocol/domain_name/path_prefix：
# https://stdout.com.cn/camus -> https:// + stdout.com.cn + /camus
public_url: https://stdout.com.cn/camus

# 可选的 CDN 地址，供 with_cdn() 使用；默认空，为空时 with_cdn(path) 返回 with_prefix(path)（含 path_prefix）
cdn_url: ''                # 例如 https://cdn.stdout.com.cn/camus

theme: terminal
pdf_engine: chrome         # 设为 disable 时跳过 PDF 生成
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

# 目录搬运规则：把 path 目录的内容映射到 target 输出路径
build:
  - path: /introduction
    transfers:
      - source: .
        target: /intro/Well-known
```

`public_url` 可带协议也可省略：省略时默认 `https://`，末尾多余的 `/` 会被归一化。它用于绝对链接、sitemap 与 PDF 封面信息。

`cdn_url` 可选、默认空。设置后 `with_cdn(path)` 返回 `cdn_url + with_prefix(path)`；为空时返回 `with_prefix(path)`，始终带上 `public_url.path_prefix`。监听模式（`-W`）下会忽略 `public_url` 与 `cdn_url`，本地构建使用根相对路径。

### 目录元信息（`SUMMARY.md`）

源目录树中的任意目录都可以放置 `SUMMARY.md` 来描述该目录。`SUMMARY.md` 仅作为元信息，不会被当作文章渲染。

```markdown
---
title: Well-known musicals
subtitle: 可选的简短摘要
---

可选的目录页简介。
```

解析规则：

- `title` / `subtitle` / `description` 可写在可选的 YAML Front Matter 中。
- 缺少 `title` 时使用正文中第一个 `# 标题`。
- 缺少 `description` 时使用剩余正文。
- 未设置的字段继承根目录；根目录的 `SUMMARY.md`（`posts/SUMMARY.md` 或 `src/SUMMARY.md`）定义站点标题/副标题/简介。
- 元信息中的换行会转换为 `<br />`。

### 搬运规则

`camus.yaml` 的 `build` 段把某个目录的内容映射到另一个输出位置（旧键 `transfer` 仍兼容）。每项包含 `path` 与 `transfers` 列表：

| 字段 | 必填 | 说明 |
|---|---|---|
| `path` | 是 | 相对于源目录的路径。 |
| `transfers[].source` | 是 | 源路径。`.` 或 `/` 表示目录自身；其它值拼接到 `path` 之下。 |
| `transfers[].target` | 是 | 目标输出目录，作为该目录的对外路径。 |

示例：

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

  # 只搬运子目录：
  # posts/guide/advanced/** -> /docs/advanced/**
  - path: /guide
    transfers:
      - source: advanced
        target: /docs/advanced
```

说明：

- 搬运是整目录映射：源目录下的所有文件（含嵌套子目录）都会输出到 `target` 之下。
- 不设置搬运时保留原目录路径。
- 根路径 `/` 不能使用搬运。
- 路径中包含 `private` 段会被忽略并打印警告；`private` 目录本身也不会进入源目录树。

## 文章

Markdown 文件放在 `src/` 或 `posts/` 下。可选 Front Matter 位于 `---` 之间（文件首行必须为 `---` 才会启用）：

```markdown
---
display-name: My article
date: 2026-01-01
tags: [ "notes", "cpp" ]
visibility: open
---
```

支持 `display-name`、`short-path`、`date`、`visibility`（`open` / `hidden` / `hidden_in_toc`）与 `tags` 等字段。`hidden` 文章不渲染；`hidden_in_toc` 文章正常渲染，但不进入 TOC/导航列表。

## 主题与模板函数

主题包含 `home.html`、`page.html`、`stats.html`、`friends.html`，加载顺序为 `theme/{name}/` → `.camus/theme/{name}/` → 平台数据目录。所有内置主题都会引用公共入口 `assets/css/style.css` 与 `assets/js/onload.js`（默认空文件，可在本地 `./assets` 覆盖）。模板使用 [inja](https://github.com/pantor/inja)，可直接访问解析后的配置，并提供：

| 函数 | 说明 |
|---|---|
| `{{ with_prefix("/css/site.css") }}` | 为路径加上 `path_prefix` |
| `{{ with_cdn("/css/site.css") }}` | 用 `cdn_url + path_prefix` 拼接；`cdn_url` 为空时回退到 `with_prefix(path)`（含 `path_prefix`） |

## 输出

```text
dist/
├── html/            # 静态 HTML 站点（目录页、文章、stats、friends、sitemap、meta）
└── pdf/             # pdf_engine 非空且不为 disable 时生成
```
