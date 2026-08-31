# Ukuvota Modernization Plan

Date: 2026-08-27

## Scope

Rebuild Uku toward the original Ukuvota experience shown in the saved reference screenshots, while keeping the current native Kryon implementation. Do not add compatibility layers for old Kryon APIs. Shared runtime/editor/theme primitives go into the real Kryon repo first, then Uku consumes them by moving the `vendor/kryon` submodule pointer.

## References Saved In This Repo

The screenshots from the request are saved under `docs/reference/`:

- `ukuvota-ref-01-dashboard-empty.png`: empty dashboard with active/completed sections.
- `ukuvota-ref-02-setup-process.png`: process setup, topic field, rich description editor, negative weighting selector.
- `ukuvota-ref-03-schedule.png`: proposal/voting time selection.
- `ukuvota-ref-04-review.png`: process creation review.
- `ukuvota-ref-05-proposal-collection.png`: live proposal phase with share link, QR action, add proposal/template actions.
- `ukuvota-ref-06-wysiwyg-editor.png`: rich proposal editor modal.
- `ukuvota-ref-07-template-review.png`: review screen with default proposal templates.
- `ukuvota-ref-08-emoji-voting.png`: anonymous/alias voting with seven emoji score choices.
- `ukuvota-ref-09-dashboard-active.png`: dashboard with active process cards and completed section.

Legacy source reference: https://gitlab.com/yunity/ukuvota

## Current Baseline

- Uku now points at Kryon `584bf159 Fix native linux raylib backend rename`.
- Kryon API usage in Uku has been migrated away from removed `UI*` compatibility names to current public names.
- Default theme style now clamps to `THEME_STYLE_MATERIAL`.
- Native x86_64 verification passes with `make linux-x86_64`.
- Full `make linux` still needs a configured aarch64 cross toolchain.
- Current app is a mostly single-file C UI in `src/main.c` with two `.kry` helpers.
- Current local persistence uses SQLite tables for `processes`, `options`, `proposals`, `votes`, `results`, `account`, and `settings`.
- Current workflow supports consent, ranked, and collection process types, remote sync calls, QR/share link, process history, theme settings, and account setup.

## Legacy Ukuvota Features Found

From the legacy Quasar/PouchDB app:

- Routes: home, manual, create, collect, vote, results, not found.
- Topic document fields: question, description, proposal end time, voting end time, voting interval, negative score weighting, proposals, votes, emojis.
- Creation always adds default proposals: `Status quo` and `Repeat process`.
- Proposal phase allows inline proposal creation/editing.
- Process layout auto-redirects between collect, vote, and results based on timers.
- Voting uses seven score values: `-3, -2, -1, 0, 1, 2, 3`, rendered as emoji SVG faces.
- Voters submit a name or persistent alias; no account is required in the legacy prototype.
- Duplicate voter names are rejected.
- Results can be filtered by selected voters.
- Results include ranked emoji/list view, raw data table, result display settings, image export, text export, and Markdown export.
- Share tools include copyable URL, QR modal, and manual notification helper.
- Manual/reference content explains the process and negative score weighting in English and German.

## Main Gaps To Close

- **Visual parity:** current screens are functional but do not match the supplied dark Material-style Ukuvota layout tightly enough.
- **Creation flow:** current create screen is one dense form; target is a three-step flow: setup, schedule, review.
- **Rich text:** current topic/proposal descriptions are plain text areas; target needs a reusable Kryon WYSIWYG/Markdown editor widget.
- **Proposal templates:** current default proposals are loaded internally; target needs visible template actions and review-stage template cards.
- **Anonymous/alias participation:** proposal/vote actions can use a throwaway guest identity, but process creation is still blocked before local save when no account is loaded. Target allows unlogged users to start, share, propose, and vote with a process-scoped identity, with optional account-backed identity.
- **Daochi naming cleanup:** current client internals still use old `lyra_*` function names and Ksync/Inbe wire names. Target uses Daochi or domain-neutral names internally and exposes Daochi public aliases while continuing to accept every shipped legacy API/header/key name.
- **Voting UI:** current face picker exists, but target needs large per-proposal emoji rows, clear selected state, and anonymous submit layout.
- **Dashboard grouping:** current home filters active processes but needs the exact active/completed section treatment from references.
- **Results parity:** legacy voter filtering, data table, Markdown/text/image export, and result display settings need to be restored in native UI.
- **Manual/reference:** current manual content is thinner than legacy content and should absorb the original summary/negative-weighting explanations.

## Implementation Plan

### Phase 1: Stabilize Kryon Upgrade

