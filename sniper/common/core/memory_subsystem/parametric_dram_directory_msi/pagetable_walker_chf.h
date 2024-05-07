#ifndef PTWCHF
#define PTWCHF

#include <stdint.h>
#include <iostream>
#include "page_table_walker_types.h"
#include "pagetable_walker.h"
#include "cache_cntlr.h"
#include "fixed_types.h"
#include "city.h"
#include "memory_manager.h"
namespace ParametricDramDirectoryMSI{
    class PageTableCHF: public PageTableWalker{
        struct stats_radix{
            int number_of_levels; 
            int *address_bit_indices;
        } stats_radix;

        ptw_table* starting_table;

        Core* core;
        CacheCntlr *cache;
        ShmemPerfModel* m_shmem_perf_model;
        SubsecondTime *latency_per_level;

        int table_size_in_bits;
        int large_page_percentage;

        UInt64 page_table_walks;
        UInt64 pagefaults_2MB;
        UInt64 pagefaults_4KB;
        UInt64 chain_per_entry_4KB;
        UInt64 chain_per_entry_2MB;
        UInt64 num_accesses_4KB;
        UInt64 num_accesses_2MB;
        
        SubsecondTime total_latency_4KB;
        SubsecondTime total_latency_2MB;
        SubsecondTime total_latency_page_fault;

        public:
            PageTableCHF(int number_of_levels, int *level_bit_indices, int table_size_in_bits, int large_page_percentage, Core* core, ShmemPerfModel* m_shmem_perf_model, PWC* pwc, bool pwc_enabled);
            int hash_function(IntPtr address);
            SubsecondTime init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache, CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, UInt32 _data_length, bool modeled, bool count);
            SubsecondTime InitializeWalkRecursive(IntPtr eip, IntPtr address,int level, ptw_table* new_table, Core::lock_signal_t lock_signal, Byte* data_buf, UInt32 data_length,bool modeled, bool count);
            int init_walk_functional(IntPtr address);
            int handle_page_fault(IntPtr address, int hash_result, int hash_result2);
            bool isPageFault(IntPtr address);
    };
}
#endif //PTWCHF