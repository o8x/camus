Camus
=====

![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/o8x/camus/auto-release-tag.yml)
![GitHub License](https://img.shields.io/github/license/o8x/camus)

<a href="README.md">English</a> |
<a href="README_zh-CN.md" >中文</a>

Camus 是一个使用 C++23 开发的静态站点生成器。它将 Markdown 文章转换为静态 HTML 页面，支持主题切换、文件监视、目录生成和 sitemap 生成——全部打包在单个自包含的二进制文件中。

- **自包含**：示例站点模板嵌入在二进制文件中，使用 `-i` 即可一键初始化新站点。
- **高性能**：基于 C++23 构建，Release 模式下经过深度优化。
- **监视模式**：源文件变更时自动重新构建。
- **多主题**：内置 `default`、`accent`、`pixel` 和 `terminal` 主题，也可自定义主题。
- **目录生成**：为多级目录层级生成结构化的目录。
- **Sitemap**：自动生成 `sitemap.xml`。

## 快速开始

```bash
camus -iw mysite        # 将示例模板安装到 ./mysite
camus -Ww mysite        # 监视文件变更并自动重新构建
camus -I                # 检查并显示当前配置
```

构建后的站点默认输出到 `html/` 目录。

## 命令行选项

| 短选项  | 长选项         | 说明                  |
|------|-------------|---------------------|
| `-h` | `--help`    | 显示帮助信息并退出           |
| `-v` | `--version` | 显示版本信息、构建信息和 Git 哈希 |
| `-i` | `--install` | 将示例站点模板安装到工作目录      |
| `-f` | `--force`   | 强制执行（忽略错误，覆盖已有内容）   |
| `-d` | `--debug`   | 启用调试日志              |
| `-W` | `--watch`   | 监视文件变更并自动重新构建       |
| `-D` | `--dryrun`  | 空运行（模拟操作，不实际写入）     |
| `-I` | `--inspect` | 检查并显示当前配置           |
| `-w` | `--workdir` | 指定工作目录（默认：`.`）      |

## 项目结构

执行 `camus -i` 后，工作目录将包含：

```
.
├── .env                  # 环境变量（模板中使用 {{env.KEY}} 引用）
├── .gitignore
├── camus.yaml            # 站点配置
├── .camus/               # 内部模板缓存
├── assets/               # 静态资源（原样复制到输出目录）
├── posts/                # Markdown 文章
└── theme/                # HTML 模板
```

## 配置

- 参见 [示例配置](example)

### .env

渲染时模板变量会被替换。在 HTML 模板中使用 `{{env.key}}` 引用：

## 文章 Front Matter

每篇 Markdown 文件头部使用 YAML front matter：

```yaml
---
short-path: my-post
display-name: 我的第一篇文章
date: 2025-03-21
visibility: open
tags: [ "example", "text" ]
---
```

| 参数             | 说明                                                         |
|----------------|------------------------------------------------------------|
| `short-path`   | 自定义输出文件名（设置后将覆盖源文件名）                                       |
| `display-name` | 文章标题，显示在页面和目录中                                             |
| `date`         | 发布日期                                                       |
| `visibility`   | `open`（默认），`hidden`（隐藏，不渲染），`hidden-in-toc`（不在目录中显示）       |
| `tags`         | 文章标签，值必须符合 JSON 数组格式（如 `[ "example", "text" ]`）               |

### 隐藏内容

使用标记包裹内容以在渲染时排除：

```
--hidden-section-start
此内容不会出现在输出中。
--hidden-section-end
```

## 从源码构建

### 环境要求

- CMake 3.28.3+
- 支持 C++23 的 GCC 或 Clang
- Git（用于克隆子模块）

### 构建

```bash
git clone --recurse-submodules https://github.com/o8x/camus.git camus
make -C camus build
```

模板将在构建后步骤中自动嵌入到二进制文件中。

### Makefile 便捷命令

```bash
make build       # Release 构建
make serve       # 构建并在 localhost:8000 启动服务
make push        # 构建并推送到 GitHub Pages
make package     # 构建发布包
```

## 依赖

所有依赖以 Git 子模块形式包含：

| 库             | 用途                     |
|---------------|------------------------|
| cmark-gfm     | GitHub 风格 Markdown 渲染  |
| yaml-cpp      | YAML 配置文件解析            |
| cpp-httplib   | 监视模式下的 HTTP 服务器        |
| nlohmann/json | TOC 的 JSON 输出          |

## 许可证

本项目遵循 [LICENSE](LICENSE) 文件中规定的条款进行分发。
