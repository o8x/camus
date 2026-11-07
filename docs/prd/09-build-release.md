# 9. 构建、测试与发布

实现：根 `CMakeLists.txt`、`camus/CMakeLists.txt`、`common/CMakeLists.txt`、`Makefile`、`.github/workflows/auto-release-tag.yml`。

## 9.1 环境要求

| 依赖 | 说明 |
|---|---|
| CMake ≥ 3.28.3 | 根 `cmake_minimum_required` |
| Ninja | Makefile 与 CI 使用的生成器 |
| GCC / Clang（C++23） | 根 CMake 将 `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` 固定为 `/usr/bin/gcc`、`/usr/bin/g++` |
| Git | 读取分支与提交 hash；拉取 submodule |
| mold | Linux Release 链接器（`-fuse-ld=mold`） |
| pkg-config + OpenSSL | `cpp-httplib` 的 HTTPS 支持（`--update`）；`common/CMakeLists.txt` 通过 `pkg_check_modules(OPENSSL ...)` 引入，macOS 另链接 `Security` / `CoreFoundation` |

依赖来源：`third-party/` 下 yaml-cpp、cmark-gfm、nlohmann_json、inja、cpp-httplib；OpenSSL 由系统提供（Linux 安装 `libssl-dev`，macOS 安装 `openssl@3` 并设置 `PKG_CONFIG_PATH`）。

## 9.2 CMake 配置要点

根 `CMakeLists.txt`：

- `project(Camus VERSION ...)`（具体版本号只在 `docs/PRD.md` 项目表与 `CMakeLists.txt` 中定义），`CMAKE_CXX_STANDARD 23`。
- 通过 Git 获取 `GIT_COMMIT_LONG`、`GIT_BRANCH`；未找到 Git 时为 `unknown`。
- Release 构建追加：`-O3 -ffast-math -falign-functions=64 -flto=auto`；macOS 加 `-march=native`；Linux 加 `-march=x86-64-v3 -fuse-ld=mold -fno-semantic-interposition`，链接选项 `--hash-style=gnu,--as-needed,--gc-sections,--print-gc-sections`。
- `WORK_ON_DEBUG`：Debug/RelWithDebInfo 为 `true`，Release 为 `false`。
- 构建顺序：`camus`、`common`、第三方库（`EXCLUDE_FROM_ALL SYSTEM`，作为系统组件，其头文件以 `-isystem` 引入以抑制警告）。

`camus/CMakeLists.txt`：

- 静态库 `camus`（`src/*`）+ 可执行文件 `Camus`，输出名为 `camus`，目录 `${CMAKE_BINARY_DIR}/bin`。
- 安装 `example/` 的 `theme/`（含 `theme/pdf/`）、`assets/`、`posts/`（含各目录的 `SUMMARY.md`）、`camus.yaml`、`.gitignore` 与 `Justfile` 到平台数据目录（Linux `var/lib/camus`，macOS `Library/Application Support/Camus`，见 §8.4）。
- 注册 CTest `build-test-site`：在源码根目录执行 `${CMAKE_BINARY_DIR}/bin/camus -d site`，超时 10 秒。
- CPack 配置见 §9.5。

`common/CMakeLists.txt`：

- 收集 `common/common/*` 源文件与 `cpp-httplib`，不再依赖 `libarchive`。
- `cmark-gfm`（源码 / 构建目录）与 `cpp-httplib` 的 include 目录通过 `target_include_directories(... SYSTEM PUBLIC ...)` 标记为系统头文件；第三方库自身的 include 由 `add_subdirectory(... SYSTEM)` 传播为系统头文件。
- 生成 `build/build.h`（由 `config.h.in` 配置），包含版本、CMake 版本、构建时间、构建类型、C++ 标准、Git 信息与 `CAMUS_LOCAL_DATA_DIR=".camus"`、`CAMUS_DATA_DIR_MACOS="Library/Application Support/Camus"`、`CAMUS_DATA_DIR_LINUX="/var/lib/camus"`、`CAMUS_CONF_NAME="camus.yaml"`、`CAMUS_SUMMARY_NAME="SUMMARY.md"`、`CAMUS_OPEN_TARGET_PROJECT="project"`、`CAMUS_OPEN_TARGET_RUNTIME="runtime"`、`CAMUS_PRIVATE_DIR="private"`、`CAMUS_PDF_THEME_DIR="theme/pdf"`、`CAMUS_PDF_TEMPLATE_NAME="page.html"`、`CAMUS_PDF_STYLE_NAME="style.css"`、`CAMUS_PDF_FONTS_NAME="fonts.css"`、`CAMUS_PDF_FONTS_DIR="fonts"`。

## 9.3 Makefile 目标

| 目标 | 行为 |
|---|---|
| `make build` | `cmake -DCMAKE_BUILD_TYPE=Release -G Ninja -B build/release -S .` 后构建所有目标 |
| `make package` | Release + 按平台选择 CPack 生成器（Linux `DEB`，macOS `productbuild`），构建并打包 |
| `make serve` | 构建后运行 `ctest -R build-test-site`，再用 `python3 -m http.server` 在 `localhost:8000` 提供 `site/dist/html` |
| `make push` | 先跑 `build-test-site`，再在 `site/dist/html` 初始化 git、提交并强制推送到 `REPO_URL`（默认 `git@github.com:o8x/o8x.github.io.git`） |
| `make all` | 等价 `build` |