1. Keep all Kryon build/runtime fixes in `/mnt/storage/Projects/kryon` on `master`.
2. Commit each Kryon fix before updating `uku/vendor/kryon`.
3. Keep `uku/vendor/kryon` pristine after every checkout: `git status --short` inside the submodule must be empty.
4. Run `make linux-x86_64` after every Uku-side migration.
5. Add a CI/build note or Makefile target distinction so `make linux-x86_64` is the local verification target when aarch64 cross variables are absent.

### Phase 2: Material Theme Adoption

1. Use Kryon `THEME_STYLE_MATERIAL` as the default style for new installs.
2. Audit all custom rounded rectangles, borders, and colors in `src/main.c`.
3. Replace hardcoded surface/button/text choices with `GetUIMaterialScheme()` and style tokens where possible.
4. Normalize controls: 8px-or-less card radius, consistent input height, Material text fields, icon buttons for search/copy/QR/settings/history.
5. Keep the dark brown/rose look from references as a Material palette, not one-off per-widget colors.

### Phase 3: Shared Kryon WYSIWYG Widget

Implement in Kryon first:

1. Add a reusable `RichTextEditor` public widget with title field, multiline body, toolbar, selection tracking, and Markdown storage.
2. Toolbar commands: bold, italic, underline, heading/paragraph, foreground color, background/highlight color, strikethrough, align left/center/right, numbered list, bullet list, code, quote, and link.
3. Editing model stores Markdown-compatible text, with formatting commands applying structured Markdown spans/blocks.
4. Add a preview/render path using Kryon markdown rendering.
5. Add keyboard behavior: text selection, Ctrl+B/I/U, Enter, Backspace, paste, and cursor persistence.
6. Add tests in Kryon for command application and selection boundaries.
7. After commit, bump Uku's submodule pointer and replace plain description fields with the widget.

### Phase 4: Creation Wizard

1. Split creation state into setup, schedule, and review steps.
2. Setup screen matches `ukuvota-ref-02`: topic question, rich description editor, negative score weighting dropdown, process mode buttons.
3. Schedule screen matches `ukuvota-ref-03`: timezone selector, proposal/voting start and end fields, duration sliders.
4. Review screen matches `ukuvota-ref-04` and `ukuvota-ref-07`: topic, rendered description, negative weighting, phase times, timezone, default proposal/template list.
5. Persist all wizard state in `UkuDecision`, adding fields for timezone and explicit timestamps if needed.
6. Keep validation strict: non-empty topic, valid durations, at least two options where process type requires options.
7. Do not require a durable account before entering review or submitting the process; creation identity is resolved by the anonymous ownership phase below.

### Phase 5: Anonymous Process Ownership

Use the existing signed guest identity model for process creation, not a local-only fake owner:

1. Generate the process id before creating the owner identity so the guest key can be stored under the process id.
2. Add a single creation identity helper that returns the loaded account when present, otherwise creates/loads a process-scoped guest keypair.
3. Save local process rows with the resolved identity as owner, whether the identity is a durable account or a guest identity.
4. Build process-create JSON and authorization headers from the resolved request identity, not directly from `app->account`.
5. Run remote process creation with `guest_active` only for the duration of the request, then persist the guest auth token back to the per-process guest settings.
6. Keep the UX anonymous: do not show a forced account modal; show optional account setup only as an explicit user action.
7. Treat anonymous as pseudonymous per-process ownership: the process can be managed from the device/browser that holds the guest key, but losing that key loses owner controls.
8. Use the same identity helper for default proposals inserted during process creation so default proposal authorship matches the process owner.
9. Update owner checks for edit/delete/export controls to allow either the durable account id or the matching process guest id.
10. Add tests/smoke checks for anonymous web creation, anonymous native creation, synced guest-owned creation, reload-with-guest-key management, and logged-in creation.

### Phase 6: Proposal Collection And Templates

1. Redesign proposal phase to match `ukuvota-ref-05`.
2. Add shareable link row with copy and QR icon actions.
3. Add `Add Proposal` rich-editor modal matching `ukuvota-ref-06`.
4. Add `Add proposal by template` action.
5. Store proposal templates in app code first: Status quo, Repeat process, plus a small extensible table for future templates.
6. Prevent duplicate default templates per process.
7. Allow proposal authorship to be anonymous alias, account ID, or empty depending on participation mode.

### Phase 7: Anonymous Voting

