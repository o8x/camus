# Camus PRD 索引

> 本文件是 Camus 行为的唯一事实来源入口（索引）。完整行为按一级章节拆分在 `docs/prd/NN-slug.md`，本文件只维护项目表、核心设计、阅读指南与维护规则。

## 项目表

| 项 | 值 |
|---|---|
| 项目名称 | Camus |
| 项目描述 | 使用 C++23 编写的静态站点生成器，将 Markdown 渲染为静态 HTML |
| 当前版本 | 15.4.0 |
| 语言标准 | C++23 |
| 构建系统 | CMake 3.28.3+ / Ninja / Make |
| 版本来源 | `CMakeLists.txt` 中 `project(Camus VERSION ...)`，与本表「当前版本」保持一致 |
| 许可证 | 见仓库根目录 `LICENSE` |
| 仓库地址 | https://github.com/o8x/camus |

## 核心设计

- **外部资源安装**：主题、默认静态资源与示例模板由安装包放到平台数据目录（macOS `~/Library/Application Support/Camus`，Linux `/var/lib/camus`）；`-i` 安装示例文件与 `posts/private/`，`-c` 默认只复制最小资源骨架，`-c --full` 复制完整资源；静态资源按数据目录 → `.camus` → 本地 `./assets` 叠加（详见 §8）。
- **流水线式构建**：`writer::build()` 依次执行 `emit_article → emit_toc → emit_friends → emit_sitemap → emit_assets → flush_writes → emit_pdf`，所有文件写入先进入内存队列统一落盘，最后按需调用 Chrome/Edge 生成 PDF，封面取自根 `SUMMARY.md` 的 `cover_image`，并注入可交互书签大纲（详见 §1.4、§6）。
- **配置驱动**：`camus.yaml` 提供站点、输出、渲染、公开地址、友链与目录搬运配置，各目录下的 `SUMMARY.md` 提供目录页元信息（标题/副标题/描述）；配置每次构建重新加载（详见 §3）。源目录固定按 `src` → `posts` 探测，资源目录固定 `./assets`，输出固定 `./dist/html` 与 `./dist/pdf`。
- **编辑/运行目录直达**：`--open` 在构建成功后用 VS Code 打开工作目录，`--open runtime` 用系统文件管理器打开数据目录（详见 §2）。
- **内存目录树**：构建开始时扫描源目录（`src`/`posts`）生成 `catalog_node` 树，文章 Front Matter、路由映射、导航、统计都在该树上完成（详见 §4）。
- **模板引擎**：HTML 使用 inja，Markdown 使用 cmark-gfm；主题由 `theme/{name}/{home,page,stats,friends}.html` 组成，PDF 模板、样式与字体位于 `theme/pdf/{page.html,style.css,fonts.css,fonts/}`，本地 `theme/` 可覆盖数据目录中的默认主题与 PDF 资源（详见 §5）。
- **安全清理**：构建前清空输出目录；当输出目录文件数超过 200 时拒绝自动删除并以致命错误退出，避免误删（详见 §6.1）。
- **干跑模式**：`-D/--dryrun` 下所有写文件操作被跳过，用于检查配置与渲染流程（详见 §6.10）。

## 文档清单与阅读指南

| 文件 | 一级章节 | 何时阅读 |
|---|---|---|
| `docs/prd/01-overview.md` | 1. 产品概述 | 了解定位、技术栈、资源目录、整体流水线 |
| `docs/prd/02-command-line.md` | 2. 命令行接口 | 改动 CLI 选项、退出码、启动流程时 |
| `docs/prd/03-configuration.md` | 3. 配置系统 | 改动 `camus.yaml` 与模板可用变量的解析时 |
| `docs/prd/04-catalog-and-posts.md` | 4. 内容目录树与文章解析 | 改动扫描、Front Matter、可见性、路由、URL、统计时 |
| `docs/prd/05-rendering.md` | 5. 模板与渲染 | 改动 Markdown/HTML 渲染、主题、inja 函数、渲染数据时 |
| `docs/prd/06-output.md` | 6. 输出产物 | 改动页面、目录页、stats、friends、sitemap、meta、assets 输出时 |
| `docs/prd/07-watch-mode.md` | 7. 监听模式与本地服务 | 改动 `-W` 热更新与服务端口时 |
| `docs/prd/08-template-install.md` | 8. 资源安装与数据目录 | 改动资源安装位置、数据目录解析、主题/资源查找时 |
| `docs/prd/09-build-release.md` | 9. 构建、测试与发布 | 改动 CMake、Makefile、CTest、CPack、CI 时 |
| `docs/prd/10-common-utilities.md` | 10. 公共模块与工具 | 改动字符串、文件、日志、网络、解压等公共能力时 |

## 变更类型 → 需同步文件

| 变更类型 | 需同步文件 |
|---|---|
| 新增/修改/删除 CLI 选项、退出码 | `02-command-line.md`；必要时 `09-build-release.md`（CI 调用） |
| 新增/修改配置字段、默认值、模板变量 | `03-configuration.md`；涉及渲染数据时同步 `05-rendering.md`、`06-output.md` |
| 改动扫描、Front Matter、可见性、路由、URL | `04-catalog-and-posts.md`；涉及产出路径时同步 `06-output.md` |
| 改动 Markdown/HTML 渲染、主题、inja 函数 | `05-rendering.md`；涉及模板数据结构时同步 `06-output.md` |
| 改动输出文件、目录清理、dryrun 行为 | `06-output.md` |
| 改动监听/本地服务 | `07-watch-mode.md` |
| 改动资源安装、数据目录、主题/资源解析 | `08-template-install.md`；必要时同步 `05-rendering.md`、`06-output.md` |
| 改动构建脚本、测试、打包、CI、产物命名 | `09-build-release.md`；版本变更时同步本索引项目表与 `CMakeLists.txt` |
| 改动公共函数签名/行为 | `10-common-utilities.md`；被调用章节同步 |
| 版本号变更 | 本索引「项目表」与 `CMakeLists.txt`；AGENTS 与其它章节不写入具体版本号 |

## 维护规则

- 文档与实现不一致时以代码为准：先核对实现，再修文档。
- 改代码必须同步修改对应章节文件；索引仅在文件清单、阅读指南、维护规则或项目信息变化时更新。
- 新增一级章节必须新建 `docs/prd/NN-slug.md`（编号与章节号一致），在本索引登记，并遵守以下书写要求。
- 书写要求：章节化、可被实现直接对照；界面/布局数值用表格；多步流程/状态机/时序用 mermaid；对外接口写清签名、可用范围、是否持久（刷新/重启是否丢失）、副作用条件与示例；数值与代码常量一致并注明来源。
- 当前版本只记录在本索引「项目表」；`docs/prd/*.md` 与示例不写死具体版本号，需要引用时使用 `v{PROJECT_VERSION}` 等占位形式，或注明「以本索引项目表 / `CMakeLists.txt` 为准」。
