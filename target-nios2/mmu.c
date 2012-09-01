/*
 * Altera Nios II MMU emulation for qemu.
 *
 * Copyright (c) 2012 Chris Wulff
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "cpu.h"
#include "exec-all.h"


uint32_t mmu_read(CPUState *env, uint32_t rn) {
  switch (rn) {
    case CR_TLBACC:
      qemu_log("TLBACC READ %08X\n", env->regs[rn]);
      break;

    case CR_TLBMISC:
      qemu_log("TLBMISC READ %08X\n", env->regs[rn]);
      break;

    case CR_PTEADDR:
      qemu_log("PTEADDR READ %08X\n", env->regs[rn]);
      break;

    default:
      break;
  }
  return env->regs[rn];
}

/* rw - 0 = read, 1 = write, 2 = fetch.  */
unsigned int mmu_translate(CPUState *env,
                           struct nios2_mmu_lookup *lu,
                           target_ulong vaddr, int rw, int mmu_idx)
{
  int pid = (env->regs[CR_TLBMISC] & CR_TLBMISC_PID_MASK) >> 4;
  int vpn = vaddr >> 12;
  
  //qemu_log_mask(CPU_LOG_INT, "mmu_translate vaddr %08X, pid %08X, vpn %08X\n", vaddr, pid, vpn);

  int way;
  for (way = 0; way < env->mmu.tlb_num_ways; way++) {
    struct nios2_tlb_entry *entry = &env->mmu.tlb[(way * env->mmu.tlb_num_ways) + (vpn & env->mmu.tlb_entry_mask)];
    //qemu_log_mask(CPU_LOG_INT, "TLB[%d] TAG %08X, VPN %08X\n", (way * env->mmu.tlb_num_ways) + (vpn & env->mmu.tlb_entry_mask), entry->tag, (entry->tag >> 12));
    if (((entry->tag >> 12) != vpn) ||
        (((entry->tag & (1<<11)) == 0) && ((entry->tag & ((1<<env->mmu.pid_bits)-1)) != pid))) {
      continue;
    }
    lu->vaddr = vaddr & TARGET_PAGE_MASK;
    lu->paddr = (entry->data & CR_TLBACC_PFN_MASK) << TARGET_PAGE_BITS;
    lu->prot = ((entry->data & CR_TLBACC_R) ? PAGE_READ : 0) |
	       ((entry->data & CR_TLBACC_W) ? PAGE_WRITE : 0) |
	       ((entry->data & CR_TLBACC_X) ? PAGE_EXEC : 0);
    //qemu_log_mask(CPU_LOG_INT, "HIT TLB[%d] %08X %08X %08X\n", (way * env->mmu.tlb_num_ways) + (vpn & env->mmu.tlb_entry_mask), lu->vaddr, lu->paddr, lu->prot);
    return 1;
  }
  return 0;
}

static void mmu_flush_pid(CPUState *env, uint32_t pid) {
  int idx;
  qemu_log("TLB Flush PID %d\n", pid);
  for (idx = 0; idx < env->mmu.tlb_num_entries; idx++) {
    struct nios2_tlb_entry *entry = &env->mmu.tlb[idx];
    //qemu_log("TLB[%d] => %08X %08X\n", idx, entry->tag, entry->data);
    if ((entry->tag & ((1<<env->mmu.pid_bits)-1)) == pid) {
      uint32_t vaddr = entry->tag & TARGET_PAGE_MASK;
      //qemu_log("TLB Flush Page %08X\n", vaddr);
      tlb_flush_page(env, vaddr);
    }
  }
}

