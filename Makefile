.PHONY: all clean fmt lint

all:
	go run ./internal/cmd/build_libswipterm

clean:
	rm -rf build Frameworks Guest/build

fmt:
	golangci-lint fmt

lint:
	GOOS=darwin golangci-lint run ./...
	GOOS=ios CGO_ENABLED=1 golangci-lint run ./...
