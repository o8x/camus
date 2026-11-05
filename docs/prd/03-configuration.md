# 3. 配置系统

配置实现：`camus/src/yaml_config.h`、`camus/src/yaml_config.cpp`。主配置文件名固定为 `camus.yaml`（`config.h.in` 的 `CAMUS_CONF_NAME`），模板缓存目录固定为 `.camus`（`CAMUS_DIR`）。

## 3.1 配置文件与位置

| 文件/目录 | 位置 | 作用 | 是否持久 |
|---|---|---|---|
| `camus.yaml` | 运行时当前工作目录 | 主配置 | 是 |
| `.env` | 运行时当前工作目录 | 模板变量（键统一转小写） | 是 |
| `theme/{name}/*.html` | 工作目录 | 本地主题覆盖 | 是 |
| `.camus/` | 工作目录 | 内嵌主题与默认资源缓存（`-i` 生成） | 是 |

- `build()` 先 `reload()` 再 `chdir(workdir)`（`writer.cpp:806-808`），因此**构建时**配置从 `-w` 指定的工作目录读取。
- `inspect()` 在 `chdir` 之前执行（`main.cpp:135-140`），因此 **`-I` 从进程原始当前目录读取 `camus.yaml`**，与 `-w` 无关。
- 每次 `reload()` 重新读取文件；热重载（watch）依赖此行为。

## 3.2 解析总则

- 根节点必须包含 `camus` 映射，否则 `error::panic("config file parse failed")`（退出 127）。
- `catalog` 必须是序列，否则 `panic("parse catalog config failed")`。
- 字符串值会做 `trim_space`；`catalog` 中 `title`/`subtitle`/`description` 的换行符替换为 `<br />`。
- 源目录、资源目录与输出目录不再可配置：
  - 源目录固定按 `src` → `posts` 顺序探测，存在即使用，否则回退 `src`（随后若不存在则报错）；
  - 本地资源目录固定为 `./assets`，不存在时回退内嵌 `.camus/assets`；
  - HTML 输出固定为 `./dist/html`，PDF 输出固定为 `./dist/pdf`。
- `render.html_engine` 固定为 `default`，`render.markdown_engine` 固定为 `cmark-gfm`，均不从配置读取。
- 路径在解析后转为绝对路径供内部使用；模板 JSON 中保留解析后的值（见 §3.8）。

## 3.3 `camus` 段字段

| 字段 | 类型 | 默认/必填 | 说明 | 代码位置 |
|---|---|---|---|---|
| `output.meta` | bool | 必填 | 是否输出 `meta.json` | `yaml_config.cpp:184` |
| `output.sitemap` | bool | 必填 | 是否输出 `sitemap.xml` | `yaml_config.cpp:183` |
| `output.filename_case` | 字符串 | 默认 `keep` | `uuid` 时文章 `display_name` 生成 UUID 并输出到 `/s/<uuid>.html` 风格短链 | `yaml_config.cpp:182` |
| `deploy.protocol` | 字符串 | 可选 | 自动补 `://`；有域名而无协议时默认 `https://` | `yaml_config.cpp:190-204` |
| `deploy.domain_name` | 字符串 | 可选 | 去除 `/` 与空格 | 同上 |
| `deploy.path_prefix` | 字符串 | 可选 | 归一化为以 `/` 开头的前缀 | `yaml_config.cpp:206-208` |
| `render.theme` | 字符串 | 默认 `default` | 主题目录名 | `yaml_config.cpp:129` |
| `render.markdown_options` | 字符串数组 | 可选 | cmark-gfm 选项名列表，见 §3.4 | `yaml_config.cpp:144-175` |
| `render.pdf_engine` | 字符串 | 可选 | PDF 引擎：`chromium`/`chrome`/`edge`，或可执行文件路径；非空时启用 PDF 生成 | `yaml_config.cpp:127-136` |
| `render.pdf_options` | 字符串数组 | 可选 | Chrome/Edge 启动参数；`--print-to-pdf=` 会替换为实际输出路径 | 同上 |

`deploy.full_prefix()` = `protocol + domain_name + path_prefix`。示例配置产物为 `https://stdout.com.cn/camus`。

### 3.3.1 PDF 引擎与参数

`render.pdf_engine` 非空时构建末尾执行 PDF 生成（见 §6.11）：