void mmu_write(CPUState *env, uint32_t rn, uint32_t v) {
//	qemu_log_mask(CPU_LOG_INT, "mmu_write %08X = %08X\n", rn, v);

  switch (rn) {
    case CR_TLBACC:
      qemu_log_mask(CPU_LOG_INT, "TLBACC: IG %02X, FLAGS %c%c%c%c%c, PFN %05X\n", v>>25,
             (v&CR_TLBACC_C) ? 'C' : '.', (v&CR_TLBACC_R) ? 'R' : '.',
             (v&CR_TLBACC_W) ? 'W' : '.', (v&CR_TLBACC_X) ? 'X' : '.',
             (v&CR_TLBACC_G) ? 'G' : '.', v & CR_TLBACC_PFN_MASK);
      // if tlbmisc.WE == 1 then trigger a TLB write on writes to TLBACC
      if (env->regs[CR_TLBMISC] & CR_TLBMISC_WR) {
        int way = (env->regs[CR_TLBMISC] >> 20);
        int vpn = (env->regs[CR_PTEADDR] & CR_PTEADDR_VPN_MASK) >> 2;
        int pid = (env->regs[CR_TLBMISC] & CR_TLBMISC_PID_MASK) >> 4;
        int g = (v & CR_TLBACC_G) ? 1 : 0;
        struct nios2_tlb_entry *entry = &env->mmu.tlb[(way * env->mmu.tlb_num_ways) + (vpn & env->mmu.tlb_entry_mask)];
	uint32_t newTag = (vpn << 12) | (g << 11) | pid;
	uint32_t newData = v & (CR_TLBACC_C | CR_TLBACC_R | CR_TLBACC_W | CR_TLBACC_X | CR_TLBACC_PFN_MASK);
	if ((entry->tag != newTag) || (entry->data != newData)) {
          //qemu_log("TLB Flush Page (OLD) %08X\n", entry->tag & TARGET_PAGE_MASK);
          //tlb_flush_page(env, entry->tag & TARGET_PAGE_MASK); // Flush existing entry
          entry->tag = newTag;
          entry->data = newData;
          qemu_log_mask(CPU_LOG_INT, "TLB[%d] = %08X %08X\n", (way * env->mmu.tlb_num_ways) + (vpn & env->mmu.tlb_entry_mask), entry->tag, entry->data);
	}
      }
      env->regs[CR_TLBACC] = v;
      break;

    case CR_TLBMISC:
      qemu_log_mask(CPU_LOG_INT, "TLBMISC: WAY %X, FLAGS %c%c%c%c%c%c, PID %04X\n", v >> 20,
             (v&CR_TLBMISC_RD) ? 'R' : '.', (v&CR_TLBMISC_WR) ? 'W' : '.',
             (v&CR_TLBMISC_DBL) ? '2' : '.', (v&CR_TLBMISC_BAD) ? 'B' : '.',
             (v&CR_TLBMISC_PERM) ? 'P' : '.', (v&CR_TLBMISC_D) ? 'D' : '.',
	     (v&CR_TLBMISC_PID_MASK) >> 4);
      if ((v&CR_TLBMISC_PID_MASK) != (env->regs[CR_TLBMISC]&CR_TLBMISC_PID_MASK)) {
	mmu_flush_pid(env, (env->regs[CR_TLBMISC]&CR_TLBMISC_PID_MASK) >> 4);
      }
      // if tlbmisc.RD == 1 then trigger a TLB read on writes to TLBMISC
      if (v & CR_TLBMISC_RD) {
        int way = (v >> 20);
        int vpn = (env->regs[CR_PTEADDR] & CR_PTEADDR_VPN_MASK) >> 2;
        struct nios2_tlb_entry *entry = &env->mmu.tlb[(way * env->mmu.tlb_num_ways) + (vpn & env->mmu.tlb_entry_mask)];
        env->regs[CR_TLBACC] &= CR_TLBACC_IGN_MASK;
        env->regs[CR_TLBACC] |= entry->data | ((entry->tag & (1<<11)) ? CR_TLBACC_G : 0);
        env->regs[CR_TLBMISC] &= ~CR_TLBMISC_PID_MASK;
        env->regs[CR_TLBMISC] |= (entry->tag & ((1<<env->mmu.pid_bits)-1)) << 4;
        env->regs[CR_PTEADDR] &= CR_PTEADDR_VPN_MASK;
        env->regs[CR_PTEADDR] |= (entry->tag >> 12) << 2;
      }
      env->regs[CR_TLBMISC] = v;
      break;

    case CR_PTEADDR:
      qemu_log_mask(CPU_LOG_INT, "PTEADDR: PTBASE %03X, VPN %05X\n", v >> 22, (v & CR_PTEADDR_VPN_MASK) >> 2);
      env->regs[CR_PTEADDR] = v;
      break;

    default:
      break;
  }
}

void mmu_init(struct nios2_mmu *mmu) {
  qemu_log_mask(CPU_LOG_INT, "mmu_init\n");

  mmu->pid_bits = 8;          // TODO: get this from ALTR,pid-num-bits
  mmu->tlb_num_ways = 16;     // TODO: get this from ALTR,tlb-num-ways
  mmu->tlb_num_entries = 256; // TODO: get this from ALTR,tlb-num-entries
  mmu->tlb_entry_mask = (mmu->tlb_num_entries/mmu->tlb_num_ways) - 1;

  mmu->tlb = (struct nios2_tlb_entry*)malloc(sizeof(struct nios2_tlb_entry)*mmu->tlb_num_entries);
}

