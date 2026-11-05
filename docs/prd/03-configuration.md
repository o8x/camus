# 3. 配置系统

配置实现：`camus/src/yaml_config.h`、`camus/src/yaml_config.cpp`。主配置文件名固定为 `camus.yaml`（`config.h.in` 的 `CAMUS_CONF_NAME`），目录搬运规则写在 `camus.yaml` 的 `transfer` 段，目录元信息来自各目录下的 `README.md`（`CAMUS_README_NAME`），默认主题/资源从平台数据目录读取（见 §8）。

## 3.1 配置文件与位置

| 文件/目录 | 位置 | 作用 | 是否持久 |
|---|---|---|---|
| `camus.yaml` | 运行时当前工作目录 | 主配置 | 是 |
| `README.md` | 源目录及任意子目录 | 目录页元信息（`title`/`subtitle`/`description`），不参与文章渲染 | 是 |
| `theme/{name}/*.html` | 工作目录 | 本地主题覆盖 | 是 |
| `theme/pdf/{page.html,style.css,fonts.css,fonts/}` | 工作目录 | 本地 PDF 模板/样式/字体覆盖 | 是 |
| `.camus/` | 工作目录 | `-c` 复制的本地主题、PDF 模板、资源与示例模板 | 是 |
| `<数据目录>/` | macOS `~/Library/Application Support/Camus`；Linux `/var/lib/camus` | 默认主题、静态资源与示例模板（由安装包写入） | 是 |

- `build()` 先 `reload()` 再 `chdir(workdir)`（`writer.cpp:806-808`），因此**构建时**配置从 `-w` 指定的工作目录读取。
- `inspect()` 在 `chdir` 之前执行（`main.cpp:135-140`），因此 **`-I` 从进程原始当前目录读取 `camus.yaml`**，与 `-w` 无关。
- 每次 `reload()` 重新读取 `camus.yaml` 并扫描各目录的 `README.md`；热重载（watch）依赖此行为。

## 3.2 解析总则

- 根节点必须是映射，顶层直接包含 `meta`、`sitemap`、`filename_case`、`public_url`、`theme`、`pdf_engine`、`pdf_options`、`markdown_options`、`friends` 等配置项，否则 `error::panic("config file parse failed")`（退出 127）。不再接受 `render` 嵌套段；出现时打印 WARN 并忽略其内容。
- `README.md` 在目录扫描时跳过，不进入文章树。
- 字符串值会做 `trim_space`；`README.md` 中 `title`/`subtitle`/`description` 的换行符替换为 `<br />`。
- 源目录、资源目录与输出目录不再可配置：
  - 源目录固定按 `src` → `posts` 顺序探测，存在即使用，否则回退 `src`（随后若不存在则报错）；
  - 本地资源目录固定为 `./assets`，不存在时回退 `.camus/assets`，再回退数据目录的 `assets/`；
  - HTML 输出固定为 `./dist/html`，PDF 输出固定为 `./dist/pdf`。
- `html_engine` 固定为 `default`，`markdown_engine` 固定为 `cmark-gfm`，均不从配置读取。
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
| `cdn_url` | 字符串 | 空 | `with_cdn()` 使用的 CDN 基地址；为空时 `with_cdn(path)` 原样返回 `path`，末尾多余 `/` 会被去除 | `yaml_config.cpp` |
| `theme` | 字符串 | 默认 `default` | 主题目录名 | `yaml_config.cpp` |
| `markdown_options` | 字符串数组 | 可选 | cmark-gfm 选项名列表，见 §3.4 | `yaml_config.cpp` |
| `pdf_engine` | 字符串 | 可选 | PDF 引擎：`chromium`/`chrome`/`edge`，或可执行文件路径；非空且不为 `disable` 时启用 PDF 生成 | `yaml_config.cpp` |
| `pdf_options` | 字符串数组 | 可选 | Chrome/Edge 启动参数；`--print-to-pdf=` 会替换为实际输出路径 | `yaml_config.cpp` |

`public_url` 拆解规则（`parse_public_url()`）：

