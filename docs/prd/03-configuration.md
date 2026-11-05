# 3. 配置系统

配置实现：`camus/src/yaml_config.h`、`camus/src/yaml_config.cpp`。主配置文件名固定为 `camus.yaml`（`config.h.in` 的 `CAMUS_CONF_NAME`），目录路由与搬运规则放在同目录下的 `catalog.yaml`（`CAMUS_CATALOG_NAME`），默认主题/资源从平台数据目录读取（见 §8）。

## 3.1 配置文件与位置

| 文件/目录 | 位置 | 作用 | 是否持久 |
|---|---|---|---|
| `camus.yaml` | 运行时当前工作目录 | 主配置 | 是 |
| `catalog.yaml` | 与 `camus.yaml` 同目录 | 目录路由与文件搬运规则 | 是 |
| `theme/{name}/*.html` | 工作目录 | 本地主题覆盖 | 是 |
| `theme/pdf/{page.html,style.css}` | 工作目录 | 本地 PDF 模板/样式覆盖 | 是 |
| `.camus/` | 工作目录 | `-i` 安装的本地主题、PDF 模板、资源与示例模板 | 是 |
| `<数据目录>/` | macOS `~/Library/Application Support/Camus`；Linux `/var/lib/camus` | 默认主题、静态资源与示例模板（由安装包写入） | 是 |

- `build()` 先 `reload()` 再 `chdir(workdir)`（`writer.cpp:806-808`），因此**构建时**配置从 `-w` 指定的工作目录读取。
- `inspect()` 在 `chdir` 之前执行（`main.cpp:135-140`），因此 **`-I` 从进程原始当前目录读取 `camus.yaml`**，与 `-w` 无关。
- 每次 `reload()` 重新读取 `camus.yaml` 与 `catalog.yaml`；热重载（watch）依赖此行为。

## 3.2 解析总则

- 根节点必须是映射，顶层直接包含 `meta`、`sitemap`、`filename_case`、`public_url`、`render`、`friends` 等配置项，否则 `error::panic("config file parse failed")`（退出 127）。
- `catalog.yaml` 必须存在且根节点为序列，否则 `panic("catalog file not found ...")` / `panic("parse catalog config failed")`。
- 字符串值会做 `trim_space`；`catalog.yaml` 中 `title`/`subtitle`/`description` 的换行符替换为 `<br />`。
- 源目录、资源目录与输出目录不再可配置：
  - 源目录固定按 `src` → `posts` 顺序探测，存在即使用，否则回退 `src`（随后若不存在则报错）；
  - 本地资源目录固定为 `./assets`，不存在时回退 `.camus/assets`，再回退数据目录的 `assets/`；
  - HTML 输出固定为 `./dist/html`，PDF 输出固定为 `./dist/pdf`。
- `render.html_engine` 固定为 `default`，`render.markdown_engine` 固定为 `cmark-gfm`，均不从配置读取。
- 路径在解析后转为绝对路径供内部使用；模板 JSON 中保留解析后的值（见 §3.7）。

## 3.3 顶层配置字段

