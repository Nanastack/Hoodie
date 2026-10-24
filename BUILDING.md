# Building Hoodie

This page is for developers. Players can install the compiled player ZIP using the README.

## Requirements

- Windows and MSVC with C++20 and x86 tools.
- Windows SDK.
- Ashita v4 plugin SDK, supplied separately. Point `ASHITA_SDK` at the directory containing `Ashita.h`.

Microsoft Detours source and its original license are included in `third_party/Microsoft.Detours/`.

From an **x86 Visual Studio Developer PowerShell**:

```powershell
$env:ASHITA_SDK = 'C:\Ashita\plugins\sdk'
.\build.ps1
```

The build produces `package/plugins/hoodie.dll` and runs the native tests. `-SkipTests` builds only the DLL. `-ToolchainRoot` optionally selects a standalone MSVC/Windows SDK layout instead of using a Developer PowerShell.

## Layout

- `src/`: DLL implementation, headers and native tests.
- `package/plugins/`: compiled DLL.
- `package/resources/`: approved armor models and hood-change animation data.
- `third_party/`: Microsoft Detours source and license.
- `licenses/`: dependency notices.
- `release-manifest.json` and `SHA256SUMS.json`: shipped asset inventory and hashes.

The production build does not define `HOODIE_TESTING`. That flag is only used by the native integration fixture to exercise callbacks, logs and failure paths. The race/armor swap implementation has been removed.

All five supported body appearance models (3, 47, 10, 11 and 46) route by equipped appearance and race. Tarutaru male/female use shared compatible assets. The displayed armor list in the README is intentionally restricted to names from the original game through ToAU; it is not a change to model routing.

The 35 fitted armor/race pairs were visually approved in game. The cleaned release DLL passed the native regression suite; see `VERIFICATION.md` for the test scope. The release package does not include the Ashita SDK, editor saves, local export paths, personal settings or game logs.
