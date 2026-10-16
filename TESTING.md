# Reusable testing

## Portable component matrix

Python 3.9+ and a Windows-targeting LLVM-MinGW clang++ are needed. On Windows the runner looks for Windhawk's compiler under Program Files; override its location when needed:

```text
python tools/project.py build-tests --compiler "C:/Program Files/Windhawk/Compiler/bin/clang++.exe"
python tools/project.py run-tests --run-hidden
```

Both architectures are selected by default. Use `--arch x64` or `--arch x86` for a subset. Cross-compilation can build the cases, but `run-tests` requires Windows. The scripts never install/enable/disable the mod, write the registry, elevate, send input or restart applications.

The 16 cases in tests/matrix.json build to 32 executables. Coverage includes font policy, Properties tab painting/baseline, independent shortcut settings, picker/address chrome, caption ownership, opening scope/timing, DWM dispatch and structural XAML handling. MMC checks are narrow style/rebar tests, not a working MMC adapter. These cases use only their own hidden native controls; their `dwm.exe` basename deliberately matches the mod's exclusion. They do not run the Windows compositor.

Each build records commands' outcomes and executable hashes; each run checks that test sources and binaries have not changed. Results/logs live under build/. Failures/timeouts are failures, not acceptance passes. Preserve logs when investigating a new defect.

## Important limits

The .inc policy files are extracted snapshots of the implementation at this kit's commit. They are useful regression cases, not an automatic test of whatever is currently installed. After changing production code, update the affected extraction/fixture and verify the old policy fails where appropriate. Do not simply change expected values to make a test green.

Pixel-size models are not real DPI/per-monitor testing. Structural XAML markers are not Microsoft WinUI acceptance. Native samples are not all applications. Component compilation/passes do not prove file-picker flicker, real installer rendering, accessibility or elevated interaction.

## Native fixture sources for future live work

- tests/native-fixtures/font_fixture.cpp and its resource/manifest: native Properties/text/font testbed.
- tests/native-fixtures/acl-page-fixture.cpp: synthetic security-list/property-page lifecycle testbed; not a tool for editing real permissions.
- tests/native-fixtures/installer-fixture.cpp: synthetic classic/Inno-like/WiX-like control routes, not real installer engines.

These sources are reference fixtures, not part of the default runner. Some modes show windows or wait for lifecycle handshakes. Read their entry points and add an exact-build controller before running them. Coordinate focus-taking tests with the user. Do not count synthetic installer routes as real Inno or native WixStdBA acceptance.

## Build and install the mod

Use the installed Windhawk compiler/API/import libraries matching that machine. The upstream checkout contains its validation tools; those currently need Python 3.12+ and PyYAML. This kit's standard-library runner does not need those dependencies.

This archive does not install a mod automatically. For live testing, prepare a distinct preview identity and preserve the existing source/settings first. Keep published and preview builds from running together. Verify exact installed source/DLL identity before drawing conclusions from fixtures. Do not reuse old machine-specific offsets or elevated scripts.

For app exclusions, use Advanced > Custom process exclusion list, one executable/path per line, without commas. The defaults in source exclude mmc.exe and nvcplui.exe. Updating an exclusion does not prove an already-running process unloaded the mod; observe a fresh process separately.
