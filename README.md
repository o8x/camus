Camus
=====

由 C++ 开发的复古 blog 生成工具，markdown 解析工具采用 maddy，生成内容仅包含 ul li 等元素和基本边距美化，不含其他任何特效与美化。支持通过模板修改样式，默认安装 foundation CSS v6.9.0

## 用法

直接读取当前目录

```shell
camus
```

设置工作目录

```shell
camus [./blog]
```

### 配置文件

目录结构

```shell
├── blog
│     ├── camus.ini
│     ├── html
│     │     └── index.html
│     ├── posts
│     │     └── first page.md
│     └── template
│         ├── home_template.html
│         └── page_template.html
```

程序启动将会自动加载 ${work_dir}/camus.ini

```ini
[camus]
; markdown 要读取的文件目录
posts_directory=posts
; html 输出目录
out_directory=html
; 在没有 short_path 的情况下定义文件名，可选 uuid(自动生成), original_filename (原文件名)
filename_type=original_filename
; 渲染引擎，可切换 gomarkdown 和 cmark，cmark 不支持表格
markdown_engine=gomarkdown

[template]
; 主页模板
home_template_file=template/home_template.html
; 文章内页模板
page_template_file=template/page_template.html
; 主标题，显示在首页的 title 中
main_title=Just Another Camus blog
; 副标题，显示在首页的描述中
main_subtitle=Static blog generated by Camus
; 描述，显示在首页，一般是一段作者信息等文字
main_description=Rome was not built in one day.
```

## 文章参数

位于 markdown 文件头部

```yaml
---
short-path: simple1
display-name: 示例1
date: 2025-03-21
visibility: open
---
```

| 参数           | 用途                                                       |
|--------------|----------------------------------------------------------|
| short-path   | 不为空时，优先使用 short-path 作为最终生成文件名                           |
| display-name | 显示名称，即文章标题和目录中显示的标题                                      |
| date         | 文章发布时间                                                   |
| visibility   | 文章可见性，可选 open（默认），hidden（隐藏，不渲染）, hidden-in-toc（不显示在目录中） |

### 隐藏内容

支持在文章中使用 `--hidden-section-start|end` 标记隐藏多行内容

```
...
--hidden-section-start
这里的内容将不会被渲染
--hidden-section-end
...
```
