# 8. 资源安装与数据目录

实现：`camus/src/install.h`、`camus/src/install.cpp`、`common/common/paths/paths.h`、`common/common/paths/paths.cpp`、`camus/src/yaml_config.cpp`、`camus/src/writer.cpp`、`camus/CMakeLists.txt`。

> 旧版的「单文件自包含」已移除：可执行文件不再追加示例压缩包。`-i/--install` 只把示例配置与文章安装到工作目录根部；把完整资源复制到 `.camus` 改用 `-c/--copy`。

## 8.1 数据目录

运行期资源根目录由 `camus::paths` 解析（`paths.cpp`）：

| 平台 | 默认数据目录 | 回退 |
|---|---|---|
| macOS | `~/Library/Application Support/Camus`（`$HOME` 存在时） | `/Library/Application Support/Camus` |
| Linux | `/var/lib/camus` | 无 |

- 常量来源：`config.h.in` 的 `CAMUS_DATA_DIR_MACOS`（`Library/Application Support/Camus`）与 `CAMUS_DATA_DIR_LINUX`（`/var/lib/camus`）。
- `data_dirs()` 返回按优先级排列的候选目录；`find(relative)` 返回第一个存在的候选路径。
- `source_data_dir()` 返回第一个同时包含 `theme/` 与 `assets/` 的数据目录，供 `-c` 使用。
- `paths::find(CAMUS_CONF_NAME)` 定位包含 `camus.yaml` 的数据目录，供 `-i` 使用。
- 数据目录只读即可：构建过程不会向其写入。

目录内容（由安装包写入）：

```text
<数据目录>/
├── theme/{default,document,pixel,terminal}/{home,page,stats,friends}.html
├── theme/pdf/{page.html,style.css,fonts.css}   # PDF 模板、打印样式与字体样式
├── theme/pdf/fonts/                    # 自定义 PDF 字体文件（构建时复制到 dist/pdf/fonts）
├── assets/                    # 默认静态资源（css/js/fonts/robots.txt 等）
│   ├── css/style.css          # 公共样式入口，默认空文件
│   └── js/onload.js           # 公共脚本入口，默认空文件
├── posts/                     # 示例文章
│   └── SUMMARY.md              # 示例站点与目录元信息
├── camus.yaml                 # 示例配置
└── .gitignore                 # 示例忽略规则
```

## 8.2 安装与复制

### 8.2.1 `-i/--install`（示例文件）

`install_templates()` 从 `paths::find("camus.yaml")` 所在的数据目录，把以下内容复制到工作目录根部（已存在的同名路径跳过）：

- `posts/`（含各目录的 `SUMMARY.md`）
- `camus.yaml`
- `.gitignore`

```mermaid
flowchart TD
    A[查找数据目录 camus.yaml] -->|未找到| X[报错, 返回 1]
    A -->|找到| B[逐个检查 posts/camus.yaml/.gitignore]
    B --> C{工作目录已存在?}
    C -->|是| S[跳过该项]
    C -->|否| D[复制到工作目录]
    S --> OK[返回 0, 继续构建]
    D --> OK
```

- 不会创建或修改 `.camus`。
- 成功后不退出，主程序继续构建（见 §2.4）。

### 8.2.2 `-c/--copy`（完整资源）

`copy_resources()` 把 `source_data_dir()` 的全部内容复制到 `<workdir>/.camus`：

```mermaid
flowchart TD
    A[解析 source_data_dir] -->|无资源| X[报错, 返回 1]
    A -->|找到| B{.camus 已存在且非空?}
    B -->|否| D[创建 .camus 并复制全部文件]
    B -->|是且无 -f| W[WARN 已复制, 可继续构建]
    B -->|是且有 -f| C[empty_path .camus 后复制]
    D --> S[补齐工作目录 posts/camus.yaml/.gitignore]
    C --> S
    W --> S
    S --> OK[返回 0, 继续构建]
```

- `.camus` 已存在且非空、未指定 `-f`：打印 WARN，仅补齐工作目录根部示例文件，不覆盖 `.camus`。
- `-f`：先删除 `.camus`（受 §6.1 的 >200 文件保护），再复制。
- 复制完成后，对 `posts`、`camus.yaml`、`.gitignore`：仅当工作目录根部不存在同名路径时才从 `.camus` 复制。
- 复制结果持久于磁盘；`-c` 成功后不会退出，主程序继续构建（见 §2.4）。

## 8.3 资源查找顺序

复制完整资源后优先使用工作目录 `.camus`；未复制时使用平台数据目录。

主题（`yaml_config.cpp` 的 `read_theme()`）：

1. 工作目录 `theme/{theme_name}/{type}.html`（显式本地覆盖）。
2. `paths::find_resource("theme/{theme_name}/{type}.html")`：先 `.camus/theme/...`，再数据目录。
3. 均不存在时 `logging::fatal("read theme content failed name=...")`，退出 127。

PDF 模板、样式与字体（`writer.cpp` 的 `read_pdf_theme_file()` / `copy_pdf_theme_fonts()`）：

1. 工作目录 `theme/pdf/{page.html,style.css,fonts.css}`（显式本地覆盖，可只覆盖单个文件）。
2. `paths::find_resource("theme/pdf/<file>")`：先 `.camus/theme/pdf/...`，再数据目录。
3. `page.html` / `style.css` 均不存在时 `logging::fatal("read pdf theme content failed name=...")`，退出 127；`fonts.css` 可选。
4. `theme/pdf/fonts/` 按同一顺序查找，存在时递归复制到 `dist/pdf/fonts/`。

静态资源（`writer::emit_assets()`）：

1. 默认资源目录 `paths::default_assets_dir()`：优先 `.camus/assets`，其次数据目录 `assets/`。
2. 本地 `./assets`：存在且非空时整体覆盖默认资源。
3. 两者都不可用时记录 debug 并跳过。
4. 资源最终提升到输出根目录（详见 §6.9）。

## 8.4 安装位置（CPack）

`camus/CMakeLists.txt` 安装 `example/` 下的 `theme/`、`assets/`、`posts/`（含各目录的 `SUMMARY.md`）、`camus.yaml` 与 `.gitignore`：

| 平台 / 生成器 | 资源安装目标 |
|---|---|
| Linux（DEB） | `var/lib/camus`（最终 `/var/lib/camus`） |
| macOS（productbuild，当前用户域） | `Library/Application Support/Camus` → `~/Library/Application Support/Camus` |
| macOS（productbuild，系统域，如 `installer -target /`） | `Library/Application Support/Camus` → `/Library/Application Support/Camus` |

- 安装 `example/theme` 的整个目录，因此包含 `theme/pdf/{page.html,style.css,fonts.css}` 与 `theme/pdf/fonts/`。
- 可执行文件仍安装到 `/usr/local/bin/camus`。
- macOS 通过 `CPACK_PRODUCTBUILD_DOMAINS` 同时启用用户域与系统域；用户域安装到 `~/Library/...`，系统域安装到 `/Library/...`，运行期按 §8.1 的顺序查找。

## 8.5 持久性

- `-i` 安装的根部示例文件持久于工作目录；`-c` 复制结果持久于工作目录 `.camus`；`-f` 可重装覆盖 `.camus`。
- 数据目录中的资源由安装包持久化；升级安装包会覆盖同名文件。
- 工作目录的 `theme/` 与 `./assets` 始终拥有最高优先级。
