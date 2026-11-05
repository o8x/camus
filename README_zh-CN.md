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
/path/to/camus -i      # 安装示例 camus.yaml、catalog.yaml、posts 与 .gitignore
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
| `camus -i` | 安装示例 `camus.yaml`、`catalog.yaml`、`posts`、`.gitignore` |
| `camus -c` | 把完整内置资源复制到 `./.camus` |
| `camus -c -f` | 强制重装 `./.camus` |

## 配置

站点由两个文件驱动：

- `camus.yaml`：站点、输出、渲染与友链配置
- `catalog.yaml`：目录路由与文件搬运规则

### `camus.yaml`

```yaml
meta: true                 # 生成 meta.json
sitemap: true              # 生成 sitemap.xml
filename_case: keep        # keep | uuid

# 站点对外访问地址，会自动拆解为 protocol/domain_name/path_prefix：
# https://stdout.com.cn/camus -> https:// + stdout.com.cn + /camus
public_url: https://stdout.com.cn/camus

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
```

`public_url` 可带协议也可省略：省略时默认 `https://`，末尾多余的 `/` 会被归一化。它用于绝对链接、sitemap、PDF 封面信息以及 `with_cdn()` 函数。

### `catalog.yaml`

`catalog.yaml` 是一个 YAML 序列，每一项描述一个目录路由：

```yaml
- path: /                     # 必填；指定源目录
  title: Musicals             # 目录页标题
  subtitle: Musical theatre   # 目录页副标题
  description: |              # 目录页简介
    Musicals combine songs, dialogue, acting and dance.
  transfers: []               # 根目录的 transfers 无效

- path: /introduction
  transfers:
    - source: .
      target: /intro/Well-known
  title: Well-known musicals
```

字段说明：

| 字段 | 必填 | 说明 |
|---|---|---|
| `path` | 是 | 相对于源目录的路径；缺失该项会被忽略。 |
| `title` | 否 | 显示名 / 页面标题；为空时继承根路由。 |
| `subtitle` | 否 | 页面副标题；为空时继承根路由。 |
| `description` | 否 | 页面简介；为空时继承根路由。 |
| `transfers` | 否 | 文件搬运规则；仅当 `path != "/"` 时生效。 |

### 搬运规则

`transfers` 把某个目录的内容映射到另一个输出位置。每条规则包含：

| 字段 | 必填 | 说明 |
|---|---|---|
| `source` | 是 | 源路径。`.` 或 `/` 表示路由目录自身；其它值拼接到路由 `path` 之下。 |
| `target` | 是 | 目标输出目录，作为该目录的对外路径。 |

示例：

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
- 根路由 `path: "/"` 用于定义站点标题/副标题/简介，其 `transfers` 无效。
- 路径中包含 `private` 段的路由会被忽略并打印警告；`private` 目录本身也不会进入源目录树。

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
| `{{ with_cdn("/css/site.css") }}` | 用 `protocol + domain_name + path_prefix` 拼接绝对 URL |

## 输出

```text
dist/
├── html/            # 静态 HTML 站点（目录页、文章、stats、friends、sitemap、meta）
└── pdf/             # pdf_engine 非空且不为 disable 时生成
```
