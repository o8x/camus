# 5. 模板与渲染

实现：`common/common/render/render.cpp`、`camus/src/writer.cpp`、`camus/src/yaml_config.cpp`。

## 5.1 主题解析

主题由四个模板组成，类型枚举 `camus_theme_type`（`yaml_config.h:51`）：

| 类型 | 文件 | 用途 |
|---|---|---|
| `CAMUS_THEME_TYPE_HOME` | `home.html` | 目录索引页 |
| `CAMUS_THEME_TYPE_PAGE` | `page.html` | 文章页 |
| `CAMUS_THEME_TYPE_STATS` | `stats.html` | 统计页 |
| `CAMUS_THEME_TYPE_FRIENDS` | `friends.html` | 友链页 |

加载顺序（`yaml_config.cpp:244-264`）：

1. 若本地存在 `theme/{theme_name}/{type}.html`，优先使用本地覆盖。
2. 否则查找 `.camus/theme/{theme_name}/{type}.html`（`-c --full` 复制）。
3. 仍未找到时在平台数据目录中查找同名文件（macOS `~/Library/Application Support/Camus`，Linux `/var/lib/camus`）。
4. 文件不存在时 `logging::fatal("read theme content failed ...")`，进程退出 127。

`theme_name` 默认 `default`；示例站点使用 `terminal`。HTML 引擎固定为 `default`（inja），不再从配置读取，`theme_name` 也不会再拼接引擎子路径。

所有内置主题（`default`/`document`/`pixel`/`terminal`）都额外引用公共资源入口：

- 样式：`{{ with_prefix("/css/style.css") }}`，对应本地 `assets/css/style.css`
- 脚本：`{{ with_prefix("/js/onload.js") }}`，对应本地 `assets/js/onload.js`

两个文件在默认资源中为空文件；用户可在本地 `./assets` 下提供同名文件覆盖，不存在时构建不会报错。

`pixel` 主题在 `assets/css/pixel-inja.css` 中设置 `html { font-size: 85%; }`，字体与基于 rem 的元素尺寸整体约缩小 15%。

## 5.2 Markdown 渲染

`render_markdown(html, engine, opt)`（`render.cpp`）：

- 仅支持 `engine == "cmark-gfm"`；其它引擎 `logging::fatal("unsupported engine type ...")`。
- 调用 `cmark_markdown_to_html`，失败返回空串。
- 选项位由顶层 `markdown_options` 映射（见 §3.4），Debug/RelWithDebInfo 构建默认带 `CMARK_OPT_SOURCEPOS`。

## 5.3 文章渲染流程

`render_single_article()`（`writer.cpp:317`）：

```mermaid
flowchart TD
    A[contents 以换行连接为 markdown] --> C[计算有效字数与阅读时长, 累加统计]
    C --> D[cmark-gfm 渲染 Markdown -> html]
    D --> E[html 作为 inja 模板, 以配置 JSON 渲染]
    E --> F[组装文章数据 j]
    F --> G[inja 渲染 page.html]
    G --> H[HTML 后处理替换]
    H --> I[写入 link_url 与 real_url]
```

- **Markdown 内联模板**：Markdown 渲染后的 HTML 会再用配置 JSON 做一次 inja 渲染，因此正文中可使用 `{{ build.version }}` 等配置变量（注意与 Markdown 语法/花括号冲突时的转义需求）。
- `display_name` 也会先经 inja 渲染，再清洗 `/`、`\`、`'`、`"` 为空格（§4.5）。

## 5.4 inja 自定义函数

在 `writer` 构造时注册（`writer.cpp:661-689`），可在任意主题模板/正文中使用：

| 函数 | 签名 | 行为 | 示例 |
|---|---|---|---|
| `format_datetime` | `format_datetime(unix_ts)` → string | UTC `%Y-%m-%d %H:%M:%S` | `{{ format_datetime(page.write_time) }}` |
| `format_date` | `format_date(unix_ts)` → string | UTC `%Y-%m-%d` | `{{ format_date(page.write_time) }}` |
| `with_prefix` | `with_prefix(path)` → string | `clean_path(path, public_url.path_prefix)` | `{{ with_prefix("index.html") }}` |
| `with_cdn` | `with_cdn(path)` → string | 返回 `cdn_url + with_prefix(path)`；`cdn_url` 为空时返回 `with_prefix(path)`（即包含 `public_url.path_prefix`） | `{{ with_cdn("css/a.css") }}` |

`with_cdn` 仅依赖 `cdn_url` 与 `public_url.path_prefix` 配置，不再读取任何环境变量。`-W` 监听模式下二者均被忽略，`with_cdn(path)` 等价 `with_prefix(path)`（前缀为空时原样返回）。

