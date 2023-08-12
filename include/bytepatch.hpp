#pragma once
#include <functional>
#include <stdio.h>
#include <string.h>
#include "core/logging.hpp"
#include <sys/mman.h>
#include <errno.h>
#include <experimental/array>

class BytePatch
{
public:
    ~BytePatch()
    {
        Shutdown();
    }
    template <std::size_t C>
    BytePatch(std::function<uintptr_t(const char *)> SigScanFunc,
        const char *pattern, size_t offset,
        const std::array<uint8_t, C> &patch_)
    {
        addr = reinterpret_cast<void*>(SigScanFunc(pattern));
        if (!addr)
        {
            logging::Info("Signature not found: %s", pattern);
            return;
        }
        addr = reinterpret_cast<void*>(reinterpret_cast<char*>(addr) + offset);
        Init(patch_.data(), patch_.size());
    }
    template <std::size_t C>
    BytePatch(uintptr_t addr, const std::array<uint8_t, C> &patch_)
        : BytePatch(reinterpret_cast<void*>(addr), patch_)
    {}
    template <std::size_t C>
    BytePatch(void *addr_, const std::array<uint8_t, C> &patch_)
        : addr{ addr_ }
    {
        Init(patch_.data(), patch_.size());
    }
    void Patch()
    {
        PatchWith(patch.get());
    }
    void Shutdown()
    {
        if (!patched)
            return;

        PatchWith(original.get());
        patched = false;
    }
    bool Patched()
    {
        return patched;
    }
private:
    void Init(const uint8_t *data, size_t len)
    {
        patch_size = len;
        patch = std::make_unique<uint8_t[]>(len);
        original = std::make_unique<uint8_t[]>(len);
        memcpy(patch.get(), data, len);
        memcpy(original.get(), addr, len);
    }
    void PatchWith(uint8_t *patch_data)
    {
        if (!addr)
            return;

        uintptr_t page_size = sysconf(_SC_PAGE_SIZE);
        void *page = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & ~(page_size - 1)),
            *page_end = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(addr) + patch_size) & ~(page_size - 1));

        uintptr_t all_pages_size = reinterpret_cast<uintptr_t>(page_end) - reinterpret_cast<uintptr_t>(page) + page_size;
        if (mprotect(page, all_pages_size, PROT_READ | PROT_WRITE | PROT_EXEC))
        {
            logging::Info("Failed to mprotect: %s", strerror(errno));
            return;
        }
        memcpy(addr, patch_data, patch_size);
        patched = true;
        if (mprotect(page, all_pages_size, PROT_READ | PROT_EXEC))
            logging::Info("Failed to reverse mprotect: %s", strerror(errno));
    }
    void *addr = nullptr;
    std::unique_ptr<uint8_t[]> patch, original;
    size_t patch_size = 0;
    bool patched = false;
};