| 配置值 | macOS 解析 | Linux 解析 |
|---|---|---|
| `chromium` / `chrome` / `google-chrome` | `/Applications/Google Chrome.app/Contents/MacOS/Google Chrome`，其次 Chromium | `google-chrome`、`google-chrome-stable`、`chromium`、`chromium-browser`（按 PATH 查找） |
| `edge` / `msedge` / `microsoft-edge` | `/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge` | `microsoft-edge`、`microsoft-edge-stable`、`microsoft-edge-beta`（按 PATH 查找） |
| 含 `/` 的路径 | 直接作为可执行文件路径 | 同左 |

- 引擎找不到时输出 WARN 并跳过 PDF，不影响 HTML 构建。
- `pdf_options` 原样作为启动参数；其中以 `--print-to-pdf` 开头的项只取第一个并统一重写，支持 `{{dest_dir}}` 占位符（占位符代表 PDF 输出目录 `./dist/pdf`）：
  - `--print-to-pdf=`：默认输出 `<dest_dir>/index.pdf`；
  - `--print-to-pdf={{dest_dir}}/docs/book.pdf`：输出到 `<dest_dir>/docs/book.pdf`；
  - `--print-to-pdf=book.pdf`：相对路径按 `<dest_dir>/book.pdf` 解析。
- 未显式提供 `--headless` 时自动追加；始终追加临时 `--user-data-dir`（构建结束删除），避免污染用户浏览器配置。

### 3.4 markdown_options 映射

Markdown 引擎固定为 `cmark-gfm`，`render.markdown_options` 始终按本表解析；未知名会被收集用于展示但不产生标志位。`DEFAULT` 对应 `CMARK_OPT_DEFAULT`；Debug/RelWithDebInfo 构建默认追加 `SOURCEPOS`。

| 配置名 | cmark 标志 |
|---|---|
| `SOURCEPOS` | `CMARK_OPT_SOURCEPOS` |
| `HARDBREAKS` | `CMARK_OPT_HARDBREAKS` |
| `UNSAFE` | `CMARK_OPT_UNSAFE` |
| `NOBREAKS` | `CMARK_OPT_NOBREAKS` |
| `NORMALIZE` | `CMARK_OPT_NORMALIZE` |
| `VALIDATE_UTF8` | `CMARK_OPT_VALIDATE_UTF8` |
| `SMART` | `CMARK_OPT_SMART` |
| `GITHUB_PRE_LANG` | `CMARK_OPT_GITHUB_PRE_LANG` |
| `LIBERAL_HTML_TAG` | `CMARK_OPT_LIBERAL_HTML_TAG` |
| `FOOTNOTES` | `CMARK_OPT_FOOTNOTES` |
| `STRIKETHROUGH_DOUBLE_TILDE` | `CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE` |
| `TABLE_PREFER_STYLE_ATTRIBUTES` | `CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES` |
| `FULL_INFO_STRING` | `CMARK_OPT_FULL_INFO_STRING` |

模板 JSON 的 `camus.render.options` 会显示为 `"{数值}({名称1,名称2,...})"`（`yaml_config.cpp:225-226`）。

## 3.5 `catalog` 段（路由）

`catalog` 为序列，每项生成一个 `route_conf`（`route_conf` 序列化字段：`path`、`title`、`subtitle`、`description`、`transfers`）。

| 字段 | 必填 | 说明 |
|---|---|---|
| `path` | 是 | 源目录的相对路径；缺失时该条被忽略（debug 日志） |
| `title` | 否 | 目录显示名；为空继承根路由 `title` |
| `subtitle` | 否 | 目录副标题；为空继承根路由 |
| `description` | 否 | 目录描述；为空继承根路由 |
| `transfers` | 否 | 仅当 `path != "/"` 且为序列时生效 |
| `transfers[].source` | 是 | 源路径；`.` 或 `/` 表示路由自身路径，否则拼接到路由路径下 |
| `transfers[].target` | 是 | 目标输出路径（作为该目录的外部路径） |

- 根路由 `path: "/"` 决定 `camus.site`；根路由的 `transfers` 无效。
- 若不存在 `path: "/"`，`camus.site` 默认 `{path: "/", title: "A mysterious site"}`。
- 所有路由的空 `title/subtitle/description` 最终继承 `camus.site`。
- 路由匹配使用 `filesystem::path_equal(..., clean_check=true)`（`yaml_config.cpp:408`），即按清理后的路径比较。

