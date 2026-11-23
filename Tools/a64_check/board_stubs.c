#include "board/board_internal.h"
#include "cpu/cpu_internal.h"

static void stop(vcpu_t *cpu) {
    cpu->stop_reason = VCPU_STOP_HALT;
    atomic_store_explicit(&cpu->stop_requested, true, memory_order_release);
}

void vmm_vcpu_wfi(vcpu_t *cpu) { stop(cpu); }
void vmm_vcpu_wfe(vcpu_t *cpu) { stop(cpu); }
void vmm_vcpu_sev(vmm_machine_t *) {}
void vmm_vcpu_wfe_notify_write(vmm_machine_t *, uintptr_t) {}
bool vmm_handle_psci(vcpu_t *) { return false; }
bool vmm_preboot_trap(vcpu_t *) { return false; }
uint64_t vmm_mmio_read(vmm_machine_t *, vcpu_t *, uint64_t, uint32_t) { return 0; }
void vmm_mmio_write(vmm_machine_t *, vcpu_t *, uint64_t, uint32_t, uint64_t) {}
uint64_t vmm_timer_cntfrq(void) { return 24000000; }
uint64_t vmm_timer_cntvct(const vmm_machine_t *) { return 0; }
void vmm_timer_write(vcpu_t *, uint64_t *reg, uint64_t value) { *reg = value; }