1. Change vote identity model from required `voter_user_id` to `voter_key + display_name`.
2. For logged-in users, `voter_key` remains account public ID.
3. For anonymous users, reuse or derive a stable process-scoped guest key from local settings, while still asking for display name/persistent alias.
4. Keep duplicate visible names blocked within a process unless the same local voter key is updating its vote.
5. Update local SQLite schema with migration, not destructive reset.
6. Continue using signed bearer-authenticated requests; the anonymous path logs in with the guest key instead of requiring a durable account.
7. Redesign voting screen to match `ukuvota-ref-08`: large proposal cards, seven emoji scores, clear selected ring, voters count, alias field, submit/update button.

### Phase 8: Daochi Naming And Compatibility

Rename new code away from Lyra/Ksync/Inbe terminology without breaking old clients:

1. In Uku internals, rename `lyra_*` helpers to domain names such as `http_request`, `login`, `create_remote_process`, `fetch_process_detail`, `submit_proposal`, and `submit_vote`.
2. In Uku internals, keep `account` for user-facing durable identity, but rename conversion wrappers so Ksync is treated as the legacy key-file/protocol format, not the app's domain model.
3. In Daochi server internals, migrate `ksync*` helper names toward `daochi*`, `account*`, `identity*`, `record*`, or `sync*` depending on the actual concept.
4. Add Daochi wire aliases before changing clients: `X-Daochi-User`, `X-Daochi-Signature`, `X-Daochi-Client`, `X-Daochi-Since-Version`, `X-Daochi-Limit`, and `X-Daochi-Admin`.
5. Keep accepting `X-Ksync-*` and `X-Inbe-*` headers, `user_id_hash` JSON fields, `/api/v1/sync/challenge`, `/api/v1/sync/login`, account key headers, and exported legacy key formats.
6. Add new request/response field aliases where useful, preferring `account_id` in new docs while still accepting and returning `user_id_hash` for old clients until a major API cleanup.
7. Keep existing signature contexts valid: `ksync-sync-v1` and `inbe-sync-v1` continue to verify. Add a Daochi context only as an additive option with explicit server tests.
8. Update docs and UI copy to say Daochi for the service/protocol and avoid exposing old implementation names unless documenting compatibility.
9. Land compatibility tests before client migration: old headers still work, new Daochi headers work, mixed body/header users are rejected, old key imports still work, and old WebSocket subprotocols still work.
10. Do not rename database columns in the same change unless a tested migration and rollback strategy are present; public JSON and DB churn are separate risks.

### Phase 9: Results And Legacy Exports

1. Restore voter inclusion/exclusion controls from legacy results.
2. Add ranked result view with average and total score display.
3. Add table view: voters as rows, proposals/options as columns, average row, total row.
4. Add result settings for average/total visibility.
5. Add copy/export as text and Markdown.
6. Add image export if Kryon has or gets a stable render-to-image path.
7. Ensure negative weighting math matches legacy behavior: negative scores multiply by the configured weighting before totals/averages.

### Phase 10: Dashboard And Navigation

1. Match `ukuvota-ref-01` and `ukuvota-ref-09`: top bar, active section, completed section, empty panels, centered new-process CTA.
2. Active cards show topic, current phase, countdown/end time, and phase color.
3. Completed cards show result summary and completion date.
4. Keep direct deep links opening process detail.
5. Add search/join behavior to the top bar without crowding the dashboard.

### Phase 11: Manual, Localization, And Copy

1. Port legacy `summary.en.md`, `negativeScoreWeighting.en.md`, and German equivalents into native content.
2. Update localized strings for wizard steps, templates, anonymous voting, results tabs, and export actions.
3. Keep user-facing copy focused on behavior and process meaning.
4. Avoid exposing implementation details like SQLite, submodules, or sync mechanics in UI copy.

### Phase 12: Verification

1. Build native: `make linux-x86_64`.
2. Build web if Emscripten is available: `make web`.
3. Smoke test these paths: empty dashboard, create wizard, anonymous process creation, guest-owned reload/manage, proposal phase, template proposal, anonymous vote, logged-in vote, duplicate alias rejection, results, export, completed dashboard.
4. Screenshot compare key screens against the nine saved references at desktop and mobile-ish view sizes.
5. Verify `git -C vendor/kryon status --short` is empty before any Uku commit.

## Commit Boundaries

Recommended commit sequence:

1. Kryon: build-system/API fixes needed by the submodule bump.
2. Uku: submodule bump plus direct API rename migration.
3. Kryon: Material/rich editor primitives.
4. Uku: creation wizard and theme restyle.
5. Uku: proposal templates and collection UI.
6. Uku/server: anonymous process ownership and vote support.
7. Daochi/Uku: compatibility-safe Daochi naming migration.
8. Uku: voting UI and results/export parity.
9. Uku: dashboard polish, manual/localization, final verification.
