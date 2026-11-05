# 8. 资源安装与数据目录

实现：`common/common/paths/paths.h`、`common/common/paths/paths.cpp`、`camus/src/yaml_config.cpp`、`camus/src/writer.cpp`、`camus/CMakeLists.txt`。

> 旧版的「单文件自包含」已整体移除：可执行文件不再追加示例压缩包，也不再提供 `-i/--install` 与 `-f/--force`，`common/common/extract`（libarchive）与 `hack/post_build.sh` 均已删除。

## 8.1 数据目录

运行期资源根目录由 `camus::paths` 解析（`paths.cpp`）：

| 平台 | 默认数据目录 | 回退 |
|---|---|---|
| macOS | `~/Library/Application Support/Camus`（`$HOME` 存在时） | `/Library/Application Support/Camus` |
| Linux | `/var/lib/camus` | 无 |

- 常量来源：`config.h.in` 的 `CAMUS_DATA_DIR_MACOS`（`Library/Application Support/Camus`）与 `CAMUS_DATA_DIR_LINUX`（`/var/lib/camus`）。
- `data_dirs()` 返回按优先级排列的候选目录；`find(relative)` 返回第一个存在的候选路径。
- 数据目录只读即可：构建过程不会向其写入。

目录内容（由安装包写入）：

```text
<数据目录>/
├── theme/{default,document,pixel,terminal}/{home,page,stats,friends}.html
├── assets/                    # 默认静态资源（css/js/fonts/robots.txt 等）
├── posts/                     # 示例文章
└── camus.yaml                 # 示例配置
```

## 8.2 主题查找顺序

`yaml_config.cpp` 的 `read_theme()`：

1. 工作目录 `theme/{theme_name}/{type}.html` 存在则使用（本地覆盖）。
2. 否则按 `paths::find("theme/{theme_name}/{type}.html")` 在数据目录候选中查找。
3. 均不存在时 `logging::fatal("read theme content failed name=...")`，退出 127。

## 8.3 静态资源查找顺序

`writer::emit_assets()`：

1. 默认资源目录 `paths::default_assets_dir()`：第一个存在且非空的数据目录 `assets/`。
2. 本地 `./assets`：存在且非空时整体覆盖默认资源。
3. 两者都不可用时记录 debug 并跳过。
4. 资源最终提升到输出根目录（详见 §6.9）。

## 8.4 安装位置（CPack）

`camus/CMakeLists.txt` 安装 `example/` 下的 `theme/`、`assets/`、`posts/` 与 `camus.yaml`：

| 平台 / 生成器 | 资源安装目标 |
|---|---|
| Linux（DEB） | `var/lib/camus`（最终 `/var/lib/camus`） |
| macOS（productbuild，当前用户域） | `Library/Application Support/Camus` → `~/Library/Application Support/Camus` |
| macOS（productbuild，系统域，如 `installer -target /`） | `Library/Application Support/Camus` → `/Library/Application Support/Camus` |

- 可执行文件仍安装到 `/usr/local/bin/camus`。
- macOS 通过 `CPACK_PRODUCTBUILD_DOMAINS` 同时启用用户域与系统域；用户域安装到 `~/Library/...`，系统域安装到 `/Library/...`，运行期按 §8.1 的顺序查找。

## 8.5 持久性

- 资源安装结果持久于数据目录；升级安装包会覆盖同名文件。
- 工作目录的本地 `theme/` 与 `./assets` 不参与安装，始终优先于数据目录。
- 不再有 `.camus/.version`、自解压逻辑或安装版本检查。