## 5.5 文章页渲染数据

`render_single_article` 组装的 JSON `j`（`writer.cpp:359-369`）由配置数据叠加节点数据得到：

| 键 | 结构 | 说明 |
|---|---|---|
| `camus` / `source_dir` / `assets_dir` / `meta` / `sitemap` / `filename_case` / `public_url` / `protocol` / `domain_name` / `path_prefix` / `render` / `friends` / `site` / `route` / `git` / `build` | 见 §3.7 | 站点配置 |
| `title` | 字符串 | 文章显示名（来自节点 `to_json`） |
| `write_time` | 数字 | 文章时间戳 |
| `path` | 字符串 | 节点 `real_url` 加上 `public_url.path_prefix` 的完整链接（主题直接作 `href`） |
| `is_dir` | bool | 文章为 false |
| `link_path` | 字符串 | 短链路径，无短链时为空 |
| `tags` | 数组 | 文章标签 |
| `toc` | 数组 | 顶级目录/文章项（见 §5.6） |
| `page.title` | 字符串 | 同 `display_name` |
| `page.write_time` | 数字 | 同上 |
| `page.description` | 字符串 | 固定为空串 |
| `page.contents` | 字符串 | 已渲染的正文 HTML |
| `stats.word_count` | 数字 | 有效字数 |
| `stats.read_min` | 数字 | 阅读分钟数 |
| `nav.next_path` / `nav.next_title` | 字符串 | 下一篇（更旧） |
| `nav.prev_path` / `nav.prev_title` | 字符串 | 上一篇（更新） |
| `render_data.stats/page/nav` | 对象 | 节点自带的渲染数据（与顶层 `stats/page/nav` 同源） |

`page.html` 渲染后还会做 HTML 后处理（见 §5.8）。

## 5.6 TOC 数据项

`build_toc_items()` 取根节点直接子节点中 `visibility` 既不是 `hidden` 也不是 `hidden_in_toc` 的项，序列化为 `catalog_node` JSON，并排序（目录优先，再按 `write_time` 降序，时间相同按 `path` 升序）。该数组用于文章页/统计页的导航。

`emit_toc` / `emit_friends` 中会对每个目录页构造：

| 键 | 结构 | 说明 |
|---|---|---|
| `parent.url` | 字符串 | 父目录 `link_url` 加上 `public_url.path_prefix` 的完整链接；根目录为空 |
| `parent.label` | 字符串 | 父目录显示名；父目录为根时固定为 `/` |
| `items` | 数组 | 当前目录直接子节点（过滤 `hidden` 与 `hidden_in_toc`；目录优先，再按时间降序、`path` 升序） |
| `current` | 对象 | 当前目录节点 JSON（含 `render_data`）；其中 `current.path` 被覆盖为相对目录树路径（根为 `/`） |
| 其余键 | — | `conf_.json()` 的顶层键（`camus/source_dir/assets_dir/meta/sitemap/filename_case/public_url/protocol/domain_name/path_prefix/render/friends/site/route/git/build`）被合并进来 |

`meta.json` 的 `catalog` 数组按目录树的先序顺序输出，保证结果稳定。

注意：这些目录页数据是「父级 + 子项 + 当前目录」结构，不是完整递归树。

## 5.7 统计页与友链页数据

- 统计页：`{ ...conf, toc, stats }`，其中 `stats` 为对象：
  - `all`：全站聚合；
  - 数字键（如 `"2025"`）：按年份聚合。
  - 每项字段：`articles`、`words`、`read_mins`、`min_time`、`max_time`、`tags`（数组）。
- 友链页：`{ ...conf, toc, friends }`，`friends` 为 §3.6 的数组。

## 5.8 HTML 后处理

文章页最终 HTML 会执行一组替换（`strings::replace(..., map, deep=true)`）：

| 查找 | 替换 |
|---|---|
| ` 00:00:00` | 空串 |
| `<img ` | `<img width="100%"` |

用途：去掉 UTC 时间中的零时分秒、让图片自适应宽度。Front Matter 分隔符在解析阶段已丢弃，正文中的 Markdown 水平线（`<hr>`）会被保留。

## 5.9 渲染错误处理

- 模板 inja 渲染异常：打印出错行号附近内容（`strings::mark_string`）后 `logging::fatal`，退出 127。
- Markdown 引擎缺失/不支持：`logging::fatal` 退出 127。
- 主题文件缺失：`logging::fatal` 退出 127。
- 文章 `link_url` 为空：`logging::fatal("make article failed ...")`。

持久性：渲染结果仅写入输出目录，可随时重建；无运行期缓存（除数据目录中的主题文件与内存队列）。

## 5.10 PDF 模板与样式

