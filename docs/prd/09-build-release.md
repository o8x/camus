# 9. 构建、测试与发布

实现：根 `CMakeLists.txt`、`camus/CMakeLists.txt`、`common/CMakeLists.txt`、`Makefile`、`.github/workflows/auto-release-tag.yml`。

## 9.1 环境要求

| 依赖 | 说明 |
|---|---|
| CMake ≥ 3.28.3 | 根 `cmake_minimum_required` |
| Ninja | Makefile 与 CI 使用的生成器 |
| GCC / Clang（C++23） | 根 CMake 将 `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER` 固定为 `/usr/bin/gcc`、`/usr/bin/g++` |
| Git | 读取分支与提交 hash；拉取 submodule |
| pkg-config + libarchive | `common` 链接 `libarchive` |
| `xxd` | `post_build.sh` 写入长度字段；CI 显式安装 |
| mold | Linux Release 链接器（`-fuse-ld=mold`） |

依赖来源：`third-party/` 下 yaml-cpp、cmark-gfm、nlohmann_json、inja、cpp-httplib。

## 9.2 CMake 配置要点

根 `CMakeLists.txt`：

- `project(Camus VERSION 11.1.0 ...)`，`CMAKE_CXX_STANDARD 23`。
- 通过 Git 获取 `GIT_COMMIT_LONG`、`GIT_BRANCH`；未找到 Git 时为 `unknown`。
- Release 构建追加：`-O3 -ffast-math -falign-functions=64 -flto=auto`；macOS 加 `-march=native`；Linux 加 `-march=x86-64-v3 -fuse-ld=mold -fno-semantic-interposition`，链接选项 `--hash-style=gnu,--as-needed,--gc-sections,--print-gc-sections`。
- `WORK_ON_DEBUG`：Debug/RelWithDebInfo 为 `true`，Release 为 `false`。
- 构建顺序：`camus`、`common`、第三方库（`EXCLUDE_FROM_ALL`）。

`camus/CMakeLists.txt`：

- 静态库 `camus`（`src/*`）+ 可执行文件 `Camus`，输出名为 `camus`，目录 `${CMAKE_BINARY_DIR}/bin`。
- `POST_BUILD` 调用 `hack/post_build.sh "${CMAKE_SOURCE_DIR}"` 嵌入模板（见 §8.6）。
- 注册 CTest `build-test-site`：在源码根目录执行 `${CMAKE_BINARY_DIR}/bin/camus -d site`，超时 10 秒。
- CPack 配置见 §9.5。

`common/CMakeLists.txt`：

- 收集 `common/common/*` 源文件与 `cpp-httplib`。
- `pkg_check_modules(ARCHIVE REQUIRED libarchive)`；macOS 下将 Homebrew 的 libarchive pkgconfig 路径加入 `PKG_CONFIG_PATH`。
- 生成 `build/build.h`（由 `config.h.in` 配置），包含版本、CMake 版本、构建时间、构建类型、C++ 标准、Git 信息与 `CAMUS_DIR=".camus"`、`CAMUS_CONF_NAME="camus.yaml"`、`CAMUS_PRIVATE_DIR="private"`。

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
- 测试内容：对 `site` 目录执行 `camus -d site`（调试日志），验证默认安装/构建链路。
- CMake 复制 `LICENSE`、`README.md` 到构建目录供 CPack 使用。

## 9.5 打包（CPack）

关键变量（`camus/CMakeLists.txt`）：

| 变量 | 值 |
|---|---|
| `CPACK_PACKAGE_NAME` | `Camus` |
| `CPACK_PACKAGE_VERSION` | `11.1.0` |
| `CPACK_PACKAGE_VENDOR` / `CONTACT` | `https://github.com/o8x` 相关地址 |
| `CPACK_OUTPUT_FILE_PREFIX` | `${CMAKE_BINARY_DIR}/packages` |
| `CPACK_PACKAGE_FILE_NAME` | `${NAME}-v${VERSION}_${OS_NAME}${OS_RELEASE}-${PROCESSOR}`，整体转小写 |

- Linux：安装到 `./usr/local/bin`，生成 DEB。
- macOS `productbuild`：安装到 `./usr/local/bin`，组件 `Runtime`，identifier `cn.com.stdout.Camus`。
- macOS `DragNDrop` 分支存在但当前 Makefile 默认用 `productbuild`。
- 产物命名示例：`camus-v11.1.0_ubuntu24.04-x86_64.deb`（实际 `OS_RELEASE` 由主机决定）。

## 9.6 CI 工作流

`.github/workflows/auto-release-tag.yml`（名称「CMake on multiple platforms」）触发：`push` 到 `main`、`v*.*.*` tag、指向 `main` 的 PR。

| Job | Runner | 主要步骤 | 产物 |
|---|---|---|---|
| `build-ubuntu` | `ubuntu-24.04` 容器 | 安装依赖（含 `xxd`、`mold`、`libarchive-dev`）→ submodule → `make package` → `dpkg -i` + `camus -v` + `camus -iIw /tmp/site` + `camus -idw /tmp/site` | `build/release/packages` |
| `build-macos` | `macos-14` | brew 安装依赖 → submodule → `make package` → `sudo installer -pkg *.pkg -target /` + 同样冒烟命令 | `build/release/packages` |
| `release` | `ubuntu-24.04` | 下载并合并产物；仅在 tag 上通过 `softprops/action-gh-release` 发布（匹配 `*.dmg/*.pkg/*.rpm/*.deb/*.exe`） | GitHub Release |
| `pages` | `ubuntu-24.04` 容器 | 安装 deb → `camus -iIw example` + `camus -dw example` → 上传并部署 GitHub Pages | `example/dist/html` |

工作流权限：`contents: write`、`pages: write`、`id-token: write`；`pages` concurrency。

## 9.7 版本变更

- 版本号来源于 `CMakeLists.txt` 的 `project(... VERSION ...)`；当前 **11.1.0**。
- 修改版本号后需同步：
  - `docs/PRD.md` 项目表；
  - `docs/prd/01-overview.md` 版本信息、`02-command-line.md` 示例、`08-template-install.md` 示例；
  - 其它文档中出现的版本引用。
- CMake 会把版本编入 `build/build.h`，并通过 `-v` 与模板变量 `{{ build.version }}` 对外体现。
