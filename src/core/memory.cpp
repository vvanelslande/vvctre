// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include "audio_core/dsp_interface.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/fastmem_mapper.h"
#include "common/logging/log.h"
#include "common/swap.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/lock.h"
#include "core/memory.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Memory {

class RasterizerCacheMarker {
public:
    void Mark(VAddr addr, bool cached) {
        bool* p = At(addr);
        if (p)
            *p = cached;
    }

    bool IsCached(VAddr addr) {
        bool* p = At(addr);
        if (p)
            return *p;
        return false;
    }

private:
    bool* At(VAddr addr) {
        if (addr >= VRAM_VADDR && addr < VRAM_VADDR_END) {
            return &vram[(addr - VRAM_VADDR) / PAGE_SIZE];
        }
        if (addr >= LINEAR_HEAP_VADDR && addr < LINEAR_HEAP_VADDR_END) {
            return &linear_heap[(addr - LINEAR_HEAP_VADDR) / PAGE_SIZE];
        }
        if (addr >= NEW_LINEAR_HEAP_VADDR && addr < NEW_LINEAR_HEAP_VADDR_END) {
            return &new_linear_heap[(addr - NEW_LINEAR_HEAP_VADDR) / PAGE_SIZE];
        }
        return nullptr;
    }

    std::array<bool, VRAM_SIZE / PAGE_SIZE> vram{};
    std::array<bool, LINEAR_HEAP_SIZE / PAGE_SIZE> linear_heap{};
    std::array<bool, NEW_LINEAR_HEAP_SIZE / PAGE_SIZE> new_linear_heap{};
};

u8* PageTable::Get(VAddr vaddr) const {
    u8* page_ptr = pointers[vaddr >> PAGE_BITS];
    if (!page_ptr)
        return nullptr;
    return page_ptr + vaddr;
}

void PageTable::Set(PageType page_type, VAddr vaddr, u8* backing_memory) {
    attributes[vaddr >> PAGE_BITS] = page_type;
    if (backing_memory) {
        pointers[vaddr >> PAGE_BITS] = backing_memory - vaddr;
        DEBUG_ASSERT_MSG(pointers[vaddr >> PAGE_BITS], "Unintentional nullptr within pointers");
    } else {
        pointers[vaddr >> PAGE_BITS] = nullptr;
    }
}

class MemorySystem::Impl {
public:
    static constexpr size_t required_backing_memory = Memory::FCRAM_SIZE + Memory::VRAM_SIZE;
    Common::FastmemMapper fastmem_mapper{required_backing_memory};

    Common::BackingMemory fcram = fastmem_mapper.Allocate(Memory::FCRAM_SIZE);
    Common::BackingMemory vram = fastmem_mapper.Allocate(Memory::VRAM_SIZE);

    PageTable* current_page_table = nullptr;
    RasterizerCacheMarker cache_marker;
    std::vector<PageTable*> page_table_list;

    AudioCore::DspInterface* dsp = nullptr;
};

MemorySystem::MemorySystem() : impl(std::make_unique<Impl>()) {}
MemorySystem::~MemorySystem() = default;

void MemorySystem::ResetPageTable(PageTable& page_table) {
    page_table.pointers.fill(nullptr);
    page_table.attributes.fill(Memory::PageType::Unmapped);
}

void MemorySystem::SetCurrentPageTable(PageTable* page_table) {
    impl->current_page_table = page_table;
}

PageTable* MemorySystem::GetCurrentPageTable() const {
    return impl->current_page_table;
}

void MemorySystem::MapPages(PageTable& page_table, u32 base, u32 size, u8* memory, PageType type) {
    LOG_DEBUG(HW_Memory, "Mapping {} onto {:08X}-{:08X}", (void*)memory, base * PAGE_SIZE,
              (base + size) * PAGE_SIZE);

    RasterizerFlushVirtualRegion(base << PAGE_BITS, size * PAGE_SIZE,
                                 FlushMode::FlushAndInvalidate);

    u32 end = base + size;
    while (base != end) {
        ASSERT_MSG(base < PAGE_TABLE_NUM_ENTRIES, "out of range mapping at {:08X}", base);

        page_table.Set(type, base << PAGE_BITS, memory);

        // If the memory to map is already rasterizer-cached, mark the page
        if (type == PageType::Memory && impl->cache_marker.IsCached(base * PAGE_SIZE)) {
            page_table.SetRasterizerCachedMemory(base);
            impl->fastmem_mapper.Unmap(page_table, base * PAGE_SIZE, PAGE_SIZE);
        } else if (memory != nullptr) {
            impl->fastmem_mapper.Map(page_table, base * PAGE_SIZE, memory, PAGE_SIZE);
        } else {
            impl->fastmem_mapper.Unmap(page_table, base * PAGE_SIZE, PAGE_SIZE);
        }

        base += 1;
        if (memory != nullptr)
            memory += PAGE_SIZE;
    }
}

