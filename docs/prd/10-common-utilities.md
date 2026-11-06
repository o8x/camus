# 10. 公共模块与工具

位置：`common/common/`。这些为内部公共 API（C++），非对外网络接口；无持久化状态（除非明确写文件）。

## 10.1 字符串 `strings`（`common/common/str/str.h`）

| 签名 | 行为 |
|---|---|
| `std::string rand_string(uint16_t len = 4)` | 返回 `len` 个随机数字字符（函数名为历史遗留） |
| `std::string uuid_v4()` | 生成标准 36 字符 UUID v4 |
| `std::string trim_space(const std::string&)` | 去除两端空白 `" \n\r\t\f\v"` |
| `std::string trim_left/trim_right/trim(s, remove)` | 循环移除两端出现的指定子串 |
| `std::vector<std::string> split(string_view s, char delim)` | 按字符拆分（`getline` 语义） |
| `std::vector<std::string> split(string_view s, string_view delim, size_t n = 0)` | 按子串拆分，`n>0` 时最多 `n` 段 |
| `std::string replace(str, from, to)` | 全部替换（自左向右，跳过已替换部分） |
| `std::string replace(str, map, bool deep=false)` | 依次应用映射；`deep=true` 时再应用一轮 |
| `std::string replace_nocase(str, from, to)` | 把 `from` 当正则、忽略大小写替换 |
| `std::string string_join(vec, delimiter)` | 拼接 |
| `std::string url_encode(value)` | 仅保留字母数字与 `-_.~`，其余转 `%XX` 大写 |
| `std::string to_lower(string_view)` | ASCII 小写 |
| `coloring_*` / `coloring(text, color)` | ANSI 颜色包装 |
| `size_t get_display_width(str)` | 估算终端显示宽度（ASCII/2字节=1，3/4字节=2） |
| `std::string padding_left(str, width)` | 右侧补空格到指定显示宽度 |
| `uint16_t estimate_effective_length(str)` | `CJK + (字母+数字)/5` 向上取整 |
| `uint16_t estimate_reading_minutes(text)` | `effective_length / 300.0` 截断；常量 `READING_SPEED=300.0` |
| `std::string mark_string(text, target_line, column)` | 打印目标行附近 3 行，调试用 |
| `constexpr uint64_t make_hash(str)` | FNV-1a 64 位 + 混淆收尾 |
| `template T convert_type<T>(std::string)` | 转整数/浮点/字符串，不支持类型 `panic` |
| `split_pair<T>(s, delim, trim=false)` | 按分隔符拆成两段；段数不为 2 时返回空 `{}` |

## 10.2 文件系统 `filesystem`（`common/common/filesystem/filesystem.h`）

| 签名 | 行为 |
|---|---|
| `int write_file(path, content)` | 覆盖写入；成功返回内容长度，打开失败返回 `-1` |
| `std::string read_file(name, bool trim=false)` | 读全文件；打开失败返回 `"Could not open file: <绝对路径>"`（调用方需注意） |
| `std::string clean_path(path, prefix="")` | `lexically_normal(prefix + "/" + path)`；`//` 归一为 `/` |
| `path clean_path_prefix(path, prefix, absolute=false)` | 取相对前缀的路径 |
| `bool path_empty(path)` | **不存在返回 false**；存在且为空目录返回 true |
| `int scan_path_files(path, max_scan=200)` | 递归计数，超过 `max_scan` 截止 |
| `path get_self_path(arg0)` | Linux 读 `/proc/self/exe`，macOS 用 `_NSGetExecutablePath`+`realpath`，否则回退 `arg0` |
| `std::string path_abs(path)` | `absolute()` 后把 `//` 替换为 `/` |
| `void empty_path(path, safe_check=false, auto_create=false)` | 递归删除；`safe_check` 时文件数 >200 触发 fatal；`auto_create` 删除后重建 |
| `bool path_equal(p1, p2, clean_check=false)` | 先字符串比较，再 `equivalent`，失败则 `weakly_canonical`；`clean_check` 时再比较 `clean_path` |

## 10.3 时间与随机 `functions`（`common/common/functions/functions.h`）

| 签名 | 行为 |
|---|---|
| `std::string get_now_time(format="%Y-%m-%d %H:%M:%S")` | 本地时间格式化 |
| `uint64_t rand_int(min, max)` | 均匀随机整数（含端点） |
| `time_t datetime_to_unix(datetime)` | 无空格按 `%Y-%m-%d`，有空格按 `%Y-%m-%d %H:%M:%S`，**UTC**（`timegm`） |
| `std::string format_time_t(ts, format="%Y-%m-%d %H:%M:%S")` | **UTC**（`gmtime`）格式化 |

## 10.4 日志与错误

`logging`（`common/common/logging/logging.h`）：

| 级别 | 值 | 标签 |
|---|---|---|
| `DEBUG_LEVEL` | 1 | `[DEBU]`（无颜色） |
| `INFO_LEVEL` | 2 | `[INFO]`（绿） |
| `WARN_LEVEL` | 3 | `[WARN]`（黄） |
| `ERROR_LEVEL` | 4 | `[ERRO]`（红） |
| `FATAL_LEVEL` | 5 | `[FATA]`（品红） |

- 默认级别 `INFO_LEVEL`；`set_level(level)` 返回新级别。
- 输出格式：`<本地时间> [LEVL] <内容>`，内容两端空白被裁剪，`$HOME` 替换为 `~`。
- 仅当 stdout 是终端、未设置 `NO_COLOR`、且存在 `TERM` 时使用颜色。
- `logging::fatal(...)` 打印后 `std::exit(127)`。
- `error::panic(...)` 等价于 `logging::fatal(...)`（`common/common/error/error.h`）。

