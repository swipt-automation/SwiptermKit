.PHONY: all vulkan clean clean-vulkan fmt lint

VULKAN_FRAMEWORKS = KosmicKrisp MoltenVK
VULKAN_ARCHIVES = $(VULKAN_FRAMEWORKS:%=build/%.xcframework.zip)

all:
	go run ./internal/cmd/build_libswipterm

vulkan:
	go run ./internal/cmd/build_vulkan

clean:
	rm -rf Frameworks/LibSwipterm.xcframework Guest/build $(filter-out build/vulkan $(VULKAN_ARCHIVES),$(wildcard build/*))

clean-vulkan:
	rm -rf build/vulkan $(VULKAN_FRAMEWORKS:%=Frameworks/%.xcframework) $(VULKAN_ARCHIVES)

fmt:
	golangci-lint fmt

lint:
	GOOS=darwin golangci-lint run ./...
	GOOS=ios CGO_ENABLED=1 golangci-lint run ./...