void MemorySystem::MapMemoryRegion(PageTable& page_table, VAddr base, u32 size, u8* target) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:08X}", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: {:08X}", base);
    MapPages(page_table, base / PAGE_SIZE, size / PAGE_SIZE, target, PageType::Memory);
}

void MemorySystem::UnmapRegion(PageTable& page_table, VAddr base, u32 size) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:08X}", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: {:08X}", base);
    MapPages(page_table, base / PAGE_SIZE, size / PAGE_SIZE, nullptr, PageType::Unmapped);
}

u8* MemorySystem::GetPointerForRasterizerCache(VAddr addr) {
    if (addr >= LINEAR_HEAP_VADDR && addr < LINEAR_HEAP_VADDR_END) {
        return impl->fcram.Get() + (addr - LINEAR_HEAP_VADDR);
    }
    if (addr >= NEW_LINEAR_HEAP_VADDR && addr < NEW_LINEAR_HEAP_VADDR_END) {
        return impl->fcram.Get() + (addr - NEW_LINEAR_HEAP_VADDR);
    }
    if (addr >= VRAM_VADDR && addr < VRAM_VADDR_END) {
        return impl->vram.Get() + (addr - VRAM_VADDR);
    }
    UNREACHABLE();
}

void MemorySystem::RegisterPageTable(PageTable* page_table) {
    impl->page_table_list.push_back(page_table);
}

void MemorySystem::UnregisterPageTable(PageTable* page_table) {
    impl->page_table_list.erase(
        std::find(impl->page_table_list.begin(), impl->page_table_list.end(), page_table));
}

template <typename T>
T MemorySystem::Read(const VAddr vaddr) {
    if (const u8* ptr = impl->current_page_table->Get(vaddr)) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        T value;
        std::memcpy(&value, ptr, sizeof(T));
        return value;
    }

    PageType type = impl->current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Read{} @ 0x{:08X} at PC 0x{:08X}", sizeof(T) * 8, vaddr,
                  Core::System::GetInstance().CPU().GetPC());
        return 0;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ {:08X}", vaddr);
        break;
    case PageType::RasterizerCachedMemory: {
        RasterizerFlushVirtualRegion(vaddr, sizeof(T), FlushMode::Flush);

        T value;
        std::memcpy(&value, GetPointerForRasterizerCache(vaddr), sizeof(T));
        return value;
    }
    default:
        UNREACHABLE();
    }
}

template <typename T>
void MemorySystem::Write(const VAddr vaddr, const T data) {
    if (u8* ptr = impl->current_page_table->Get(vaddr)) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        std::memcpy(ptr, &data, sizeof(T));
        return;
    }

    PageType type = impl->current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Write{} 0x{:08X} @ 0x{:08X} at PC 0x{:08X}",
                  sizeof(data) * 8, (u32)data, vaddr, Core::System::GetInstance().CPU().GetPC());
        return;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ {:08X}", vaddr);
        break;
    case PageType::RasterizerCachedMemory: {
        RasterizerFlushVirtualRegion(vaddr, sizeof(T), FlushMode::Invalidate);
        std::memcpy(GetPointerForRasterizerCache(vaddr), &data, sizeof(T));
        break;
    }
    default:
        UNREACHABLE();
    }
}

bool MemorySystem::IsValidPhysicalAddress(const PAddr paddr) {
    return GetPhysicalPointer(paddr) != nullptr;
}

bool MemorySystem::IsValidVirtualAddress(const Kernel::Process& process, const VAddr vaddr) {
    auto& page_table = process.vm_manager.page_table;

    if (page_table.Get(vaddr))
        return true;

    if (page_table.attributes[vaddr >> PAGE_BITS] == PageType::RasterizerCachedMemory)
        return true;

    return false;
}

u8* MemorySystem::GetPointer(const VAddr vaddr) {
    if (u8* ptr = impl->current_page_table->Get(vaddr)) {
        return ptr;
    }

    if (impl->current_page_table->attributes[vaddr >> PAGE_BITS] ==
        PageType::RasterizerCachedMemory) {
        return GetPointerForRasterizerCache(vaddr);
    }

    LOG_ERROR(HW_Memory, "unknown GetPointer @ 0x{:08x} at PC 0x{:08X}", vaddr,
              Core::System::GetInstance().CPU().GetPC());
    return nullptr;
}