## 10.5 网络 `net`（`common/common/net/net.h`）

`std::string get_local_addr(const std::string& dns_server = "114.114.114.114")`：

- 创建 UDP socket，向 DNS 服务器地址 `connect`（不发送数据），通过 `getsockname` 取本机 IPv4。
- 成功时原样返回取得的 IPv4 地址，不做 fake-ip 网段改写。
- 任一步失败返回空串。用于监听模式日志中展示可访问地址（服务本身绑定 `0.0.0.0`）。

## 10.6 平台路径 `paths`（`common/common/paths/paths.h`）

| 签名 | 行为 |
|---|---|
| `path data_dir()` | macOS 返回 `$HOME/Library/Application Support/Camus`（`$HOME` 缺失时回退 `/Library/Application Support/Camus`）；Linux 返回 `/var/lib/camus` |
| `std::vector<path> data_dirs()` | 按优先级返回候选数据目录；macOS 追加系统级 `/Library/Application Support/Camus` |
| `path local_data_dir()` | 返回工作目录 `.camus`（`CAMUS_LOCAL_DATA_DIR`） |
| `path find(relative_path)` | 在候选数据目录下查找相对路径，返回第一个存在的路径；找不到返回空路径 |
| `path find_resource(relative_path)` | 先查 `.camus/relative`，再调用 `find`；找不到返回空路径 |
| `path find_local_or_resource(relative_path)` | 先查工作目录下的相对路径（显式本地覆盖），再调用 `find_resource`；找不到返回空路径 |
| `path source_data_dir()` | 返回第一个同时包含 `theme/` 与 `assets/` 的数据目录，供 `-c` 使用 |
| `path default_assets_dir()` | 优先 `.camus/assets`，其次 `resource_assets_dir()`；找不到返回空路径 |
| `path resource_assets_dir()` | 返回第一个存在且非空的数据目录 `assets/`（不含 `.camus` 覆盖）；找不到返回空路径 |

- 常量来源：`config.h.in` 的 `CAMUS_LOCAL_DATA_DIR`、`CAMUS_DATA_DIR_MACOS` 与 `CAMUS_DATA_DIR_LINUX`。
- 供 `-i` 安装示例文件、`-c` 复制最小/完整资源（§8.2）、主题查找（§5.1、§8.3）与静态资源输出（§6.9、§8.3）使用；构建过程不写入数据目录。

## 10.7 命令行 `cmdline`（`common/common/cmdline/cmdline.h`）

- `cmdline::parser`：`add(name, short_name, desc, need, default_value)`、`parse(argc, argv)`、`exist(name)`、`get<T>(name)`、`rest()`、`usage()`、`error_full()`。
- 支持组合短选项、`--name=value`、`--name value`；未知选项进入错误列表。
- 主程序只用到了布尔开关与 `workdir` 字符串选项（§2）。

## 10.8 编译期常量（`config.h.in`）

| 宏 | 值/来源 | 含义 |
|---|---|---|
| `PROJECT_VERSION` | `CMakeLists.txt` 的 `project(... VERSION ...)` | 版本 |
| `PROJECT_VERSION_MAJOR/MINOR/PATCH` | 由项目版本自动拆分 | 版本分段 |
| `CMAKE_VERSION` | CMake 版本 | 构建工具版本 |
| `BUILD_TIMESTAMP` | 配置时间 | 构建时间 |
| `BUILD_TYPE` | 构建类型 | Debug/Release 等 |
| `CXX_STANDARD` | `23` | C++ 标准 |
| `GIT_FOUND`/`GIT_BRANCH`/`GIT_COMMIT_HASH` | Git 信息 | 版本输出与模板变量 |
| `GIT_REPO` | `https://github.com/o8x/camus` | 仓库地址 |
| `CAMUS_LOCAL_DATA_DIR` | `.camus` | 工作目录本地资源目录 |
| `CAMUS_DATA_DIR_MACOS` | `Library/Application Support/Camus` | macOS 用户数据目录（相对 `$HOME`） |
| `CAMUS_DATA_DIR_LINUX` | `/var/lib/camus` | Linux 数据目录 |
| `CAMUS_CONF_NAME` | `camus.yaml` | 主配置文件名 |
| `CAMUS_SUMMARY_NAME` | `SUMMARY.md` | 目录元信息文件名（源目录及子目录，扫描时跳过文章树） |
| `CAMUS_OPEN_TARGET_PROJECT` | `project` | `--open` 默认目标：VS Code 打开工作目录 |
| `CAMUS_OPEN_TARGET_RUNTIME` | `runtime` | `--open` 目标：文件管理器打开数据目录 |
| `CAMUS_PRIVATE_DIR` | `private` | 不参与扫描的私有目录名 |
| `CAMUS_PDF_THEME_DIR` | `theme/pdf` | PDF 模板与样式目录 |
| `CAMUS_PDF_TEMPLATE_NAME` | `page.html` | PDF 中间 HTML 模板文件名 |
| `CAMUS_PDF_STYLE_NAME` | `style.css` | PDF 打印样式文件名 |
| `CAMUS_PDF_FONTS_NAME` | `fonts.css` | PDF 字体样式文件名 |
| `CAMUS_PDF_FONTS_DIR` | `fonts` | PDF 自定义字体目录名 |
