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

## 参数

位于 markdown 文件头部

```yaml
---
short-path: 短链接名
display-name: 标题
date: 日期
ready: true
---
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
posts_directory=posts
out_directory=html
main_title=main title
main_subtitle=main subtitle
main_description=main description

[template]
home_template_file=template/home_template.html
page_template_file=template/page_template.html
```
