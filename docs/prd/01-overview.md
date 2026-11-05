# 1. 产品概述

## 1.1 定位

Camus 是一个用 C++23 编写的静态站点生成器：读取 Markdown 文章与主题模板，生成静态 HTML 站点。核心特性：

- 外部资源安装：主题、默认静态资源与示例模板由安装包放到平台数据目录，运行期从该目录读取。
- Markdown（cmark-gfm）渲染，支持 Front Matter 与隐藏片段。
- 多主题：数据目录内置 `default`、`document`、`pixel`、`terminal` 等主题，本地 `theme/` 可覆盖或自定义。
- 目录树与目录索引页、统计页、友链页、`sitemap.xml`、`meta.json` 生成。
- 可选 PDF 输出：把全部可见文章合并为单个 Markdown，经 cmark-gfm 转 HTML 后调用 Chrome/Edge 无头模式打印为 PDF。
- `-W` 监听模式：轮询文件变化并自动重建，同时提供本地 HTTP 服务。
- `private` 目录：源目录下任意名为 `private` 的子目录不参与扫描与生成，也不能作为目录页路由。

当前版本：**11.1.0**（来源：`CMakeLists.txt` 的 `project(... VERSION 11.1.0)`）。

## 1.2 技术栈与依赖

| 组件 | 用途 | 来源 |
|---|---|---|
| C++23 | 实现语言 | 根 `CMakeLists.txt` 设置 `CMAKE_CXX_STANDARD 23` |
| cmark-gfm | Markdown → HTML | `third-party/cmark-gfm` |
| yaml-cpp | 读取 `camus.yaml` | `third-party/yaml-cpp` |
| inja | HTML 模板渲染 | `third-party/inja` |
| nlohmann/json | JSON 数据模型与输出 | `third-party/nlohmann_json` |
| cpp-httplib | 监听模式本地 HTTP 服务 | `third-party/cpp-httplib` |
| CPack | 生成 deb/pkg 等安装包，并安装主题/资源/示例模板 | `camus/CMakeLists.txt` |
| Chrome / Edge（可选） | 无头模式把中间 HTML 打印为 PDF | 系统已安装的浏览器，按 `render.pdf_engine` 解析 |

依赖全部以 Git submodule 形式存在（`.gitmodules`），构建时需 `git submodule update --init --recursive`。

## 1.3 资源数据目录

运行期资源目录（`common/common/paths/paths.cpp`）：

| 平台 | 默认数据目录 | 说明 |
|---|---|---|
| macOS | `~/Library/Application Support/Camus` | `$HOME` 缺失时回退 `/Library/Application Support/Camus` |
| macOS（回退） | `/Library/Application Support/Camus` | 安装到系统域（如 `installer -target /`）时的位置 |
| Linux | `/var/lib/camus` | 由 deb 安装包写入 |

目录内容为 `theme/`、`assets/`、`posts/` 与 `camus.yaml` 示例模板。查找规则：

- 主题：工作目录 `theme/{name}/{type}.html` 优先，其次数据目录同名文件。
- 静态资源：工作目录 `./assets` 优先，缺失或为空时使用数据目录 `assets/`。

构建不再向可执行文件追加模板，也不再提供 `-i`/`-f` 选项（详见 §8）。

## 1.4 构建流水线

`writer::build()`（`camus/src/writer.cpp:800`）的执行顺序：

```mermaid
flowchart TD
    A[reload 配置 camus.yaml] --> B[切换工作目录 workdir]
    B --> C[扫描 source_dir（src/posts）构建目录树]
    C -->|空| W[WARN No articles found, 返回 1]
    C -->|非空| D[emit_article]
    D --> E[emit_toc]
    E --> F[emit_friends]
    F --> G[emit_sitemap]
    G --> H[emit_assets]
    H --> I[flush_writes 统一落盘]
    I --> P{配置了 pdf_engine?}
    P -->|是| Q[emit_pdf: 合并 Markdown + 封面/扉页/目录 -> HTML -> Chrome/Edge 打印]
    P -->|否| J
    Q --> J[INFO build complete, 返回 0]
```

`emit_article` 内部又分为：路由解析 → Front Matter 解析 → 清空输出目录 → 收集文章 → 填充上一篇/下一篇 → 计算 TOC → 逐篇渲染与统计 → 统计页 → 目录属性收尾（详见 §4、§6）。

## 1.5 关键术语

| 术语 | 含义 |
|---|---|
| workdir | 工作目录，`-w` 指定，默认当前目录；运行时会 `chdir` 到该目录 |
| source_dir | Markdown 源目录；固定按 `src` → `posts` 探测，不存在则回退 `src` |
| dest_dir | HTML 输出目录，固定 `./dist/html` |
| pdf_dir | PDF 输出目录，固定 `./dist/pdf` |
| assets_dir | 本地静态资源目录，固定 `./assets`；缺省时使用数据目录的 `assets/`，内容会被提升到输出根目录 |
| catalog | 运行期内存中的目录树 `catalog_node` |
| route | `camus.yaml` 中 `catalog` 段的目录配置，可设置标题/副标题/描述/搬运 |
| transfer | 路由内的文件搬运映射，把源目录内容映射到另一输出路径 |
| short-path | 文章短链接标识，输出到 `/s/{short_path}.html` |
| hidden | 文章可见性：不渲染、不进入输出目录树 |
| private | 源目录下任意层级名为 `private` 的子目录；不扫描、不生成、不可作为目录页路由 |
| 隐藏片段 | Markdown 中 `--hidden-section-start` 与 `--hidden-section-end` 之间不渲染的内容 |

## 1.6 版本信息

| 项 | 值 | 来源 |
|---|---|---|
| 版本号 | 11.1.0 | `CMakeLists.txt` |
| 主版本 | 10 | `PROJECT_VERSION_MAJOR` |
| 版本输出示例 | `v11.1.0 Release with CMake 3.28.3 and C++ 23` | `camus/main.cpp:110` |
| 构建信息 | `Build Time: <时间> hash=<git 40 位 hash>` | `config.h.in` + `camus/main.cpp:113` |
