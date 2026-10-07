# Ukuvota Repository Rules

- Work in this canonical checkout on master. Never use source worktrees or
  temporary checkouts.
- Ukuvota's maintained app and host code uses Ziran and current Kryon package
  APIs. Android's C and Java files are platform glue only.
- Dependencies belong in `ziran.toml` and the generated `ziran.lock`. Use
  ignored `ziran.local.toml` overrides for canonical local repositories.
- Never edit a dependency under `vendor/` or in Ziran's package cache. Change
  its owning repository, test and commit there first, then update the lock.
- Do not add compatibility aliases or wrappers for retired Kryon APIs.

## Text and source checks

- Use `Text(session, TextProps)` with bounds, wrapping, clipping, color,
  alignment and disabled state in `TextProps`.
- Do not add positional text APIs, parallel text widgets or local runtime shims.
- Keep `make clean-text-api-check` and `make test` passing. Verify release
  builds with `ZIRAN_LOCKED=1` so local overrides cannot hide missing exports.

## Readability and data

- Write conventional, readable code with descriptive names and explicit error
  handling. Format changed source and inspect `git diff --check` before commit.
- Database upgrades must preserve decisions, ballots and identities. Refuse an
  unsupported schema without dropping it or silently replacing its keys.
- Keep UI claims consistent with available behavior. Local decisions must not
  advertise online sharing or server-enforced permissions.
- Translate changed content in every touched locale and regenerate catalogs.

## Visual verification

- All tests use a private Xvfb display and an isolated data directory. Scrub
  DISPLAY, WAYLAND_DISPLAY, XAUTHORITY and DBUS_SESSION_BUS_ADDRESS before
  starting it. Capture only windows created by that test.
