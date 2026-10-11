help:
    @just -l

# build scanner_msvc.exe
build-msvc:
    @echo "Info: Unfortunately, you must use Microsoft Visual Studio 2022 Community Edition on Windows to build the MSVC binary (with the included 'src/Scanner.sln' solution). Remember to build in Release mode."
    @exit 1

# build scanner_gnu.exe
build-gnu:
    make scanner_gnu

# build scanner_linux
build-linux:
    make scanner_linux

# generate test-data files
generate-data:
    @echo "Generating test-data directory..."
    mkdir -p test_results
    (cd test_results && python3 ../utils/2_create_files.py)

_run-test-wine exe winedbg="-all":
    {{ if path_exists("test_results/Data") == "false" { "just generate-data" } else { "" } }}
    @echo "Testing Wine with {{ exe }}. Env: WINEDEBUG={{ winedbg }}"
    (here="${PWD}"; cd test_results && WINEDEBUG={{ winedbg }} wine64 "${here}/dist/{{ exe }}")

# wine scanner_msvc.exe
test-wine-msvc winedbg="-all":
    @just _run-test-wine "scanner_msvc.exe" "{{ winedbg }}"

# wine scanner_gnu.exe
test-wine-gnu winedbg="-all":
    @just _run-test-wine "scanner_gnu.exe" "{{ winedbg }}"

_run-test-proton exe winedbg="-all":
    {{ if path_exists("test_results/Data") == "false" { "just generate-data" } else { "" } }}
    @echo "Testing Proton with {{ exe }}. Env: WINEDEBUG={{ winedbg }}"
    # NOTE: We run with ID 480 (Spacewars) since we need a valid, non-0 game ID that everyone owns.
    here="${PWD}" \
    __steam_path="${HOME}/.local/share/Steam" \
    __proton_path="${__steam_path}/steamapps/common/Proton - Experimental" \
    __steam_lr_path="${__steam_path}/steamapps/common/SteamLinuxRuntime_4" \
    __game_id=480; \
    (cd test_results; \
        WINEDEBUG={{ winedbg }} \
        STEAM_COMPAT_DATA_PATH="${__steam_path}/steamapps/compatdata/0" \
        STEAM_COMPAT_CLIENT_INSTALL_PATH="${__steam_path}" \
        STEAM_COMPAT_INSTALL_PATH="${here}/test_results" \
        SteamAppId=${__game_id} SteamGameId=${__game_id} SteamOverlayGameId=${__game_id} STEAM_COMPAT_APP_ID=${__game_id} \
        STEAM_COMPAT_MOUNTS="${here}" \
        AMM_MAKO_LAUNCH=0 \
        STEAM_COMPAT_TOOL_PATHS="${__proton_path}:${__steam_lr_path}" \
        "${__steam_lr_path}/_v2-entry-point" --verb=waitforexitandrun -- \
        python3 "${__proton_path}/proton" waitforexitandrun "${here}/dist/{{ exe }}")

# proton scanner_msvc.exe
test-proton-msvc winedbg="-all":
    @just _run-test-proton "scanner_msvc.exe" "{{ winedbg }}"

# proton scanner_gnu.exe
test-proton-gnu winedbg="-all":
    @just _run-test-proton "scanner_gnu.exe" "{{ winedbg }}"

# test scanner_linux
test-linux:
    {{ if path_exists("test_results/Data") == "false" { "just generate-data" } else { "" } }}
    @echo "Testing native scanner_linux binary."
    (here="${PWD}"; cd test_results && "${here}/dist/scanner_linux")

# test on windows
test-windows:
    @echo "Info: Start Windows and run the GNU and MSVC scanners manually, to generate the Windows test logs. You can do this by copying the two 'dist/' executables and the 'test_results/Data/' directory to Windows, and placing the 'Data/' directory next to the executables."
    @exit 1
