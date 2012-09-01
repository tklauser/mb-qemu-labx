/*
 *  Altera Nios II helper routines.
 *
 *  Copyright (c) 2012 Chris Wulff
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "exec.h"
#include "helper.h"

#if !defined(CONFIG_USER_ONLY)
#define MMUSUFFIX _mmu
#define SHIFT 0
#include "softmmu_template.h"
#define SHIFT 1
#include "softmmu_template.h"
#define SHIFT 2
#include "softmmu_template.h"
#define SHIFT 3
#include "softmmu_template.h"

void tlb_fill (target_ulong addr, int is_write, int mmu_idx, void *retaddr) {
  TranslationBlock *tb;
  CPUState *saved_env;
  unsigned long pc;
  int ret;

  /* XXX: hack to restore env in all cases, even if not called from
     generated code */
  saved_env = env;
  env = cpu_single_env;

  ret = cpu_nios2_handle_mmu_fault(env, addr, is_write, mmu_idx, 1);
  if (unlikely(ret)) {
    if (retaddr) {
      /* now we have a real cpu fault */
      pc = (unsigned long)retaddr;
      tb = tb_find_pc(pc);
      if (tb) {
        /* the PC is inside the translated code. It means that we have
           a virtual CPU fault */
        cpu_restore_state(tb, env, pc, NULL);
      }
    }
  cpu_loop_exit();
  }
  env = saved_env;
}

void helper_raise_exception(uint32_t index) {
  env->exception_index = index;
  cpu_loop_exit();
}

uint32_t helper_mmu_read(uint32_t rn) {
  return mmu_read(env, rn);
}

void helper_mmu_write(uint32_t rn, uint32_t v) {
  mmu_write(env, rn, v);
}

void helper_memalign(uint32_t addr, uint32_t dr, uint32_t wr, uint32_t mask) {
  if (addr & mask) {
    qemu_log_mask(CPU_LOG_INT,
                  "unaligned access addr=%x mask=%x, wr=%d dr=r%d\n",
                   addr, mask, wr, dr);
    env->regs[CR_BADADDR] = addr;
    env->regs[CR_EXCEPTION] = EXCP_UNALIGN << 2;
    helper_raise_exception(EXCP_UNALIGN);
  }
}

void do_unassigned_access(target_phys_addr_t addr, int is_write, int is_exec,
                          int is_asi, int size) {
  // TODO
}

uint32_t helper_divs(uint32_t a, uint32_t b)
{
    return (int32_t)a / (int32_t)b;
}

uint32_t helper_divu(uint32_t a, uint32_t b)
{
    return a / b;
}

#endif // !CONFIG_USER_ONLY