std::string MemorySystem::ReadCString(VAddr vaddr, std::size_t max_length) {
    std::string string;
    string.reserve(max_length);
    for (std::size_t i = 0; i < max_length; ++i) {
        char c = Read8(vaddr);
        if (c == '\0')
            break;
        string.push_back(c);
        ++vaddr;
    }
    string.shrink_to_fit();
    return string;
}

u8* MemorySystem::GetPhysicalPointer(PAddr address) {
    struct MemoryArea {
        PAddr paddr_base;
        u32 size;
    };

    static constexpr MemoryArea memory_areas[] = {
        {VRAM_PADDR, VRAM_SIZE},
        {DSP_RAM_PADDR, DSP_RAM_SIZE},
        {FCRAM_PADDR, FCRAM_SIZE},
    };

    const auto area =
        std::find_if(std::begin(memory_areas), std::end(memory_areas), [&](const auto& area) {
            // Note: the region end check is inclusive because the user can pass in an address that
            // represents an open right bound
            return address >= area.paddr_base && address <= area.paddr_base + area.size;
        });

    if (area == std::end(memory_areas)) {
        LOG_DEBUG(HW_Memory, "unknown GetPhysicalPointer @ 0x{:08X} at PC 0x{:08X}", address,
                  Core::System::GetInstance().CPU().GetPC());
        return nullptr;
    }

    u32 offset_into_region = address - area->paddr_base;

    u8* target_pointer = nullptr;
    switch (area->paddr_base) {
    case VRAM_PADDR:
        target_pointer = impl->vram.Get() + offset_into_region;
        break;
    case DSP_RAM_PADDR:
        target_pointer = impl->dsp->GetDspMemory().data() + offset_into_region;
        break;
    case FCRAM_PADDR:
        target_pointer = impl->fcram.Get() + offset_into_region;
        break;
    default:
        UNREACHABLE();
    }

    return target_pointer;
}

/// For a rasterizer-accessible PAddr, gets a list of all possible VAddr
static std::vector<VAddr> PhysicalToVirtualAddressForRasterizer(PAddr addr) {
    if (addr >= VRAM_PADDR && addr < VRAM_PADDR_END) {
        return {addr - VRAM_PADDR + VRAM_VADDR};
    }
    if (addr >= FCRAM_PADDR && addr < FCRAM_PADDR_END) {
        return {addr - FCRAM_PADDR + LINEAR_HEAP_VADDR, addr - FCRAM_PADDR + NEW_LINEAR_HEAP_VADDR};
    }
    if (addr >= FCRAM_PADDR_END && addr < FCRAM_PADDR_END) {
        return {addr - FCRAM_PADDR + NEW_LINEAR_HEAP_VADDR};
    }
    // While the physical <-> virtual mapping is 1:1 for the regions supported by the cache,
    // some games (like Pokemon Super Mystery Dungeon) will try to use textures that go beyond
    // the end address of VRAM, causing the Virtual->Physical translation to fail when flushing
    // parts of the texture.
    LOG_ERROR(HW_Memory,
              "Trying to use invalid physical address for rasterizer: {:08X} at PC 0x{:08X}", addr,
              Core::System::GetInstance().CPU().GetPC());
    return {};
}