示例：

```yaml
catalog:
  - path: /
    title: Musicals
  - path: /introduction
    transfers:
      - source: .
        target: /intro/Well-known
    title: Well-known musicals
```

## 3.6 `friends` 段

友链为序列，每项字段均以 `as<std::string>()` 读取（缺字段会抛异常）：

| 字段 | 说明 |
|---|---|
| `title` | 站点标题 |
| `desc` | 简短描述 |
| `link_url` | 跳转地址 |
| `image_url` | 头像/图片地址 |

结果注入渲染数据 `friends` 数组，用于 `friends.html`。

## 3.7 `.env` 解析规则

- 仅当当前目录存在 `.env` 时解析。
- 按行拆分，每行按 `=` 拆分为恰好两段才生效；空键或段数不为 2 的行被忽略（因此值中包含 `=` 的行会被忽略）。
- 键做 `trim_space` 后转小写；值做 `trim_space`。
- 结果同时写入 `camus_.envs`（用于 Markdown 变量替换）和模板 JSON 的 `env` 对象。
- 示例：

```dotenv
GITHUB_URL = https://github.com/o8x/camus
CDN_ACCELERATE = https://data-1252251443.cos.accelerate.myqcloud.com
```

## 3.8 模板可用配置数据（`conf_.json()`）

模板 JSON 由 `supplement_other_vars()` 组装（`yaml_config.cpp:20-90`）：

| 顶层键 | 结构 | 说明 |
|---|---|---|
| `camus` | 对象 | `source_dir`、`assets_dir`、`site`、`output`、`deploy`、`render`、`friends`；`render.options` 为选项展示串。`source_dir`/`assets_dir`/`output.dest_dir` 为解析后的绝对路径（`source_dir` 按 `src`→`posts` 探测，`output.dest_dir` 固定 `dist/html`） |
| `site` | 对象 | 根路由（`path/title/subtitle/description/transfers`） |
| `route` | 数组 | 全部路由（继承后） |
| `env` | 对象 | `.env` 键值（键小写） |
| `git` | 对象 | `repo`、`branch`、`commit_hash` |
| `build` | 对象 | 见下表 |

`build` 对象字段（来源 `yaml_config.cpp:59-90`）：

| 字段 | 说明 |
|---|---|
| `basedir` | 解析配置时的当前目录 |
| `timestamp` | 当前 Unix 时间（毫秒，字符串） |
| `time` | 本地时间 `%Y-%m-%d %H:%M:%S` |
| `build_type` | CMake 构建类型 |
| `cmake_version` | CMake 版本 |
| `cxx_standard` | C++ 标准号（23） |
| `version` | 项目版本（11.1.0） |
| `version_major` | 主版本号（10） |
| `compiler` | 编译器版本字符串 |
| `powered_by` | 预渲染的页脚 HTML 片段 |

示例（inja）：

```html
<a href="{{ env.github_url }}">{{ env.github_url }}</a>
<span>{{ build.version }} / {{ git.commit_hash }}</span>
```

## 3.9 持久性与重载

- 配置本身持久于磁盘；运行期不写回配置。
- `reload()` 每次全量重读 `camus.yaml` 与 `.env`。
- 当前实现中 `routes_`、`camus_.friends` 在 `reload()` 前**不会清空**，多次热重载会导致内部路由/友链列表累积；`public_config_` 为覆盖写入，模板 JSON 的 `route` 也随 `routes_` 累积。
- 路由/友链的新增在下次构建生效。

## 3.10 `inspect` 输出

`writer::inspect()`（`writer.cpp:742`）会：

1. 将 `cmd_.dryrun` 置为 true 并 `reload()`。
2. 以扁平化的 `key = value` 形式打印全部配置（`yaml_config::map()`，数组下标形如 `route.0.title`）。
3. 构建目录树（空则 WARN `No articles found`）。
4. 开启 DEBUG 日志并执行一次干跑构建。
5. 按树形打印 `[D]` 目录与每个节点的 `link_url`。

注意：`-I` 与 `-D` 组合时仍只执行一次 inspect 构建。
