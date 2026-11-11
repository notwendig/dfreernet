# Bundled runtime data

[English](PROVENANCE.md) | [Deutsch](PROVENANCE.de.md)

Imported once on 2026-11-10 from the locally verified schema-33 catalog and resource tables. These are derived reference data, not a newly authored or independently licensed replacement catalog. The GPL-3.0-only grant for original code does not cover these data; existing redistribution restrictions remain. See [license scope](../SCOPE.md).

Source EXE SHA256: d5f8773a64d9259e57f3dfb0d0da93c6e6cfdc2377dc854dc0216fd402fd91ec. Catalog plaintext SHA256: 30bbe3a7edc45b1ec7975544254bbec6853199a899f46144b33eeebe946723cc (4,814,680 bytes). Independent parameter/storage/value fixtures are retained for parity testing.

Runtime data now reside in `dfreernet.sqlite3`. Parameters, texts, metadata, storage maps, editors, menus and resource texts are read from structured tables. `asset_blobs` additionally preserves the original compressed catalog reference, snapshot template with opaque encrypted metadata, and standard Blowfish constants. The template does not synthesize current controller identity. Origin information is also recorded in the `provenance` table.

`SHA256SUMS` pins the database. CMake verifies it and embeds it compressed; no EXE, DLL or encrypted RND is embedded. The optional original developer importer is `scripts/import_standalone_resources.cpp`; it is never invoked by ordinary builds or runtime. Table export and SQLite generation are separate developer tools.
