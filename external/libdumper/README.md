# dumper-testing

Rewriting PS4 application dumper from scratch in C++. This repo will be deleted when a permanent repo is setup. Don't bother cloning unless you're going to be making immediate changes.

**No code here is finished or guaranteed to work.** Goal is for readability first to allow easier collaboration until everything is working 100%, don't want to optimize it too early:

1. Get it all working. For the first draft we will assume the asset is completely installed and is currently running on the system.
2. Refactor to make more sense.
3. More robust. Find/fix edge cases and have good, descriptive, error messages for issues that cannot be handled automatically.
4. Setup tests so we don't break anything trying to enhance preformance.
5. Performance.

> "Move fast break things"

**ANYONE** can contribute. Code is currently licensed under GPLv3, by submitting a pull request you agree to this term and agree to possible relicensing later.

## Approximate order of completion
- [X] PKG
- [X] NpBind
- [X] SFO
- [X] PFS
- [X] Decrypt SELF
- [ ] FSELF
- [ ] RIF (For Additional Content w/o Data & Entitlment Keys)
- [ ] Dump
    - [X] Base
    - [X] Patch
    - [X] Additional Content w/ Data
    - [X] Theme
    - [X] Retail Theme + Theme "Unlock"
    - [X] Remaster (Build a script to diff the actual files with the original content PC side to make a patch PKG vs a Remaster?)
    - [ ] Additional Content w/o Data
    - [ ] Multi-Disc (Does it just work without changes?)
- [ ] GP4
  - [X] Basic GP4 Generation
  - [ ] PlayGo Related Issues
  - [ ] PFS Compress Option
