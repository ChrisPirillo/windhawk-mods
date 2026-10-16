        // Only this branch queues the minimal creation hook. The full
        // creation hook below is mutually exclusive after this early return.
        HMODULE openingNtUser=GetModuleHandleW(L"win32u.dll");
        auto openingCreate=openingNtUser ? reinterpret_cast<NtUserCreateWindowEx_t>(
            GetProcAddress(openingNtUser,"NtUserCreateWindowEx")) : nullptr;
        if(!openingCreate || !WindhawkUtils::SetFunctionHook(openingCreate,
                OpeningNtUserCreateWindowEx_hook,&NtUserCreateWindowEx_orig))
            Wh_Log(L"Opening animation control: XAML native creation hook unavailable");
        // Existing UI threads are also covered when menus are disabled.
        if (g_settings.ModernContextMenus || g_settings.DisableWindowOpeningAnimations)
            EnumWindows(MenuInitEnumProc, 0);
        return TRUE;