PDF 的 HTML 模板与打印样式不再硬编码在 `writer.cpp`，而是作为独立资源放在 `theme/pdf/`（常量 `CAMUS_PDF_THEME_DIR`，`config.h.in`）：

| 文件/目录 | 常量 | 作用 |
|---|---|---|
| `theme/pdf/page.html` | `CAMUS_PDF_TEMPLATE_NAME` | 完整 PDF 中间 HTML 文档模板（inja） |
| `theme/pdf/style.css` | `CAMUS_PDF_STYLE_NAME` | 打印样式，由模板通过 `{{ pdf.style }}` 内联到 `<style>` |
| `theme/pdf/fonts.css` | `CAMUS_PDF_FONTS_NAME` | 字体样式（`@font-face` 与 `body` 的 `font-family`），通过 `{{ pdf.fonts }}` 内联 |
| `assets/fonts/` | `CAMUS_PDF_FONTS_DIR` | 站点与 PDF 共用的字体目录；构建时复制到 `dist/pdf/assets/fonts/`，供 `fonts.css` 以 `assets/fonts/...` 引用 |

加载顺序（`writer::emit_pdf()` → `read_pdf_theme_file()`，`writer.cpp`）：

1. 工作目录 `theme/pdf/<file>`（显式本地覆盖）。
2. `paths::find_resource("theme/pdf/<file>")`：先 `.camus/theme/pdf/<file>`（`-c --full` 复制），再平台数据目录。
3. `page.html` / `style.css` 均不存在时 `logging::fatal("read pdf theme content failed name=...")`，退出 127；`fonts.css` 可选，缺失时 `pdf.fonts` 为空串并继续生成。

`page.html` 使用 inja 渲染，上下文为 §3.7 的配置 JSON 加上 `pdf` 对象：

| 键 | 类型 | 说明 |
|---|---|---|
| `pdf.lang` | 字符串 | 站点标题非空时为 `zh-CN`，否则为 `en` |
| `pdf.title` | 字符串 | 已 HTML 转义的封面标题（站点标题为空时回退 `Camus`） |
| `pdf.style` | 字符串 | `style.css` 经配置 JSON 渲染后的内容，末尾追加由 `pdf.page` 生成的 `@page { size; margin }` 覆盖规则（可写 inja 配置变量；另提供顶层 `pdf_page_offset` 供页码偏移使用） |
| `pdf.page` | 对象 | `size`、`margin`，来自配置 `pdf.page`；默认 `A4` / `20mm 18mm`，空值回退默认 |
| `pdf.fonts` | 字符串 | `fonts.css` 经配置 JSON 渲染后的内容；文件缺失时为空串 |
| `pdf.asset_style` | 布尔 | 站点输出 `css/style.css` 是否存在并成功复制为 `dist/pdf/assets.css`；默认模板为真时在 `<style>` 前输出 `<link rel="stylesheet" href="assets.css">`，保证 PDF 主题样式优先 |
| `pdf.intro` | 字符串 | 引导页 Markdown（`# site.title` + `site.description`）渲染后的 HTML，**未转义** |
| `pdf.cover` | 对象 | `eyebrow`、`title`、`subtitle`、`description`、`meta`（生成日期）、`image`（根 `SUMMARY.md` 的 `cover_image` 指向有效文件时为其复制后的文件名，否则空串）、`has_image`（是否存在有效封面图；空串在 inja 中为真值，模板须用该布尔值判断）；标题等字段按模板需要预转义；默认模板优先输出整页封面图，无图时输出 `title`、`subtitle` |
| `pdf.title_page` | 对象 | `title`、`subtitle`、`description`（`site.description`），均按模板需要预转义 |
| `pdf.meta` | 对象 | `article_count`、`generated_at`、`domain`、`has_domain`（域名是否为空）、`version`；默认模板不显示 `article_count`，且 `has_domain` 为假时不输出 `Site` 行 |
| `pdf.toc` | 数组 | 按真实目录树输出，同层顺序由 `pdf.sort` 决定（见 §3.3.1）。每项含 `anchor`、`index`、`number`（两位序号）、`title`（HTML 转义）、`outline_title`（纯文本，供 PDF 书签使用）、`tags`、`page`（1 起始页码；解析不到时为空串）、`page_label`（`{:03}` 三位补零页码；解析不到时为空串）、`depth`（相对根目录的显示层级，根目录下为 0）、`is_dir`。目录节点 `anchor`/`number`/`tags`/`page`/`page_label` 为空，只作分组标题；文章节点 `anchor` 为 `pdf-article-{n}`。`title`/`tags` 已转义 |
| `pdf.articles` | 数组 | 每项 `anchor`、`index`、`number`、`title`、`tags`、`content`；`content` 为渲染后的文章 HTML，**未转义** |