| 字段 | 类型 | 默认/必填 | 说明 | 代码位置 |
|---|---|---|---|---|
| `meta` | bool | 默认 `true` | 是否输出 `meta.json` | `yaml_config.cpp` |
| `sitemap` | bool | 默认 `true` | 是否输出 `sitemap.xml` | `yaml_config.cpp` |
| `filename_case` | 字符串 | 默认 `keep` | `uuid` 时文章 `display_name` 生成 UUID 并输出到 `/s/<uuid>.html` 风格短链 | `yaml_config.cpp` |
| `public_url` | 字符串 | 可选 | 站点对外访问地址，自动拆解为下方三项 | `yaml_config.cpp` `parse_public_url()` |
| `protocol` | 字符串（只读派生） | 默认 `https://` | `public_url` 的协议部分，含 `://` | 同上 |
| `domain_name` | 字符串（只读派生） | 空 | `public_url` 的域名部分（含端口，去除空格） | 同上 |
| `path_prefix` | 字符串（只读派生） | 空 | `public_url` 的路径部分，以 `/` 开头且不带尾斜杠 | 同上 |
| `render.theme` | 字符串 | 默认 `default` | 主题目录名 | `yaml_config.cpp:129` |
| `render.markdown_options` | 字符串数组 | 可选 | cmark-gfm 选项名列表，见 §3.4 | `yaml_config.cpp:144-175` |
| `render.pdf_engine` | 字符串 | 可选 | PDF 引擎：`chromium`/`chrome`/`edge`，或可执行文件路径；非空时启用 PDF 生成 | `yaml_config.cpp:127-136` |
| `render.pdf_options` | 字符串数组 | 可选 | Chrome/Edge 启动参数；`--print-to-pdf=` 会替换为实际输出路径 | 同上 |

`public_url` 拆解规则（`parse_public_url()`）：

| 输入 | `protocol` | `domain_name` | `path_prefix` | `url`（归一化） |
|---|---|---|---|---|
| `https://stdout.com.cn/camus` | `https://` | `stdout.com.cn` | `/camus` | `https://stdout.com.cn/camus` |
| `stdout.com.cn` | `https://` | `stdout.com.cn` | 空 | `https://stdout.com.cn` |
| `https://stdout.com.cn/` | `https://` | `stdout.com.cn` | 空 | `https://stdout.com.cn` |
| `http://localhost:8080/blog/` | `http://` | `localhost:8080` | `/blog` | `http://localhost:8080/blog` |

`public_url_conf::full_prefix()` = `protocol + domain_name + path_prefix`，用于绝对链接与 sitemap。

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
- PDF 中间 HTML 使用 `theme/pdf/page.html` 模板与 `theme/pdf/style.css` 样式；该目录不通过 `camus.yaml` 配置，查找顺序与可用模板变量见 §5.10、§8.3。

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

顶层配置 `render.markdown_options` 在模板 JSON 的 `camus.render.options` 会显示为 `"{数值}({名称1,名称2,...})"`（`yaml_config.cpp:225-226`）。

## 3.5 `catalog.yaml`（路由与搬运）

`catalog.yaml` 与 `camus.yaml` 同目录，根节点为序列，每项生成一个 `route_conf`（`route_conf` 序列化字段：`path`、`title`、`subtitle`、`description`、`transfers`）。`catalog.yaml` 缺失或非序列会导致解析失败。

| 字段 | 必填 | 说明 |
|---|---|---|
| `path` | 是 | 源目录的相对路径；缺失时该条被忽略（debug 日志） |
| `title` | 否 | 目录显示名；为空继承根路由 `title` || `subtitle` | 否 | 目录副标题；为空继承根路由 |
| `description` | 否 | 目录描述；为空继承根路由 |
| `transfers` | 否 | 仅当 `path != "/"` 且为序列时生效 |
| `transfers[].source` | 是 | 源路径；`.` 或 `/` 表示路由自身路径，否则拼接到路由路径下 |
| `transfers[].target` | 是 | 目标输出路径（作为该目录的外部路径） |

- 根路由 `path: "/"` 决定 `site`；根路由的 `transfers` 无效。
- 每条 transfer 的 `source`/`target` 缺失或 `target` 为空时会被忽略并打印 WARN。
- transfer 的 `source` 会预先拼成完整源路径（`source: .` → 路由自身路径，否则 `路由路径/source`），因此 `source: 子目录` 可搬运路由下的子目录；搬运会作用于该目录的所有后代。
- 同一 `path` 出现多次时只有第一条生效，后续打印 WARN 并忽略。
- `path` 中若包含名为 `private` 的路径段（常量 `CAMUS_PRIVATE_DIR`），该条路由在解析阶段被忽略并打印 WARN，不能作为目录页使用。
- 若不存在 `path: "/"`，`site` 默认 `{path: "/", title: "A mysterious site"}`。
- 所有路由的空 `title/subtitle/description` 最终继承 `site`。
- 路由匹配使用 `filesystem::path_equal(..., clean_check=true)`（`yaml_config.cpp:408`），即按清理后的路径比较。

