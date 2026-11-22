BUILD := build
HEADERS := $(BUILD)/include/CSwiptermKit
FRAMEWORK := Frameworks/CSwiptermKit.xcframework
ARCHIVE := $(BUILD)/CSwiptermKit.xcframework.zip

SDKS := macosx iphoneos iphonesimulator
macosx_TARGET := arm64-apple-macos14.0
iphoneos_TARGET := arm64-apple-ios17.0
iphonesimulator_TARGET := arm64-apple-ios17.0-simulator

CC := $(shell xcrun --find clang)
LIBTOOL := $(shell xcrun --find libtool)

VMM_SOURCES := $(sort $(wildcard $(patsubst %,VMM/%/*.c,board cpu host mmu virtio))) VMM/cpu/cpu_fast_arm64.S
SLIRP_SOURCES := $(sort $(wildcard VMM/deps/libslirp/src/*.c))
ZSTD_SOURCES := $(sort $(wildcard VMM/deps/zstd/lib/common/*.c VMM/deps/zstd/lib/decompress/*.c))
SOURCES := $(VMM_SOURCES) $(SLIRP_SOURCES) $(ZSTD_SOURCES)

CFLAGS := -O3 -g -fno-common
CPPFLAGS := -IVMM/include/CSwiptermKit -IVMM/deps/libslirp/glib_compat -IVMM/deps/zstd/lib -IVMM/deps/zstd/lib/common \
	-DZSTD_DISABLE_ASM -DZSTD_LEGACY_SUPPORT=0 -DXXH_NAMESPACE=swipterm_zstd_ \
	$(if $(filter 1,$(SWIPTERM_CPU_STATS)),-DCPU_STATS,-UCPU_STATS)

.PHONY: all clean
.DELETE_ON_ERROR:

all: $(ARCHIVE)

define SLICE
$(BUILD)/$(1)/%: TARGET_FLAGS := -target $($(1)_TARGET) -isysroot $(shell xcrun --sdk $(1) --show-sdk-path)
$(VMM_SOURCES:%=$(BUILD)/$(1)/%.o): SOURCE_FLAGS := -std=gnu2x -Wall -Wextra -Werror
$(SLIRP_SOURCES:%=$(BUILD)/$(1)/%.o): SOURCE_FLAGS := -std=gnu11 -w -IVMM/deps/libslirp/src
$(ZSTD_SOURCES:%=$(BUILD)/$(1)/%.o): SOURCE_FLAGS := -std=gnu11 -w

$(BUILD)/$(1)/%.o: % Makefile
	@mkdir -p $$(@D)
	$$(CC) $$(TARGET_FLAGS) $$(CFLAGS) $$(CPPFLAGS) $$(SOURCE_FLAGS) -MMD -MP -c $$< -o $$@

$(BUILD)/$(1)/libCSwiptermKit.a: $(SOURCES:%=$(BUILD)/$(1)/%.o)
	rm -f $$@
	$$(LIBTOOL) -static -no_warning_for_no_symbols -o $$@ $$^

-include $(SOURCES:%=$(BUILD)/$(1)/%.d)
endef
$(foreach sdk,$(SDKS),$(eval $(call SLICE,$(sdk))))

$(HEADERS)/swipterm.h: VMM/include/CSwiptermKit/swipterm.h
	@mkdir -p $(@D)
	cp $< $@

$(HEADERS)/module.modulemap: Makefile
	@mkdir -p $(@D)
	printf '%s\n' 'module CSwiptermKit {' '    header "swipterm.h"' '    export *' '}' > $@

$(FRAMEWORK): $(SDKS:%=$(BUILD)/%/libCSwiptermKit.a) $(HEADERS)/swipterm.h $(HEADERS)/module.modulemap
	rm -rf $@
	xcodebuild -create-xcframework $(foreach sdk,$(SDKS),-library $(BUILD)/$(sdk)/libCSwiptermKit.a -headers $(BUILD)/include) -output $@

$(ARCHIVE): $(FRAMEWORK)
	rm -f $@
	ditto -c -k --norsrc --noextattr --keepParent $< $@

clean:
	rm -rf $(BUILD) Frameworks
