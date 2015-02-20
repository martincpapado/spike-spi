// See LICENSE for license details.

#ifndef _RISCV_MMU_H
#define _RISCV_MMU_H

#include "decode.h"
#include "trap.h"
#include "common.h"
#include "config.h"
#include "processor.h"
#include "memtracer.h"
#include <vector>

// virtual memory configuration
typedef reg_t pte_t;
const reg_t LEVELS = sizeof(pte_t) == 8 ? 3 : 2;
const reg_t PTIDXBITS = 10;
const reg_t PGSHIFT = PTIDXBITS + (sizeof(pte_t) == 8 ? 3 : 2);
const reg_t PGSIZE = 1 << PGSHIFT;
const reg_t VPN_BITS = PTIDXBITS * LEVELS;
const reg_t PPN_BITS = 8*sizeof(reg_t) - PGSHIFT;
const reg_t VA_BITS = VPN_BITS + PGSHIFT;

struct insn_fetch_t
{
  insn_func_t func;
  insn_t insn;
};

struct icache_entry_t {
  reg_t tag;
  reg_t pad;
  insn_fetch_t data;
};

// this class implements a processor's port into the virtual memory system.
// an MMU and instruction cache are maintained for simulator performance.
class mmu_t
{
public:
  mmu_t(char* _mem, size_t _memsz);
  ~mmu_t();

  //SPI SLAVE VARIABLES//
  char spi_miso=0x61;
  char spi_miso_temp=0x00;
  int bit_counter=0;
  
  //MCP3008//
  int ch[8]={0x120,0x271,0x312,0x103,0x204,0x305,0x106,0x237};
  //ch[0]=0x100;
  //ch[1]=0x201;
  //ch[2]=0x302;
  //ch[3]=0x103;
  //ch[4]=0x204;
  //ch[5]=0x305;
  //ch[6]=0x106;
  //ch[7]=0x207;
  
  int ch_sel=0x00;
  
  //bool flag_start = false;
  bool first_byte = false;
  bool second_byte = false;
  //bool third_byte = false;
  bool diff=false;
  //SPI SLAVE VARIABLES END//
  
  //SPI SLAVE FUNCTIONS//
void spi_receive(char c)
  {
  	spi_miso_temp = spi_miso;
  	spi_miso<<= 1;
  	spi_miso |= c;
  	//bit_counter++;		
  }
 
void update_clk(char clk)
{
	if (clk)
		bit_counter++;;
}
  
char spi_send()
  {
  	if (bit_counter == 8)
  	{
  		printf("Slave Received: %x\n", spi_miso);
  		if (spi_miso==0x01 && !first_byte && !second_byte)
  		{
  			//flag_start=true;
  			first_byte=true;
  			spi_miso=0x00;
  			bit_counter=0;
  		}
  		else if (first_byte)
  		{
  			first_byte=false;
  			second_byte=true;
  			spi_miso=0xff &(ch[ch_sel]);
  			bit_counter=0;
  		}
  		else //if (second_byte)
  		{
  			first_byte=false;
  			second_byte=false;
  			spi_miso=0x00;
  			bit_counter=0;
  		}
  		//spi_miso=0x61;
  		//bit_counter=0;
  	}
  	if ((bit_counter == 4)&&(first_byte))
  	{
  		diff=(spi_miso&0x08);
  		ch_sel=(spi_miso&0x07);
  		spi_miso=0xff & (ch[ch_sel] >> 4);
  	}
  	if (spi_miso_temp & 0x80)
          return (0x01);
      else
          return (0x00);	
  }
  
 //SPI_SLAVE_END//
  