- 默认模板的扉页与目录使用英文标签（`Site` / `Version` / `Generated` / `Contents`），不输出文章数；扉页元信息按 `Site → Version → Generated` 顺序排列，`Site` 为空（`pdf.meta.has_domain` 为假）时整行省略。
- 页码从正文（`#pdf-intro` 引导页）开始计为第 1 页：封面使用命名页 `page: cover`（四边 0 边距、整页铺满复制后的封面图），扉页、目录使用命名页 `page: front`，其 `@bottom-center` 内容为空；`html { counter-reset: page {{ pdf_page_offset }}; }` 整体偏移页计数。默认模板在 `<body>` 顶部放置 `display: none` 的 `#pdf-cover`、`#pdf-toc`、`#pdf-intro` 链接，使浏览器在 PDF 中生成这些锚点的命名目标（分别对应封面、目录页与正文第一页）。
- 默认 `style.css` 通过 `@page { @bottom-center { content: counter(page, camus-page); color: #000; } }` 在正文页底部居中输出黑色页码。Chrome 不支持 `@counter-style` 的 `pad` 描述符，因此第二遍渲染前会由 `pdf::page_count()` 读取总页数，并追加一个 `@counter-style camus-page { system: fixed; first: 0; symbols: "001" "002" ... }` 覆盖样式，使页脚与目录页码统一为三位补零（页脚与 `pdf.toc[].page_label` 均从正文第 1 页起算，使用同一套正文页序）。
- 默认目录模板按 `pdf.toc[].depth` 缩进渲染多级目录：目录节点输出分组标题，文章节点输出标题与 `page_label`（三位补零页码）；不输出序号（`number`）与标签（`tags`），条目之间不画虚线下划线。最终 PDF 另由 `pdf::write_outline()` 按同一 `pdf.toc` 注入可交互书签大纲。
- **两遍渲染**：第一次生成 PDF 后解析其命名目标（anchor → 页对象）得到各锚点物理页码；以 `pdf-intro` 的物理页计算 `pdf_page_offset = 1 - intro_page`，回填 `pdf.toc[].page = article_page + pdf_page_offset` 并重新渲染 `pdf.style` 与 HTML，再次打印 PDF。解析失败时保留第一遍产物（目录页码为空、目录锚点仍可跳转）。目录页码列在首遍即占位固定宽度，避免第二遍排版位移。

- `style.css` 与 `fonts.css` 先以配置 JSON 单独渲染（可使用 `{{ build.version }}`、`{{ site.title }}` 等配置变量），再分别作为 `pdf.style` / `pdf.fonts` 字符串插入 `page.html`；`page.html` 自身以「配置 JSON + `pdf` 对象」渲染。插入的样式内容不会被二次解析。第二遍打印前会再以更新后的 `pdf_page_offset` 重新渲染一次 `pdf.style`。
- 站点资源中的 `assets/css/style.css`（构建后位于 `dist/html/css/style.css`）会在 PDF 阶段复制为 `dist/pdf/assets.css`，由默认模板以 `<link>` 引在 PDF 内联样式之前，因此不会覆盖 `theme/pdf/style.css`（PDF 主题规则在后、优先级更高）；文件不存在时 `pdf.asset_style` 为假、不输出该 `<link>`。
- `body` 的字体定义默认位于 `fonts.css`，`style.css` 只保留颜色、行高等打印样式。`fonts.css` 中的自定义字体示例默认被注释：将字体文件放入 `assets/fonts/`，取消 `@font-face` 与 `body` 覆盖注释即可启用。
- 站点资源 `assets/fonts/`（构建后位于 `dist/html/fonts/`）会在写 `dist/pdf/index.html` 前递归复制到 `dist/pdf/assets/fonts/`，因此 `url("assets/fonts/<file>.ttf")` 相对 `dist/pdf/index.html` 可正常加载；目录缺失时跳过复制。
- 默认模板的封面/扉页/目录/文章分页样式保持一致，其中封面为整页满铺图片（来源为根 `SUMMARY.md` 的 `cover_image`）、扉页字号较封面缩小、目录按真实目录树多级缩进；用户可整体替换 `page.html`，或只覆盖 `style.css` / `fonts.css`（字体文件放在站点 `assets/fonts/`）。
- 输出 `dist/pdf/index.html` 是模板渲染结果；`dist/pdf/index.md` 仍由程序生成，不受模板影响。
- 页面写入成功与否只影响 PDF 阶段；`page.html`/`style.css` 缺失属于配置错误，会以致命错误退出（与普通主题缺失一致）。
- 模板/样式/字体源文件持久于工作目录 `theme/pdf`、`.camus` 或数据目录；生成的 `dist/pdf` 产物可随时重建。
