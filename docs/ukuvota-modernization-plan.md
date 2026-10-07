# Ukuvota architecture and remaining work

Ukuvota has one maintained Ziran application. `app.zi` supplies `Frame`,
`home_view.zi` lists local processes, `create_view.zi` creates them, and
`collect_view.zi` collects proposals, ballots and results. UI code imports the
current `kryon/Widgets` surface. The desktop host uses SDL/Cairo, the browser
host uses Kryon's Canvas backend, and Android uses Kryon's Raylib host with a
small keyboard/data-directory bridge.

`storage.zi` owns SQLite transactions, process history and local participant
identities. Existing database upgrades preserve records and key material;
unknown pre-release schemas are refused. `tally.zi`, `ballot.zi`, `process.zi`
and `phase.zi` supply counting, bounded ballot parsing, process data and timing.
Core tests run both generated native code and portable Ziran bundles where
there is no foreign-library boundary.

Ziran packages replace the retired vendor submodule. `ziran.toml` declares
Kryon, KSS, SQLite and Daochi Client's reusable keys; `ziran.lock` pins their
sources and the compiler. Platform scripts resolve dependency locations with
`ziran pkg path` and generate C into ignored build directories. No handwritten
C UI, legacy `.kry` fragments, old QR/sync Java bridge or alternate web server
is maintained.

The current product supports a group sharing one device. Separate participant
identities keep changing a label from overwriting another ballot. Scores can
be left unset to abstain; completely empty ballots are refused. Unscored
proposals cannot become a winner, and equal scored leaders have no selected
winner. Status quo and Repeat process have stable stored keys.

The browser mounts SQLite storage on IndexedDB before starting the app,
flushes committed changes, reports failures and offers a SQLite download.
Tests and screenshots use private displays and disposable databases.

Online multi-device participation remains a separate feature. Before exposing
it, the server needs process/proposal/ballot routes, proof of identity,
permission enforcement, durable conflict handling and end-to-end tests. The
local app does not advertise those capabilities or present unenforced sharing
controls. Do not restore old compatibility clients to provide them.

Android builds still need device testing for keyboard, lifecycle, insets and
accessibility. Signed distribution and hosted website publication are separate
from passing local build checks. Visual assets should be refreshed from the
maintained interface when preparing a public release.
