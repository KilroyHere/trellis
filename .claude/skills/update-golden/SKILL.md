---
name: update-golden
description: Regenerate golden compiler-diagnostic files after an intended change to error messages. Only when the owner asks for it.
disable-model-invocation: true
argument-hint: "<preset> <reason>"
---

Arguments: $ARGUMENTS (a preset name, then the reason for the change).

1. Run `ctest --preset <preset> -L golden` and list the goldens that differ.
2. Show the owner the normalized diff, golden by golden, along with the reason.
3. Only after the owner agrees, run `python3 -I tools/golden.py --update --preset <preset> --reason "<reason>"`. It rewrites the files and appends the reason to `docs/golden-changes.md`.
4. Run the golden tests again on every compiler they cover.
5. Commit the goldens on their own, with the reason in the commit message.
