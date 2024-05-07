#include "pagetable_walker_chf.h"
namespace ParametricDramDirectoryMSI{
    PageTableCHF::PageTableCHF(int number_of_levels, int *level_bit_indices, int _table_size_in_bits, int _large_page_percentage, Core* _core, ShmemPerfModel* _m_shmem_perf_model, PWC* pwc, bool pwc_enabled)
    :PageTableWalker(_core->getId(), 0, _m_shmem_perf_model, pwc, pwc_enabled) {   
        this->table_size_in_bits = _table_size_in_bits;
        this->large_page_percentage = _large_page_percentage;
        this->core = _core;
        this->m_shmem_perf_model = _m_shmem_perf_model;

        this->pagefaults_4KB = 0;
        this->pagefaults_2MB = 0;
        this->chain_per_entry_4KB = 0;
        this->chain_per_entry_2MB = 0;
        this->num_accesses_4KB = 0;
        this->num_accesses_2MB = 0;

        total_latency_4KB = SubsecondTime::Zero();
        total_latency_2MB = SubsecondTime::Zero();
        total_latency_page_fault = SubsecondTime::Zero();
        this->stats_radix.number_of_levels = number_of_levels;
        this->stats_radix.address_bit_indices = (int *)malloc((number_of_levels + 1) * sizeof(int));
        for (int i = 0; i < number_of_levels + 1; i++)
        {
            this->stats_radix.address_bit_indices[i] = level_bit_indices[i];
        }
        this->starting_table = InitiateTablePtw((int)pow(2.0, 9), 0); // PL4 starts with 2^9 entries
        this->starting_table->level = 0;

        latency_per_level = new SubsecondTime[number_of_levels];
        String name = "chf_";
        name = name+std::to_string(counter).c_str();
        for (int i = 0; i < 2; i++){
            String metric_name = "page_level_latency_";
            String metric = metric_name + std::to_string(i).c_str();
            registerStatsMetric(name, core_id, metric, &latency_per_level[i]);
        }
        registerStatsMetric("chf", core->getId(), "page_table_walks", &page_table_walks);
        registerStatsMetric("chf", core->getId(), "page_faults_4KB", &pagefaults_4KB);
        registerStatsMetric("chf", core->getId(), "page_faults_2MB", &pagefaults_2MB);
        registerStatsMetric("chf", core->getId(), "conflicts_4KB", &chain_per_entry_4KB);
        registerStatsMetric("chf", core->getId(), "conflicts_2MB", &chain_per_entry_2MB);
        registerStatsMetric("chf", core->getId(), "accesses_4KB", &num_accesses_4KB);
        registerStatsMetric("chf", core->getId(), "accesses_2MB", &num_accesses_2MB);
        registerStatsMetric("chf", core->getId(), "total_latency_4KB", &total_latency_4KB);
        registerStatsMetric("chf", core->getId(), "total_latency_2MB", &total_latency_2MB);
        registerStatsMetric("chf", core->getId(), "total_latency_page_fault", &total_latency_page_fault);
    }