  // template for functions that load an aligned value from memory
  #define load_func(type) \
    type##_t load_##type(reg_t addr) __attribute__((always_inline)) { \
      void* paddr = translate(addr, sizeof(type##_t), false, false); \
      return *(type##_t*)paddr; \
    }

  // load value from memory at aligned address; zero extend to register width
  load_func(uint8)
  load_func(uint16)
  load_func(uint32)
  load_func(uint64)

  // load value from memory at aligned address; sign extend to register width
  load_func(int8)
  load_func(int16)
  load_func(int32)
  load_func(int64)

  // template for functions that store an aligned value to memory
  #define store_func(type) \
    void store_##type(reg_t addr, type##_t val) { \
      void* paddr = translate(addr, sizeof(type##_t), true, false); \
      *(type##_t*)paddr = val; \
    }

  // store value to memory at aligned address
  store_func(uint8)
  store_func(uint16)
  store_func(uint32)
  store_func(uint64)

  static const reg_t ICACHE_ENTRIES = 1024;

  inline size_t icache_index(reg_t addr)
  {
    // for instruction sizes != 4, this hash still works but is suboptimal
    return (addr / 4) % ICACHE_ENTRIES;
  }

  // load instruction from memory at aligned address.
  icache_entry_t* access_icache(reg_t addr) __attribute__((always_inline))
  {
    reg_t idx = icache_index(addr);
    icache_entry_t* entry = &icache[idx];
    if (likely(entry->tag == addr))
      return entry;

    bool rvc = false; // set this dynamically once RVC is re-implemented
    char* iaddr = (char*)translate(addr, rvc ? 2 : 4, false, true);
    insn_bits_t insn = *(uint16_t*)iaddr;

    if (unlikely(insn_length(insn) == 2)) {
      insn = (int16_t)insn;
    } else if (likely(insn_length(insn) == 4)) {
      if (likely((addr & (PGSIZE-1)) < PGSIZE-2))
        insn |= (insn_bits_t)*(int16_t*)(iaddr + 2) << 16;
      else
        insn |= (insn_bits_t)*(int16_t*)translate(addr + 2, 2, false, true) << 16;
    } else if (insn_length(insn) == 6) {
      insn |= (insn_bits_t)*(int16_t*)translate(addr + 4, 2, false, true) << 32;
      insn |= (insn_bits_t)*(uint16_t*)translate(addr + 2, 2, false, true) << 16;
    } else {
      static_assert(sizeof(insn_bits_t) == 8, "insn_bits_t must be uint64_t");
      insn |= (insn_bits_t)*(int16_t*)translate(addr + 6, 2, false, true) << 48;
      insn |= (insn_bits_t)*(uint16_t*)translate(addr + 4, 2, false, true) << 32;
      insn |= (insn_bits_t)*(uint16_t*)translate(addr + 2, 2, false, true) << 16;
    }

    insn_fetch_t fetch = {proc->decode_insn(insn), insn};
    icache[idx].tag = addr;
    icache[idx].data = fetch;

    reg_t paddr = iaddr - mem;
    if (!tracer.empty() && tracer.interested_in_range(paddr, paddr + 1, false, true))
    {
      icache[idx].tag = -1;
      tracer.trace(paddr, 1, false, true);
    }
    return &icache[idx];
  }

  inline insn_fetch_t load_insn(reg_t addr)
  {
    return access_icache(addr)->data;
  }

  void set_processor(processor_t* p) { proc = p; flush_tlb(); }


  void flush_tlb();
  void flush_icache();

  void register_memtracer(memtracer_t*);
  


private:
  char* mem;
  size_t memsz;
  processor_t* proc;
  memtracer_list_t tracer;

  // implement an instruction cache for simulator performance
  icache_entry_t icache[ICACHE_ENTRIES];

  // implement a TLB for simulator performance
  static const reg_t TLB_ENTRIES = 256;
  char* tlb_data[TLB_ENTRIES];
  reg_t tlb_insn_tag[TLB_ENTRIES];
  reg_t tlb_load_tag[TLB_ENTRIES];
  reg_t tlb_store_tag[TLB_ENTRIES];

  // finish translation on a TLB miss and upate the TLB
  void* refill_tlb(reg_t addr, reg_t bytes, bool store, bool fetch);

  // perform a page table walk for a given virtual address
  pte_t walk(reg_t addr);

  // translate a virtual address to a physical address
  void* translate(reg_t addr, reg_t bytes, bool store, bool fetch)
    __attribute__((always_inline))
  {
    reg_t idx = (addr >> PGSHIFT) % TLB_ENTRIES;
    reg_t expected_tag = addr >> PGSHIFT;
    reg_t* tags = fetch ? tlb_insn_tag : store ? tlb_store_tag :tlb_load_tag;
    reg_t tag = tags[idx];
    void* data = tlb_data[idx] + addr;

    if (unlikely(addr & (bytes-1)))
      store ? throw trap_store_address_misaligned(addr) :
      fetch ? throw trap_instruction_address_misaligned(addr) :
      throw trap_load_address_misaligned(addr);

    if (likely(tag == expected_tag))
      return data;

    return refill_tlb(addr, bytes, store, fetch);
  }
  
  friend class processor_t;
};

#endif
