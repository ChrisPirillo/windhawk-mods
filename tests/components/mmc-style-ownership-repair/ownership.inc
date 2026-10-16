static constexpr wchar_t kMmcStyleGeneration[] = L"W32M.MmcStyleGeneration";
struct MmcStyleStrip { HWND hwnd; int gwlIndex; LONG_PTR bit; HANDLE generation; };
static std::mutex g_mmcStyleStripMutex;
static std::vector<MmcStyleStrip> g_mmcStyleStripped;
static std::atomic<bool> g_mmcStyleStripsUnloading{false};
static ULONG_PTR g_mmcStyleNextGeneration = 0;

static bool MmcStyleStripTrack(HWND hwnd, int gwlIndex, LONG_PTR bit)
{
    DWORD pid = 0;
    if (!hwnd || GetWindowThreadProcessId(hwnd, &pid) != GetCurrentThreadId() ||
        pid != GetCurrentProcessId())
        return false;
    std::lock_guard<std::mutex> lock(g_mmcStyleStripMutex);
    if (g_mmcStyleStripsUnloading.load(std::memory_order_acquire))
        return false;
    HANDLE generation = GetPropW(hwnd, kMmcStyleGeneration);
    if (!generation) {
        if (++g_mmcStyleNextGeneration == 0)
            ++g_mmcStyleNextGeneration;
        generation = reinterpret_cast<HANDLE>(g_mmcStyleNextGeneration);
        // Publish ownership before a frame change can reenter or destroy HWND.
        if (!SetPropW(hwnd, kMmcStyleGeneration, generation))
            return false;
    }
    for (const auto& item : g_mmcStyleStripped) {
        if (item.hwnd == hwnd && item.gwlIndex == gwlIndex &&
            item.bit == bit && item.generation == generation)
            return true;
    }
    g_mmcStyleStripped.push_back({ hwnd, gwlIndex, bit, generation });
    return true;
}

static void MmcStyleStripApply(HWND hwnd, int gwlIndex, LONG_PTR bit)
{
    const LONG_PTR style = GetWindowLongPtrW(hwnd, gwlIndex);
    if (!(style & bit) || !MmcStyleStripTrack(hwnd, gwlIndex, bit))
        return;
    SetWindowLongPtrW(hwnd, gwlIndex, style & ~bit);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
        SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static void MmcStyleStripBeginUnload()
{
    std::lock_guard<std::mutex> lock(g_mmcStyleStripMutex);
    g_mmcStyleStripsUnloading.store(true, std::memory_order_release);
}

static void MmcStyleStripRemoveAll(bool unloading = false)
{
    std::vector<MmcStyleStrip> items;
    {
        std::lock_guard<std::mutex> lock(g_mmcStyleStripMutex);
        if (unloading)
            g_mmcStyleStripsUnloading.store(true, std::memory_order_release);
        items.swap(g_mmcStyleStripped);
    }
    for (const auto& item : items) {
        // USER can recycle an HWND. Its property disappears with the original
        // window; a new generation must never inherit the old style record.
        if (GetPropW(item.hwnd, kMmcStyleGeneration) != item.generation)
            continue;
        LONG_PTR style = GetWindowLongPtrW(item.hwnd, item.gwlIndex);
        if (!(style & item.bit)) {
            SetWindowLongPtrW(item.hwnd, item.gwlIndex, style | item.bit);
            SetWindowPos(item.hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
    // Keep the marker until all style indices owned by that lifetime restored.
    for (const auto& item : items) {
        if (GetPropW(item.hwnd, kMmcStyleGeneration) == item.generation)
            RemovePropW(item.hwnd, kMmcStyleGeneration);
    }
}