示例（`catalog.yaml` 根节点直接是序列）：

```yaml
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

## 3.7 模板可用配置数据（`conf_.json()`）

模板 JSON 由 `supplement_other_vars()` 组装（`yaml_config.cpp:20-90`）：

| 顶层键 | 结构 | 说明 |
|---|---|---|
| `camus` | 对象 | 兼容保留的聚合对象：`source_dir`、`assets_dir`、`site`、`meta`、`sitemap`、`filename_case`、`public_url`、`protocol`、`domain_name`、`path_prefix`、`render`、`friends`；`render.options` 为选项展示串 |
| `source_dir` | 字符串 | 解析后的源目录绝对路径（`src`→`posts` 探测） |
| `assets_dir` | 字符串 | 解析后的资源目录绝对路径 |
| `meta` | bool | 是否输出 `meta.json` |
| `sitemap` | bool | 是否输出 `sitemap.xml` |
| `filename_case` | 字符串 | 输出文件名策略（`keep` / `uuid`） |
| `public_url` | 字符串 | 归一化后的站点对外地址 |
| `protocol` | 字符串 | `public_url` 的协议部分（`https://`） |
| `domain_name` | 字符串 | `public_url` 的域名部分 |
| `path_prefix` | 字符串 | `public_url` 的路径前缀 |
| `render` | 对象 | 渲染配置（`theme_name`、`html_engine`、`markdown_engine`、`pdf_engine`、`pdf_options`） |
| `friends` | 数组 | 友情链接 |
| `site` | 对象 | 根路由（`path/title/subtitle/description/transfers`） |
| `route` | 数组 | 全部路由（继承后） |
| `git` | 对象 | `repo`、`branch`、`commit_hash` |
| `build` | 对象 | 见下表 |

`camus` 聚合对象在所有配置解析完成后生成，包含最终 `site`（根路由继承后）、`friends` 以及绝对路径形式的 `source_dir` / `assets_dir`。

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
| `version_major` | 主版本号（11） |
| `compiler` | 编译器版本字符串 |
| `powered_by` | 预渲染的页脚 HTML 片段 |

示例（inja）：

```html
<a href="{{ public_url }}">Site</a>
<a href="{{ git.repo }}">{{ git.repo }}</a>
<span>{{ build.version }} / {{ git.commit_hash }}</span>
```

PDF 模板在上述配置 JSON 之外额外接收 `pdf` 对象（封面、目录、文章列表、样式原文等），字段见 §5.10。

## 3.8 持久性与重载

- 配置本身持久于磁盘；运行期不写回配置。
- `reload()` 每次全量重读 `camus.yaml` 与 `catalog.yaml`，并先重置上一轮的 `camus_`、`routes_`、`public_config_`，因此反复构建/热重载不会累积路由或友链。
- 路由/友链的新增在下次构建生效。
- `writer::inspect()` 与 `writer::build()` 会先把进程工作目录切到 `--workdir`，因此 `-w <dir> -I` 读取的是该目录下的配置。

## 3.9 `inspect` 输出

`writer::inspect()`（`writer.cpp:742`）会：

1. 将 `cmd_.dryrun` 置为 true 并 `reload()`。
2. 以扁平化的 `key = value` 形式打印全部配置（`yaml_config::map()`，数组下标形如 `route.0.title`）。
3. 构建目录树（空则 WARN `No articles found`）。
4. 开启 DEBUG 日志并执行一次干跑构建。
5. 按树形打印 `[D]` 目录与每个节点的 `link_url`。

注意：`-I` 与 `-D` 组合时仍只执行一次 inspect 构建。
