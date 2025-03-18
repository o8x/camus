Camus
=====

由 C++ 开发的复古 blog 生成工具，markdown 解析工具采用 maddy，生成内容仅包含 ul li 等元素和基本边距美化，不含其他任何特效与美化。支持通过模板修改样式，默认安装 foundation CSS v6.9.0

## 用法

```shell
camus [./posts]
```

## 参数

位于 markdown 头部

```yaml
---
short_path: 短链接名
display-name: 标题
date: 日期
ready: true
---
```

## 模板

首页 (index_template.html)

```html 
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="utf-8"/>
    <meta name="description" content="{{main-description}}">
    <title>{{main-title}} - {{main-description}}</title>
</head>

{{main-script}}
{{main-style}}

<body>
<div class="main-container">
    <h1 class="main-title">{{main-title}}</h1>
    <p class="main-description">{{main-description}}</p>

    <h4>Posts:</h4>
    <div class="posts">{{posts}}</div>
</div>
</body>

</html>
```

内页 (article_template.html)

```html
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="utf-8"/>
    <meta name="description" content="{{page-description}}">
    <title>{{main-title}} - {{page-title}}</title>
</head>

<body>
<div class="page-container">
    <h1 class="page-title">
        {{page-title}}
    </h1>
    <p class="page-description">
        {{page-description}}
    </p>

    <div class="post">
        {{page-content}}
    </div>
</div>
</body>

</html>
```