    SubsecondTime PageTableCHF::init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache, CacheCntlr *_cache, Core::lock_signal_t lock_signal, Byte* _data_buf, UInt32 _data_length, bool modeled, bool count) {
        page_table_walks++;
        cache = _cache;
        uint64_t a1;
        int shift_bits = 0;
        SubsecondTime total_latency;
        SubsecondTime t_start;
        SubsecondTime now = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
        
        for (int i = stats_radix.number_of_levels; i >= 1; i--) {
            shift_bits += stats_radix.address_bit_indices[i];
        }
        a1 = ((address >> shift_bits) & 0x1ff); // first level page table

        bool pwc_hit = false;

        if (page_walk_cache_enabled) {
            PWC::where_t pwc_where;
            IntPtr pwc_address = (IntPtr)(&starting_table -> entries[a1]);
            pwc_where = pwc -> lookup(pwc_address, t_start, true, 1, count); // lookup the PL4 PWC
            if(pwc_where == PWC::HIT) 
                pwc_hit = true; 
        }
        if (pwc_hit == true) {
            total_latency = pwc->access_latency.getLatency(); 
        }     
        else {
            t_start = getShmemPerfModel() -> getElapsedTime(ShmemPerfModel::_USER_THREAD);
            IntPtr cache_address = ((IntPtr)(&starting_table->entries[a1])) & (~((64 - 1))); 
            cache->processMemOpFromCore(
                eip,
                lock_signal,
                Core::mem_op_t::READ,
                cache_address, 0,
                data_buf, data_length,
                modeled,
                count, CacheBlockInfo::block_type_t::PAGE_TABLE, SubsecondTime::Zero(), shadow_cache);
            SubsecondTime t_end = getShmemPerfModel() -> getElapsedTime(ShmemPerfModel::_USER_THREAD);
            if(page_walk_cache_enabled)
                total_latency = t_end - t_start + pwc -> miss_latency.getLatency();
            else
                total_latency = t_end - t_start;
            m_shmem_perf_model -> setElapsedTime(ShmemPerfModel::_USER_THREAD, now);
            mem_manager -> tagCachesBlockType(cache_address, CacheBlockInfo::block_type_t::PAGE_TABLE);
            latency_per_level[0] += total_latency;    
        }
        if(starting_table->entries[a1].entry_type == ptw_table_entry_type::PTW_NONE) {
            starting_table->entries[a1] = *CreateNewPtwEntryAtLevelCHF(1, stats_radix.number_of_levels, stats_radix.address_bit_indices, this, address);
            if (starting_table->entries[a1].entry_type == PTW_ADDRESS) {
                starting_table->occupancy_address += 1;
            }
            if (starting_table->entries[a1].entry_type == PTW_TABLE_POINTER) {
                starting_table->occupancy_table += 1;
            }
        }
        if(starting_table->entries[a1].entry_type==ptw_table_entry_type::PTW_ADDRESS){
            latency_per_level[0] += total_latency;
            return total_latency;
        }
        SubsecondTime final_latency = total_latency + InitializeWalkRecursive(eip, address, 2, starting_table -> entries[a1].next_level_table, lock_signal, data_buf,data_length, modeled, count);
        m_shmem_perf_model -> setElapsedTime(ShmemPerfModel::_USER_THREAD, now);
        return total_latency;
    }

    SubsecondTime PageTableCHF::InitializeWalkRecursive(IntPtr eip, IntPtr address, int level, ptw_table* new_table, Core::lock_signal_t lock_signal, Byte* data_buf, UInt32 data_length, bool modeled, bool count) {
        uint64_t a1;
        int shift_bits=0;
        IntPtr pwc_address;
        SubsecondTime t_start;
        SubsecondTime total_latency;
        for (int i = stats_radix.number_of_levels; i >= level; i--) {
            shift_bits += stats_radix.address_bit_indices[i];
        }
        a1 = ((address >> shift_bits) & 0x1ff);
        if (level == 2) { //for PL3, we still use radix page table design
            bool pwc_hit = false;
            if (page_walk_cache_enabled) { 
			    PWC::where_t pwc_where;
                pwc_address = (IntPtr)(&new_table->entries[a1]);
                pwc_where = pwc->lookup(pwc_address, t_start ,true, level, count);
                if( pwc_where == PWC::HIT ) pwc_hit = true; 
            }
            if (pwc_hit == true) {
                total_latency = pwc->access_latency.getLatency(); 
            }
            else {
                t_start = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                IntPtr cache_address = ((IntPtr)(&new_table->entries[a1])) & (~((64 - 1))); 
                cache->processMemOpFromCore(
                    eip, 
                    lock_signal,
                    Core::mem_op_t::READ,
                    cache_address, 0,
                    data_buf, data_length,
                    modeled,
                    count, CacheBlockInfo::block_type_t::PAGE_TABLE, SubsecondTime::Zero());
                SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                if(page_walk_cache_enabled)
                    total_latency = t_end - t_start + pwc->miss_latency.getLatency();
                else
                    total_latency = t_end - t_start;
                mem_manager->tagCachesBlockType(cache_address,CacheBlockInfo::block_type_t::PAGE_TABLE);
                latency_per_level[level-1] += total_latency;
            }
            
            if (new_table->entries[a1].entry_type==ptw_table_entry_type::PTW_NONE){
                new_table->entries[a1] = *CreateNewPtwEntryAtLevelCHF(level, stats_radix.number_of_levels, stats_radix.address_bit_indices, this, address);
                if (new_table->entries[a1].entry_type == PTW_ADDRESS) {
                    new_table->occupancy_address += 1;
                }
                if (new_table->entries[a1].entry_type == PTW_TABLE_POINTER) {
                    new_table->occupancy_table += 1;
                }
            }
            if (new_table->entries[a1].entry_type == ptw_table_entry_type::PTW_ADDRESS) {
                return total_latency;
            }
            return total_latency + InitializeWalkRecursive(eip, address,level + 1, new_table->entries[a1].next_level_table, lock_signal, data_buf, data_length, modeled,count);
        }
        else { // for every entry in PL3, we create a hash page table in the first, when the collisions go higher, we transform the hash page table to flattened page table
            return SubsecondTime::Zero(); 
        }
    }

	int PageTableCHF::init_walk_functional(IntPtr address) {
        return 0;
    }

	bool PageTableCHF::isPageFault(IntPtr address) {
        return 0;
    }

    int handle_page_fault(IntPtr address, int hash_result_4KB, int hash_result2_MB) {
        return 0;
    }

    int PageTableCHF::hash_function(IntPtr address){
        uint64 mask = 0;
        for(int i = 0 ; i < table_size_in_bits; i++){
            mask += (int)pow(2, i);
        }
        char*  new_address = (char*)address;
        uint64 result = CityHash64((const char*)&address, 8) & (mask);
        return result;
    }
}