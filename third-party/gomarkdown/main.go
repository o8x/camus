package main

import (
	"C"

	"github.com/gomarkdown/markdown"
	"github.com/gomarkdown/markdown/html"
	"github.com/gomarkdown/markdown/parser"
)

func main() { }

//export MarkdownToHTML
func MarkdownToHTML(md *C.char) *C.char {
	goStr := C.GoString(md)
	goBytes := []byte(goStr)

	extensions := parser.CommonExtensions | parser.AutoHeadingIDs | parser.NoEmptyLineBeforeBlock
	p := parser.NewWithExtensions(extensions)
	doc := p.Parse(goBytes)

	htmlFlags := html.CommonFlags | html.HrefTargetBlank
	opts := html.RendererOptions{
		Flags: htmlFlags,
	}

	result := markdown.Render(doc, html.NewRenderer(opts))
	cBytes := C.CBytes(result)
	return (*C.char)(cBytes)
}