| 输入 | `protocol` | `domain_name` | `path_prefix` | `url`（归一化） |
|---|---|---|---|---|
| `https://stdout.com.cn/camus` | `https://` | `stdout.com.cn` | `/camus` | `https://stdout.com.cn/camus` |
| `stdout.com.cn` | `https://` | `stdout.com.cn` | 空 | `https://stdout.com.cn` |
| `https://stdout.com.cn/` | `https://` | `stdout.com.cn` | 空 | `https://stdout.com.cn` |
| `http://localhost:8080/blog/` | `http://` | `localhost:8080` | `/blog` | `http://localhost:8080/blog` |

`public_url_conf::full_prefix()` = `protocol + domain_name + path_prefix`，用于绝对链接与 sitemap。

### 3.3.1 PDF 引擎与参数

`pdf_engine` 非空且不为 `disable` 时构建末尾执行 PDF 生成（见 §6.11）；`disable` 不区分大小写并忽略首尾空格：

| 配置值 | macOS 解析 | Linux 解析 |
|---|---|---|
| `chromium` / `chrome` / `google-chrome` | `/Applications/Google Chrome.app/Contents/MacOS/Google Chrome`，其次 Chromium | `google-chrome`、`google-chrome-stable`、`chromium`、`chromium-browser`（按 PATH 查找） |
| `edge` / `msedge` / `microsoft-edge` | `/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge` | `microsoft-edge`、`microsoft-edge-stable`、`microsoft-edge-beta`（按 PATH 查找） |
| 含 `/` 的路径 | 直接作为可执行文件路径 | 同左 |

- `pdf_engine: disable` 表示显式禁用 PDF：不解析引擎、不产生 `dist/pdf` 内容，也不输出告警。
- 引擎找不到时输出 WARN 并跳过 PDF，不影响 HTML 构建。
- `pdf_options` 原样作为启动参数；其中以 `--print-to-pdf` 开头的项只取第一个并统一重写，支持 `{{dest_dir}}` 占位符（占位符代表 PDF 输出目录 `./dist/pdf`）：
  - `--print-to-pdf=`：默认输出 `<dest_dir>/index.pdf`；
  - `--print-to-pdf={{dest_dir}}/docs/book.pdf`：输出到 `<dest_dir>/docs/book.pdf`；
  - `--print-to-pdf=book.pdf`：相对路径按 `<dest_dir>/book.pdf` 解析。
- 未显式提供 `--headless` 时自动追加；始终追加临时 `--user-data-dir`（构建结束删除），避免污染用户浏览器配置。
- PDF 中间 HTML 使用 `theme/pdf/page.html` 模板、`theme/pdf/style.css` 样式与 `theme/pdf/fonts.css` 字体样式；自定义字体放在 `theme/pdf/fonts/`。该目录不通过 `camus.yaml` 配置，查找顺序与可用模板变量见 §5.10、§8.3。

### 3.4 markdown_options 映射

Markdown 引擎固定为 `cmark-gfm`，顶层 `markdown_options` 始终按本表解析；未知名会被收集用于展示但不产生标志位。`DEFAULT` 对应 `CMARK_OPT_DEFAULT`；Debug/RelWithDebInfo 构建默认追加 `SOURCEPOS`。

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

顶层配置 `markdown_options` 在模板 JSON 的 `camus.render.options` 会显示为 `"{数值}({名称1,名称2,...})"`（`yaml_config.cpp:225-226`）。

## 3.5 `transfer`（目录搬运）与 `README.md`（目录元信息）

### 3.5.1 `README.md` 元信息

构建时会递归扫描源目录（含根目录）中的 `README.md`，为对应目录生成 `route_conf`（序列化字段：`path`、`title`、`subtitle`、`description`、`transfers`）。`README.md` 本身不会作为文章渲染。

| 来源 | 说明 |
|---|---|
| Front Matter `title` | 目录显示名 / 页面标题 |
| Front Matter `subtitle` | 目录副标题 |
| Front Matter `description` | 目录描述 |
| 正文第一个 `# 标题` | `title` 缺失时的回退值 |
| 正文剩余内容 | `description` 缺失时的回退值 |

