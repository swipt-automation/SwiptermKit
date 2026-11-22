ROOT := ../..
BUILD := $(ROOT)/build/a64_check$(SUFFIX)
CFLAGS := -std=c23 -O3 -g -fno-common -Wall -Wextra -Werror -I$(ROOT)/VMM -I$(ROOT)/VMM/include/CSwiptermKit -I$(ROOT)/Guest/swiptermd $(DEFINES)
VMM_SOURCES := cpu/cpu_exec.c cpu/cpu_fp_simd.c cpu/cpu_ldst.c cpu/cpu_sysreg.c mmu/mmu.c
VMM_OBJECTS := $(patsubst %.c,$(BUILD)/vmm/%.o,$(VMM_SOURCES)) $(BUILD)/vmm/cpu/cpu_fast_arm64.o
TOOL_OBJECTS := $(BUILD)/a64_check.o $(BUILD)/board_stubs.o $(BUILD)/native.o

$(BUILD)/a64_check: $(VMM_OBJECTS) $(TOOL_OBJECTS)
	clang -o $@ $^

$(BUILD)/vmm/%.o: $(ROOT)/VMM/%.c $(wildcard $(ROOT)/VMM/cpu/*.h $(ROOT)/VMM/cpu/*.inc $(ROOT)/VMM/cpu/generated/*.inc $(ROOT)/VMM/mmu/*.h)
	@mkdir -p $(dir $@)
	clang $(CFLAGS) -c $< -o $@

$(BUILD)/vmm/cpu/cpu_fast_arm64.o: $(ROOT)/VMM/cpu/cpu_fast_arm64.S $(wildcard $(ROOT)/VMM/cpu/*.h $(ROOT)/VMM/cpu/*.inc $(ROOT)/VMM/cpu/generated/*.inc)
	@mkdir -p $(dir $@)
	clang -I$(ROOT)/VMM $(DEFINES) -c $< -o $@

$(BUILD)/%.o: %.c state.h generated/a64_leaves.inc
	@mkdir -p $(dir $@)
	clang $(CFLAGS) -I. -c $< -o $@

$(BUILD)/native.o: native.S state.h
	@mkdir -p $(dir $@)
	clang -I. -c $< -o $@

check: $(BUILD)/a64_check
	$(BUILD)/a64_check coverage slow_path.txt
	$(BUILD)/a64_check stubs 16
	$(BUILD)/a64_check fuzz -n 300
	$(BUILD)/a64_check fuzz -p -n 200
	$(BUILD)/a64_check system 600

.PHONY: check
