#include "pagetable_walker_chf.h"
namespace ParametricDramDirectoryMSI{
    PageTableCHF::PageTableCHF(int table_size_in_bits, int small_page_size_in_bits, int big_page_size_in_bits, int large_page_percentage, Core* _core, ShmemPerfModel* _m_shmem_perf_model, PWC* pwc, bool pwc_enabled)
    :PageTableWalker(_core->getId(), 0, _m_shmem_perf_model, pwc, pwc_enabled) {
        std::cout << "[CHF] first line" << std::endl;
    }
    SubsecondTime PageTableCHF::init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache, CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, UInt32 _data_length, bool modeled, bool count) {
        return SubsecondTime::Zero();
    }
	int PageTableCHF::init_walk_functional(IntPtr address) {
        return 0;        
    }
	bool PageTableCHF::isPageFault(IntPtr address) {
        return 0;
    }
}