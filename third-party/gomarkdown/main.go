package main

import (
    // #include <stdint.h>
	"C"

	"github.com/gomarkdown/markdown"
	"github.com/gomarkdown/markdown/html"
	"github.com/gomarkdown/markdown/parser"
)

func main() { }

//export MarkdownToHTML
func MarkdownToHTML(md *C.char) (length C.uint32_t, html_ptr *C.char) {
	goStr := C.GoString(md)
	goBytes := []byte(goStr)

	defer func() {
	    goStr = ""
	    goBytes = nil
	}()

	extensions := parser.CommonExtensions | parser.AutoHeadingIDs | parser.NoEmptyLineBeforeBlock
	p := parser.NewWithExtensions(extensions)
	doc := p.Parse(goBytes)

	htmlFlags := html.CommonFlags | html.HrefTargetBlank
	opts := html.RendererOptions{
		Flags: htmlFlags,
	}

	result := markdown.Render(doc, html.NewRenderer(opts))
	return C.uint32_t(len(result)), (*C.char)(C.CBytes(result))
}