可变参数：`SITE_ROOT` 默认 `site`，`REPO_URL` 可覆盖。

## 9.4 测试

- `ctest --extra-verbose -R build-test-site --test-dir build/release`（`make serve` 已包含）。
- 测试内容：对 `site` 目录执行 `camus -d --build site`（调试日志），验证默认构建链路。
- CMake 复制 `LICENSE`、`README.md` 到构建目录供 CPack 使用。

## 9.5 打包（CPack）

关键变量（`camus/CMakeLists.txt`）：

| 变量 | 值 |
|---|---|
| `CPACK_PACKAGE_NAME` | `Camus` |
| `CPACK_PACKAGE_VERSION` | 继承 `project(... VERSION ...)` |
| `CPACK_PACKAGE_VENDOR` / `CONTACT` | `https://github.com/o8x` 相关地址 |
| `CPACK_OUTPUT_FILE_PREFIX` | `${CMAKE_BINARY_DIR}/packages` |
| `CPACK_PACKAGE_FILE_NAME` | `${NAME}-v${VERSION}_${OS_NAME}${OS_RELEASE}-${PROCESSOR}`，整体转小写 |

- Linux：可执行文件安装到 `./usr/local/bin`，资源安装到 `var/lib/camus`，生成 DEB（`CPACK_MONOLITHIC_INSTALL ON`）。
- macOS `productbuild`：可执行文件安装到 `./usr/local/bin`（组件 `Runtime`），资源安装到 `Library/Application Support/Camus`（组件 `Resources`），identifier `io.github.the-writer.Camus`。
- macOS `productbuild` 启用用户域与系统域：用户域落到 `~/Library/Application Support/Camus`，系统域（如 `installer -target /`）落到 `/Library/Application Support/Camus`。
- macOS `DragNDrop` 分支存在但当前 Makefile 默认用 `productbuild`。
- 产物命名示例：`camus-v{PROJECT_VERSION}_ubuntu24.04-x86_64.deb`（实际版本号见 `CMakeLists.txt`，`OS_RELEASE` 由主机决定）。

## 9.6 CI 工作流

`.github/workflows/auto-release-tag.yml`（名称「CMake on multiple platforms」）触发：`push` 到 `main`、`v*.*.*` tag、指向 `main` 的 PR。

| Job | Runner | 主要步骤 | 产物 |
|---|---|---|---|
| `build-ubuntu` | `ubuntu-24.04` 容器 | 安装依赖（含 `mold`、`libssl-dev`）→ submodule → `make package` → `dpkg -i` + `camus -v` + `camus --build -dw "$GITHUB_WORKSPACE/example"` + `camus -iw /tmp/site` + `camus -cw /tmp/site-copy` | `build/release/packages` |
| `build-macos` | `macos-15` | brew 安装依赖（含 `openssl@3`，并设置 `PKG_CONFIG_PATH`）→ submodule → `make package` → `sudo installer -pkg *.pkg -target /` + `camus -v` + `camus --build -dw "$GITHUB_WORKSPACE/example"` + `camus -iw /tmp/site` | `build/release/packages` |
| `release` | `ubuntu-24.04` | 仅在 tag 上运行：下载并合并产物 → 生成 `latest.json`（`version` / `tag` / `published_at` / `assets`）→ `softprops/action-gh-release` 发布（含 `latest.json` 与 `*.dmg/*.pkg/*.rpm/*.deb/*.exe`） | GitHub Release |
| `pages` | `ubuntu-24.04` 容器 | 仅在 tag 上运行且 tag 提交位于 `main` 历史时部署：安装 git → 全量 checkout → `safe.directory` 后校验 tag 在 `main` → `actions/configure-pages` 取得 `base_url` 并经 `CAMUS_PUBLIC_URL` 注入 → 安装 deb → `camus --build -dw example`（`CAMUS_AUTO_CHECK_UPDATE=false`）→ 上传并部署 GitHub Pages | `example/dist/html` |

`build-ubuntu` / `build-macos` 的 Install Test 与 pages 构建均显式使用 `--build`（仅 `-dw` 不会触发构建），并设置 `CAMUS_AUTO_CHECK_UPDATE=false` 避免 CI 额外发起更新检查。

依赖缓存：`build-macos` 使用 `actions/cache` 缓存 `~/Library/Caches/Homebrew`（key `macOS-homebrew-v1`），并移除了每次运行的 `brew update`；缓存 key 带手动版本号，依赖列表变化时可递增该版本使缓存失效。`build-ubuntu` 不缓存 apt，安装依赖后再 checkout（确保容器内 `git` 可用并生成真实 `.git` 仓库）。

工作流权限：`contents: write`、`pages: write`、`id-token: write`；`pages` concurrency。

`release` 与 `pages` 仅在 tag 推送（`github.ref_type == 'tag'`）时运行；`pages` 还要求 tag 提交是 `main` 的祖先，否则只执行构建 / 测试，不部署。`main` 分支推送与 PR 只运行 `build-ubuntu` / `build-macos`。

## 9.7 版本变更

- 版本号只定义在 `docs/PRD.md` 项目表与 `CMakeLists.txt` 的 `project(... VERSION ...)`，两者保持一致。
- 修改版本号后只需同步上述两处；AGENTS 与其它章节不写入具体版本号，需要引用时使用 `v{PROJECT_VERSION}` 等占位形式。
- CMake 会把版本编入 `build/build.h`，并通过 `-v` 与模板变量 `{{ build.version }}` 对外体现。
