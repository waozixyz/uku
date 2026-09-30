# Uku Repository Rules

- Never edit files under `vendor/` from this repository. Make Kryon changes in
  the core Kryon repository, commit and push them there, then update Uku's
  Kryon submodule pointer.
- Do not add compatibility aliases or local runtime shims for removed Kryon
  APIs. Migrate maintained source to the current canonical API.

## Canonical Text Rule

- `Text` has exactly one supported form: `Text((TextProps){...})`. Put bounds,
  wrapping, clipping, color, alignment, and disabled state in `TextProps`.
- Do not add positional `Text` calls, parallel helpers such as `TextWrapped` or
  `TextColored`, or local wrappers that conceal the old signature.
- Keep `make clean-text-api-check` passing. A UI migration is incomplete until
  the maintained source builds with upstream Kryon.

## Readability Rule

- Write conventional, fully readable code. Do not compress multiple statements,
  branches, declarations, or error checks onto one line. Use descriptive names,
  explicit control flow, and focused helpers.
- Format changed source, run `git diff --check`, and inspect the final diff before
  considering a change complete.

## Bend

When using Bend:
- run `bend guide` to learn it
- use `LAWS.bend` to keep important rules
- run `bend PROOF.bend` before committing
- parallelize the code whenever possible
