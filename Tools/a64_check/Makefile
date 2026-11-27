ROOT := ../..
BUILD := $(ROOT)/build/a64_check$(SUFFIX)
CFLAGS := -std=c23 -O3 -g -fno-common -Wall -Wextra -Werror -I$(ROOT)/VMM -I$(ROOT)/VMM/include/CSwiptermKit -I$(ROOT)/Guest/swiptermd $(DEFINES) -MMD -MP
VMM_SOURCES := cpu/cpu_exec.c cpu/cpu_fp_simd.c cpu/cpu_jit_memory.c cpu/cpu_ldst.c cpu/cpu_pool.c cpu/cpu_sysreg.c mmu/mmu.c
VMM_OBJECTS := $(patsubst %.c,$(BUILD)/vmm/%.o,$(VMM_SOURCES)) $(BUILD)/vmm/cpu/cpu_fast_arm64.o
TOOL_OBJECTS := $(BUILD)/a64_check.o $(BUILD)/board_stubs.o $(BUILD)/native.o

$(BUILD)/a64_check: $(VMM_OBJECTS) $(TOOL_OBJECTS)
	clang -o $@ $^

$(BUILD)/vmm/%.o: $(ROOT)/VMM/%.c
	@mkdir -p $(dir $@)
	clang $(CFLAGS) -c $< -o $@

$(BUILD)/vmm/cpu/cpu_fast_arm64.o: $(ROOT)/VMM/cpu/cpu_fast_arm64.S
	@mkdir -p $(dir $@)
	clang -I$(ROOT)/VMM $(DEFINES) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	clang $(CFLAGS) -I. -c $< -o $@

$(BUILD)/native.o: native.S
	@mkdir -p $(dir $@)
	clang -I. -MMD -MP -c $< -o $@

-include $(VMM_OBJECTS:.o=.d) $(TOOL_OBJECTS:.o=.d)

check: $(BUILD)/a64_check
	$(BUILD)/a64_check fuzz -n 300
	$(BUILD)/a64_check fuzz -p -n 200
	$(BUILD)/a64_check system 600
	$(BUILD)/a64_check -j fuzz -n 300
	$(BUILD)/a64_check -j fuzz -p -n 200
	$(BUILD)/a64_check -j system 600

.PHONY: check
