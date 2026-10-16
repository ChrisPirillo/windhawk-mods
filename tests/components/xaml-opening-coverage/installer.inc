static bool CbtThreadCreationTime(DWORD tid, FILETIME* created)
{
    HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    if (!thread)
        return false;
    FILETIME exited, kernel, user;
    DWORD exitCode = 0;
    const bool alive =
        GetThreadTimes(thread, created, &exited, &kernel, &user) &&
        GetExitCodeThread(thread, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(thread);
    return alive;
}

static void InstallCbtHookForThread(DWORD tid)
{
    if (!g_settings.RoundedButtons && !g_settings.ModernContextMenus && !g_settings.DisableWindowOpeningAnimations && !ConsistentCaptionTrackingEnabled()) return;
    const bool currentThread = tid == 0 || tid == GetCurrentThreadId();
    const unsigned generation =
        g_cbtHooksGeneration.load(std::memory_order_acquire);
    if (currentThread) {
        if (t_cbtHookInstalled && t_cbtHookGeneration == generation)
            return;
        tid = GetCurrentThreadId();
    }
    FILETIME created = {};
    if (!CbtThreadCreationTime(tid, &created))
        return;

    std::lock_guard<std::mutex> lk(g_cbtHooksMutex);
    auto it = g_cbtHooks.find(tid);
    if (it != g_cbtHooks.end()) {
        if (CompareFileTime(&it->second.created, &created) == 0) {
            if (currentThread) {
                t_cbtHookInstalled = true;
                t_cbtHookGeneration = generation;
            }
            return;
        }
        // The ID now belongs to a newer thread; the old hook ended with the
        // thread it was installed on.
        g_cbtHooks.erase(it);
    }
    HHOOK h = SetWindowsHookExW(WH_CBT, CbtProc, nullptr, tid);
    if (!h)
        return;
    // Entries of exited threads are otherwise only replaced on ID reuse.
    if (g_cbtHooks.size() >= 256) {
        std::erase_if(g_cbtHooks, [](const auto& entry) {
            FILETIME alive = {};
            return !CbtThreadCreationTime(entry.first, &alive) ||
                CompareFileTime(&alive, &entry.second.created) != 0;
        });
    }
    g_cbtHooks[tid] = { h, created };
    if (currentThread) {
        t_cbtHookInstalled = true;
        t_cbtHookGeneration = generation;
    }
}

