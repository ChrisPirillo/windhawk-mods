# Continue Win32 UI Modernizer

This folder is a compact, model-independent handoff: current source, useful test sources, status and a next-chat prompt. It contains no credentials, old DLLs, screenshots or session transcript.

## Source of truth

- Source branch: https://github.com/ChrisPirillo/windhawk-mods/tree/codex/consistent-native-ui
- Draft for original-author review: https://github.com/crazyboyybs/windhawk-mods/pull/2
- Snapshot: commit `0a6e775e8e98570b055ee65d573d9dd9a296033f`; proposed version 1.0.4.3, based on upstream 1.0.4.
- Original author: crazyboyybs; GPL-3.0. The PR is a contribution to that author, not an official release.

Read STATE.json for confirmed results and unfinished work. Check the live source branch/PR before editing: the kit is a dated snapshot and may become outdated.

## A new chat or computer

1. Give the new chat this starting link: https://github.com/ChrisPirillo/windhawk-mods/blob/codex/continuation-kit/START-HERE.md
2. Ask it to read this page, STATE.json, TESTING.md and AGENTS.md. NEXT-CHAT.txt provides an optional ready-made prompt; the old conversation is not needed.
3. For code changes, clone the branch into a working checkout:

   `git clone --depth 1 --single-branch --branch codex/consistent-native-ui https://github.com/ChrisPirillo/windhawk-mods.git`

4. Keep this kit beside that checkout. Do not add all these test/handoff files to the author's PR: its intended change is one mod source file.
5. Any computer can inspect the source/status. Live Windows acceptance needs a Windows desktop and the locally installed Windhawk/toolchain. Compilation on another OS is not Windows acceptance.

GitHub stores the source and review. It does not run or automatically update the locally installed Windhawk mod. GitHub sign-in, administrator rights and machine settings must be established on each new machine; none are included here.

## First check

With Python 3.9 or newer, from a checkout of this kit branch:

`python tools/project.py verify`

This checks kit integrity without building, installing or launching anything. TESTING.md explains the reusable checks. Generated files go under build/ and are not tracked in this branch.

## Current priorities

Keep MMC and NVIDIA Control Panel excluded while their compatibility is unresolved. Work next on file-picker Preview-pane flicker and targeted app compatibility, without broad paint changes that risk the working Properties/font behavior. Do not describe open issues as fixed.