void MemorySystem::RasterizerMarkRegionCached(PAddr start, u32 size, bool cached) {
    if (start == 0) {
        return;
    }

    u32 num_pages = ((start + size - 1) >> PAGE_BITS) - (start >> PAGE_BITS) + 1;
    PAddr paddr = start;

    for (unsigned i = 0; i < num_pages; ++i, paddr += PAGE_SIZE) {
        for (VAddr vaddr : PhysicalToVirtualAddressForRasterizer(paddr)) {
            impl->cache_marker.Mark(vaddr, cached);
            for (PageTable* page_table : impl->page_table_list) {
                PageType& page_type = page_table->attributes[vaddr >> PAGE_BITS];

                if (cached) {
                    // Switch page type to cached if now cached
                    switch (page_type) {
                    case PageType::Memory:
                        page_table->SetRasterizerCachedMemory(vaddr);
                        impl->fastmem_mapper.Unmap(*page_table, vaddr, PAGE_SIZE);
                        break;
                    default:
                        break;
                    }
                } else {
                    // Switch page type to uncached if now uncached
                    switch (page_type) {
                    case PageType::RasterizerCachedMemory: {
                        u8* ptr = GetPointerForRasterizerCache(vaddr & ~PAGE_MASK);
                        page_table->SetMemory(vaddr, ptr);
                        impl->fastmem_mapper.Map(*page_table, vaddr, ptr, PAGE_SIZE);
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
        }
    }
}

void RasterizerFlushRegion(PAddr start, u32 size) {
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VideoCore::g_renderer->Rasterizer()->FlushRegion(start, size);
}

void RasterizerInvalidateRegion(PAddr start, u32 size) {
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VideoCore::g_renderer->Rasterizer()->InvalidateRegion(start, size);
}

void RasterizerFlushAndInvalidateRegion(PAddr start, u32 size) {
    // Since pages are unmapped on shutdown after video core is shutdown, the renderer may be
    // null here
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VideoCore::g_renderer->Rasterizer()->FlushAndInvalidateRegion(start, size);
}

void RasterizerFlushVirtualRegion(VAddr start, u32 size, FlushMode mode) {
    // Since pages are unmapped on shutdown after video core is shutdown, the renderer may be
    // null here
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VAddr end = start + size;

    auto CheckRegion = [&](VAddr region_start, VAddr region_end, PAddr paddr_region_start) {
        if (start >= region_end || end <= region_start) {
            // No overlap with region
            return;
        }

        VAddr overlap_start = std::max(start, region_start);
        VAddr overlap_end = std::min(end, region_end);
        PAddr physical_start = paddr_region_start + (overlap_start - region_start);
        u32 overlap_size = overlap_end - overlap_start;

        auto* rasterizer = VideoCore::g_renderer->Rasterizer();
        switch (mode) {
        case FlushMode::Flush:
            rasterizer->FlushRegion(physical_start, overlap_size);
            break;
        case FlushMode::Invalidate:
            rasterizer->InvalidateRegion(physical_start, overlap_size);
            break;
        case FlushMode::FlushAndInvalidate:
            rasterizer->FlushAndInvalidateRegion(physical_start, overlap_size);
            break;
        }
    };

    CheckRegion(LINEAR_HEAP_VADDR, LINEAR_HEAP_VADDR_END, FCRAM_PADDR);
    CheckRegion(NEW_LINEAR_HEAP_VADDR, NEW_LINEAR_HEAP_VADDR_END, FCRAM_PADDR);
    CheckRegion(VRAM_VADDR, VRAM_VADDR_END, VRAM_PADDR);
}

u8 MemorySystem::Read8(const VAddr addr) {
    return Read<u8>(addr);
}

u16 MemorySystem::Read16(const VAddr addr) {
    return Read<u16_le>(addr);
}

u32 MemorySystem::Read32(const VAddr addr) {
    return Read<u32_le>(addr);
}

u64 MemorySystem::Read64(const VAddr addr) {
    return Read<u64_le>(addr);
}

void MemorySystem::ReadBlock(const Kernel::Process& process, const VAddr src_addr,
                             void* dest_buffer, const std::size_t size) {
    auto& page_table = process.vm_manager.page_table;

    std::size_t remaining_size = size;
    std::size_t page_base = src_addr & ~PAGE_MASK;
    std::size_t page_offset = src_addr & PAGE_MASK;

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>(page_base + page_offset);

        switch (page_table.attributes[page_base >> PAGE_BITS]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped ReadBlock @ 0x{:08X} (start address = 0x{:08X}, size = {}) at PC "
                      "0x{:08X}",
                      current_vaddr, src_addr, size, Core::System::GetInstance().CPU().GetPC());
            std::memset(dest_buffer, 0, copy_amount);
            break;
        }
        case PageType::Memory: {
            const u8* src_ptr = page_table.Get(current_vaddr);
            DEBUG_ASSERT(src_ptr);
            std::memcpy(dest_buffer, src_ptr, copy_amount);
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Flush);
            std::memcpy(dest_buffer, GetPointerForRasterizerCache(current_vaddr), copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_base += PAGE_SIZE;
        page_offset = 0;
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemorySystem::Write8(const VAddr addr, const u8 data) {
    Write<u8>(addr, data);
}

void MemorySystem::Write16(const VAddr addr, const u16 data) {
    Write<u16_le>(addr, data);
}

void MemorySystem::Write32(const VAddr addr, const u32 data) {
    Write<u32_le>(addr, data);
}

void MemorySystem::Write64(const VAddr addr, const u64 data) {
    Write<u64_le>(addr, data);
}

void MemorySystem::WriteBlock(const Kernel::Process& process, const VAddr dest_addr,
                              const void* src_buffer, const std::size_t size) {
    auto& page_table = process.vm_manager.page_table;
    std::size_t remaining_size = size;
    std::size_t page_base = dest_addr & ~PAGE_MASK;
    std::size_t page_offset = dest_addr & PAGE_MASK;

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>(page_base + page_offset);

        switch (page_table.attributes[page_base >> PAGE_BITS]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped WriteBlock @ 0x{:08X} (start address = 0x{:08X}, size = {}) at PC "
                      "0x{:08X}",
                      current_vaddr, dest_addr, size, Core::System::GetInstance().CPU().GetPC());
            break;
        }
        case PageType::Memory: {
            u8* dest_ptr = page_table.Get(current_vaddr);
            DEBUG_ASSERT(dest_ptr);
            std::memcpy(dest_ptr, src_buffer, copy_amount);
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Invalidate);
            std::memcpy(GetPointerForRasterizerCache(current_vaddr), src_buffer, copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_base += PAGE_SIZE;
        page_offset = 0;
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemorySystem::ZeroBlock(const Kernel::Process& process, const VAddr dest_addr,
                             const std::size_t size) {
    auto& page_table = process.vm_manager.page_table;
    std::size_t remaining_size = size;
    std::size_t page_base = dest_addr & ~PAGE_MASK;
    std::size_t page_offset = dest_addr & PAGE_MASK;

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>(page_base + page_offset);

        switch (page_table.attributes[page_base >> PAGE_BITS]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped ZeroBlock @ 0x{:08X} (start address = 0x{:08X}, size = {}) at PC "
                      "0x{:08X}",
                      current_vaddr, dest_addr, size, Core::System::GetInstance().CPU().GetPC());
            break;
        }
        case PageType::Memory: {
            u8* dest_ptr = page_table.Get(current_vaddr);
            DEBUG_ASSERT(dest_ptr);
            std::memset(dest_ptr, 0, copy_amount);
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Invalidate);
            std::memset(GetPointerForRasterizerCache(current_vaddr), 0, copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_base += PAGE_SIZE;
        page_offset = 0;
        remaining_size -= copy_amount;
    }
}

void MemorySystem::CopyBlock(const Kernel::Process& process, VAddr dest_addr, VAddr src_addr,
                             const std::size_t size) {
    CopyBlock(process, process, dest_addr, src_addr, size);
}

void MemorySystem::CopyBlock(const Kernel::Process& dest_process,
                             const Kernel::Process& src_process, VAddr dest_addr, VAddr src_addr,
                             std::size_t size) {
    auto& page_table = src_process.vm_manager.page_table;
    std::size_t remaining_size = size;
    std::size_t page_base = src_addr & ~PAGE_MASK;
    std::size_t page_offset = src_addr & PAGE_MASK;

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>(page_base + page_offset);

        switch (page_table.attributes[page_base >> PAGE_BITS]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped CopyBlock @ 0x{:08X} (start address = 0x{:08X}, size = {}) at PC "
                      "0x{:08X}",
                      current_vaddr, src_addr, size, Core::System::GetInstance().CPU().GetPC());
            ZeroBlock(dest_process, dest_addr, copy_amount);
            break;
        }
        case PageType::Memory: {
            const u8* src_ptr = page_table.Get(current_vaddr);
            DEBUG_ASSERT(src_ptr);
            WriteBlock(dest_process, dest_addr, src_ptr, copy_amount);
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Flush);
            WriteBlock(dest_process, dest_addr, GetPointerForRasterizerCache(current_vaddr),
                       copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_base += PAGE_SIZE;
        page_offset = 0;
        dest_addr += static_cast<VAddr>(copy_amount);
        src_addr += static_cast<VAddr>(copy_amount);
        remaining_size -= copy_amount;
    }
}

u32 MemorySystem::GetFCRAMOffset(u8* pointer) {
    ASSERT(pointer >= impl->fcram.Get() && pointer <= impl->fcram.Get() + Memory::FCRAM_SIZE);
    return pointer - impl->fcram.Get();
}

u8* MemorySystem::GetFCRAMPointer(u32 offset) {
    ASSERT(offset <= Memory::FCRAM_SIZE);
    return impl->fcram.Get() + offset;
}

void MemorySystem::SetDSP(AudioCore::DspInterface& dsp) {
    impl->dsp = &dsp;
}

} // namespace Memory
