# kw_plugin_gameplay

Licensed under **GPLv3**, with a game interoperability permission. See
[LICENSING.md](LICENSING.md) for scope, source-distribution obligations and
[third-party notices](THIRD_PARTY_NOTICES.md).

Compatible native gameplay/input preservation.

The release DLL contains only the functional Attack-Move preservation hooks:
selection-removal filtering plus native object destruction/onDie scopes.
It has no input-flow subscriber, selection tracing, profiling queue/writer or
per-command diagnostic callback. `include/kwgameplay/selection_preservation_api.h`
describes the startup configuration entry. Only
`preserve_attack_move_on_loss` is accepted in `kw_gameplay.ini`; remove retired
`selection_command_trace`, `command_flow_tap` and `performance_probe` keys when
upgrading a customized dedicated INI. Unrelated legacy-root keys are ignored.

## Layout and build

Production implementation is under `src/`; supported/public or compatibility
headers are under `include/`. Generated files go only to ignored `build/`:
`build/bin` for DLLs/executables, `build/lib` for libraries, `build/obj` for
objects/dependency files, `build/config` for copied configuration templates.
Use GNU Make and x86 MinGW (`i686-w64-mingw32-gcc/g++/ar`).

```sh
make -j4
make sdk-paths
```

For relocated checkouts set `KWMOD_SDK_ROOT=/path/to/kw_modloader`.
It is the only mandatory SDK dependency: generic loader contracts and KW engine support
are both in its `include/` tree. Environment variables and Make arguments work.
`KWMOD_BIN_DIR` and `KWMOD_LIB_DIR` select matching SDK build artifacts;
`sdk-build` builds missing/stale SDK artifacts before linking. Compiler overrides
are `WINDOWS_CC`, `WINDOWS_CXX`, `WINDOWS_AR`; `BUILD_DIR` selects plugin output.

Run `make bundle` (or `make stage`) to assemble `build/bundle/` with this
plugin's DLLs, configuration and manifest in their engine-root-relative layout.
Override `BUNDLE_DIR=/path/to/staging` if needed. This is a plugin overlay,
not a standalone loader installation. Developer variants are not included.
Bundle generation replaces only previously staged, unmodified files and
refuses an unowned or locally edited output directory. Do not point it at an
installed game. See `kw_modloader/docs/BUNDLES.md` for composition and AI caveats.

Build and staging targets do not install into a game or overwrite user settings.
Use matching loader/SDK artifacts and preserve customized INIs when installing.
Runtime loading uses `.kwplugin` manifests; configuration uses `.ini` files.

Focused offline checks use `make test-config test-preservation`; the latter
needs a 32-bit C toolchain and `qemu-i386` for native x86 calling conventions.
They execute the release hook bodies with bounded memory/call fixtures, not a
diagnostic or historical implementation. No game or Wine session is started.
