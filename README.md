# Win32 UI Modernizer continuation kit

Give a new chat this link:

**[Start here](https://github.com/ChrisPirillo/windhawk-mods/blob/codex/continuation-kit/START-HERE.md)**

Ask it to read the starting page and continue with the issue you describe. This works independently of the old conversation or model. A chat needs web access or a checkout to read the files.

## What is here

- [Current status and known gaps](STATE.json)
- [Reusable test instructions](TESTING.md), 32 verified component cases and native fixture sources
- [Project instructions](AGENTS.md) and an optional [next-chat prompt](NEXT-CHAT.txt)
- A frozen [source snapshot](source/win32-ui-modernizer.wh.cpp) and file-integrity manifest

## Where development happens

The active source is on [codex/consistent-native-ui](https://github.com/ChrisPirillo/windhawk-mods/tree/codex/consistent-native-ui). The [draft for the original author](https://github.com/crazyboyybs/windhawk-mods/pull/2) remains a single mod-source change. This separate kit branch is not part of that PR and is not a release.

Before changing code, check that source branch and the PR for newer work. Update this kit's source snapshot/status/tests separately when the baseline changes. Do not merge the kit files into the author's PR.

To get the kit on another computer:

```text
git clone --depth 1 --single-branch --branch codex/continuation-kit https://github.com/ChrisPirillo/windhawk-mods.git modernizer-continuation
cd modernizer-continuation
python tools/project.py verify
```

Reading and integrity checking work across computers. Running the native tests requires Windows; installation and administrator access are local to that computer. This branch includes no credentials, installed DLLs or automatic installer.

Original mod by crazyboyybs; GPL-3.0. See [LICENSE](LICENSE).
