static bool IsCurrentProcessInteractive()
{
    static int cached = -1;
    if (cached == -1)
    {
        HWINSTA hwinsta = GetProcessWindowStation();
        USEROBJECTFLAGS flags = {};
        DWORD needed = 0;
        cached = (hwinsta &&
                  GetUserObjectInformationW(hwinsta, UOI_FLAGS, &flags,
                                            sizeof(flags), &needed) &&
                  (flags.dwFlags & WSF_VISIBLE)) ? 1 : 0;
    }
    return cached == 1;
}

