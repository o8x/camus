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

1. 默认读取 `.camus/theme/{theme_name}/{type}.html`（`-i` 安装的内嵌主题）。
2. 若本地存在 `theme/{theme_name}/{type}.html`，优先使用本地覆盖。
3. 文件不存在时 `logging::fatal("read theme content failed ...")`，进程退出 127。

`theme_name` 默认 `default`；示例站点使用 `terminal`。HTML 引擎固定为 `default`（inja），不再从配置读取，`theme_name` 也不会再拼接引擎子路径。

## 5.2 Markdown 渲染

`render_markdown(html, engine, opt)`（`render.cpp`）：

- 仅支持 `engine == "cmark-gfm"`；其它引擎 `logging::fatal("unsupported engine type ...")`。
- 调用 `cmark_markdown_to_html`，失败返回空串。
- 选项位由 `camus.render.markdown_options` 映射（见 §3.4），Debug/RelWithDebInfo 构建默认带 `CMARK_OPT_SOURCEPOS`。

## 5.3 文章渲染流程

`render_single_article()`（`writer.cpp:317`）：

```mermaid
flowchart TD
    A[contents 以换行连接为 markdown] --> B[替换 env 变量]
    B --> C[计算有效字数与阅读时长, 累加统计]
    C --> D[cmark-gfm 渲染 Markdown -> html]
    D --> E[html 作为 inja 模板, 以配置 JSON 渲染]
    E --> F[组装文章数据 j]
    F --> G[inja 渲染 page.html]
    G --> H[HTML 后处理替换]
    H --> I[写入 link_url 与 real_url]
```

- **env 替换**：对每个 `.env` 键替换 `{{ env.key }}` 与 `{{env.key}}` 两种写法（键为小写）。
- **Markdown 内联模板**：Markdown 渲染后的 HTML 会再用配置 JSON 做一次 inja 渲染，因此正文中可使用 `{{ build.version }}` 等配置变量（注意与 Markdown 语法/花括号冲突时的转义需求）。
- `display_name` 也会先经 inja 渲染，再清洗 `/`、`\`、`'`、`"` 为空格（§4.5）。

## 5.4 inja 自定义函数

在 `writer` 构造时注册（`writer.cpp:661-689`），可在任意主题模板/正文中使用：

| 函数 | 签名 | 行为 | 示例 |
|---|---|---|---|
| `format_datetime` | `format_datetime(unix_ts)` → string | UTC `%Y-%m-%d %H:%M:%S` | `{{ format_datetime(page.write_time) }}` |
| `format_date` | `format_date(unix_ts)` → string | UTC `%Y-%m-%d` | `{{ format_date(page.write_time) }}` |
| `with_prefix` | `with_prefix(path)` → string | `clean_path(path, deploy.path_prefix)` | `{{ with_prefix("index.html") }}` |
| `with_cdn` | `with_cdn(path)` → string | 若 `.env` 含键 `with_cdn`，则拼接 `cdn_accelerate` 前缀；否则退化为 `with_prefix` | `{{ with_cdn("css/a.css") }}` |

`with_cdn` 使用 `envs.at("cdn_accelerate")`，因此启用时 `.env` 需同时提供 `WITH_CDN` 与 `CDN_ACCELERATE`（键会转小写）。

## 5.5 文章页渲染数据

`render_single_article` 组装的 JSON `j`（`writer.cpp:359-369`）由配置数据叠加节点数据得到：

| 键 | 结构 | 说明 |
|---|---|---|
| `camus` / `site` / `route` / `env` / `git` / `build` | 见 §3.8 | 站点配置 |
| `title` | 字符串 | 文章显示名（来自节点 `to_json`） |
| `write_time` | 数字 | 文章时间戳 |
| `path` | 字符串 | `real_url` 去掉前缀 |
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

`build_toc_items()`（`writer.cpp:263`）取根节点直接子节点中 `visibility != hidden` 的项，序列化为 `catalog_node` JSON，并排序（目录优先，再按 `write_time` 降序）。该数组用于文章页/统计页的导航。

`emit_toc` / `emit_friends` 中会对每个目录页构造：

| 键 | 结构 | 说明 |
|---|---|---|
| `parent.url` | 字符串 | 父目录 `link_url`；根目录为空 |
| `parent.label` | 字符串 | 父目录显示名；父 URL 为 `/` 时固定为 `/` |
| `items` | 数组 | 当前目录直接子节点（排序：目录优先，再按时间降序） |
| `current` | 对象 | 当前目录节点 JSON（含 `render_data`） |
| 其余键 | — | `conf_.json()` 的顶层键（`camus/site/route/env/git/build`）被合并进来 |

注意：这些目录页数据是「父级 + 子项 + 当前目录」结构，不是完整递归树。

## 5.7 统计页与友链页数据

- 统计页：`{ ...conf, toc, stats }`，其中 `stats` 为对象：
  - `all`：全站聚合；
  - 数字键（如 `"2025"`）：按年份聚合。
  - 每项字段：`articles`、`words`、`read_mins`、`min_time`、`max_time`、`tags`（数组）。
- 友链页：`{ ...conf, toc, friends }`，`friends` 为 §3.6 的数组。

## 5.8 HTML 后处理

文章页最终 HTML 会执行一组替换（`writer.cpp:380-390`，`strings::replace(..., map, deep=true)`，共两轮）：

| 查找 | 替换 |
|---|---|
| ` 00:00:00` | 空串 |
| `<img ` | `<img width="100%"` |
| `<hr>` | 空串 |
| `<hr/>` | 空串 |
| `<hr />` | 空串 |

用途：去掉 UTC 时间中的零时分秒、让图片自适应宽度、移除 Front Matter 关闭分隔线与 Markdown 分隔线产生的水平线。

## 5.9 渲染错误处理

- 模板 inja 渲染异常：打印出错行号附近内容（`strings::mark_string`）后 `logging::fatal`，退出 127。
- Markdown 引擎缺失/不支持：`logging::fatal` 退出 127。
- 主题文件缺失：`logging::fatal` 退出 127。
- 文章 `link_url` 为空：`logging::fatal("make article failed ...")`。

持久性：渲染结果仅写入输出目录，可随时重建；无运行期缓存（除 `.camus` 主题文件与内存队列）。