- Front Matter 使用 YAML 映射解析（`---` 包裹），非法 YAML 会打印 WARN 并忽略该段。
- 字段会 `trim_space`，内部换行替换为 `<br />`。
- 未设置的字段继承根路由（`source_dir/README.md`）的对应值。
- 根路由 `path` 固定为 `/`；根 `README.md` 缺失时 `site` 默认为 `{path: "/", title: "A mysterious site"}`。
- 路由列表按 `path` 排序，根路由始终排在最前，保证 `route` 与 `meta.json` 输出稳定。
- `path` 中包含 `private` 段的目录会被忽略并打印 WARN。

示例（`posts/introduction/README.md`）：

```markdown
---
title: Well-known musicals
subtitle: Optional summary
---

1. Man of la Mancha
2. Mozart!
```

### 3.5.2 `build` 段

`camus.yaml` 顶层 `build` 为序列，每项包含 `path` 与 `transfers`；旧的顶层键 `transfer` 仍兼容（同时存在时 `build` 优先）：

| 字段 | 必填 | 说明 |
|---|---|---|
| `path` | 是 | 源目录路由路径；缺失时该项被忽略并打印 WARN |
| `transfers` | 否 | 必须为序列，否则跳过 |
| `transfers[].source` | 是 | 源路径；`.` 或 `/` 表示路由自身路径，否则拼接到路由路径下 |
| `transfers[].target` | 是 | 目标输出路径（作为该目录的外部路径） |

- 根路径 `/` 的 `transfers` 无效，出现时打印 WARN 并忽略。
- 每条 transfer 的 `source`/`target` 缺失或 `target` 为空时会被忽略并打印 WARN。
- transfer 的 `source` 会预先拼成完整源路径（`source: .` → 路由自身路径，否则 `路由路径/source`），因此 `source: 子目录` 可搬运路由下的子目录；搬运会作用于该目录的所有后代。
- `transfers` 会同时挂到同 `path` 的 `README.md` 路由上，供模板 `route` 数据使用；没有 `README.md` 的路径仍会参与搬运匹配，但不会出现在 `route` 列表中。
- 搬运匹配使用 `filesystem::path_equal(..., clean_check=true)`。

示例：

```yaml
build:
  - path: /introduction
    transfers:
      - source: .
        target: /intro/Well-known
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
| `camus` | 对象 | 兼容保留的聚合对象：`source_dir`、`assets_dir`、`site`、`meta`、`sitemap`、`filename_case`、`public_url`、`protocol`、`domain_name`、`path_prefix`、`cdn_url`、`render`、`friends`；`render.options` 为选项展示串 |
| `source_dir` | 字符串 | 解析后的源目录绝对路径（`src`→`posts` 探测） |
| `assets_dir` | 字符串 | 解析后的资源目录绝对路径 |
| `meta` | bool | 是否输出 `meta.json` |
| `sitemap` | bool | 是否输出 `sitemap.xml` |
| `filename_case` | 字符串 | 输出文件名策略（`keep` / `uuid`） |
| `public_url` | 字符串 | 归一化后的站点对外地址 |
| `protocol` | 字符串 | `public_url` 的协议部分（`https://`） |
| `domain_name` | 字符串 | `public_url` 的域名部分 |
| `path_prefix` | 字符串 | `public_url` 的路径前缀 |
| `cdn_url` | 字符串 | CDN 基地址（默认空） |
| `render` | 对象 | 模板兼容聚合对象（`theme_name`、`html_engine`、`markdown_engine`、`pdf_engine`、`pdf_options`）；对应配置项在 `camus.yaml` 顶层，此对象仅用于模板数据 |
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
| `version` | 项目版本（`PROJECT_VERSION`，以 `docs/PRD.md` 项目表 / `CMakeLists.txt` 为准） |
| `version_major` | 主版本号（`PROJECT_VERSION_MAJOR`） |
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
- `reload()` 每次全量重读 `camus.yaml` 并重新扫描 `README.md`，先重置上一轮的 `camus_`、`routes_`、`transfers_`、`public_config_`，因此反复构建/热重载不会累积路由或友链。
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
