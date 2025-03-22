execute_process(
        COMMAND sh -exc "go mod tidy; export CGO_ENABLED=1; go build -v -buildmode=c-archive -o libgomarkdown.a ./..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
