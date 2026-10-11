# LeakyVessel

Demo project for Valve's Proton sandbox's Unicode filename corruption bug. Test binaries are built via both GNU's G++ and Microsoft's MSVC, to demonstrate how the Proton bug affects the different C++ runtimes.


## Usage:

- Install [just](https://github.com/casey/just).
- Run `just` in the project directory to see a list of commands, which can be executed with `just <command>`.
  * The `build-*` commands are only necessary if you want to recompile the `dist/` test binaries. The included binaries are from the latest source code. Building manually requires G++ with MINGW (for GNU), and Visual Studio on Windows (for MSVC).
  * The `generate-data` command creates the directory of test-files, but you don't need to run this manually since it's automatically handled by the `test-*` commands.
  * The `test-*` commands execute the test binaries in various environments (Windows, Native Linux, Wine and Proton).
- The two `test-proton-*` commands should be used for debugging while fixing Proton.
- Note: The Wine/Proton test commands take an optional parameter which sets the Wine Debug Logs flag. Passing an empty parameter will clear the flag and enable verbose logs; for example `just test-proton-msvc ""`.


## Test Results:

### Game: Skyrim

This is where I first realized that something was wrong. After a lot of troubleshooting about why the game didn't load and threw errors about being unable to read files, I finally narrowed down the source of the error messages, found its log file, and was shocked to see that almost all filenames are corrupt.

What happened next was a dozen hours of deep debugging and testing as I evolved this test project to reproduce the bug and find its source.

- [Corruption seen in the game's file iteration logs](game_logs/SoundRecordDistributor.log)


### Linux Native

Every filename scans perfectly, but directory iteration order is different than Windows of course.

- [Linux Native Scanner](test_results/scanner_linux_gnu.log)


### Perfect: Wine and Windows

Identical results and file order. Perfectly reads Unicode filenames. The log files are 100% byte-exact identical.

- [GNU Scanner on Windows](test_results/scanner_windows_gnu.log)
- [MSVC Scanner on Windows](test_results/scanner_windows_msvc.log)
- [GNU Scanner on Wine](test_results/scanner_wine_gnu.log)
- [MSVC Scanner on Wine](test_results/scanner_wine_msvc.log)


### Broken: Proton

Proton's sandbox completely mangles and corrupts Unicode filenames, which breaks Windows apps/games and triggers cascading downstream bugs in MSVC's Visual C++ runtime. GNU's runtime is more robust and doesn't fall apart when it encounters the corrupt filenames.

- [GNU Scanner on Proton](test_results/scanner_proton_gnu.log)
- [MSVC Scanner on Proton](test_results/scanner_proton_msvc.log) (**The worst data corruption, which completely breaks apps/games.**)


## Where is the bug?

The filename corruption bug is in Proton's sandboxing layer:

- All Unicode filenames are corrupted and become binary noise. The corrupt binary patterns are the same *every* time, and does not depend on the existence of any other files or the scan order of the files.
- Every application therefore sees corrupt filenames.

Real-world examples of the corruption:

| Original | Corruption |
| -------- | ---------- |
| `[ПрочитайМеня.txt]` | `[P\x1fQ]` |
| `[注释说明.txt]` | `[f3(i\x07\x0ah/4f\x18\x0e.txt]` |

- The Chinese filename's corruption has generated a forward slash (at `h/4f` near the middle), which is a very important detail.
- It's also obvious that the Russian filename has accidentally generated a `\x00` NULL-byte and truncated itself.

This in turn triggers two minor bugs in Microsoft's MSVC runtime, when an application tries to iterate over all files in a directory:

- If a corrupt filename happens to generate a garbage name containing a forward slash (`/`), the MSVC runtime treats the forward slash as a directory separator, and absorbs the "directory" as part of its "directory we are iterating" state when it encounters that file. This is a bug. It obviously should not be doing that, since *every* file *is* a child of the starting directory. But nobody has realized it for the past 40 years, since nobody has filenames containing forward slashes on Windows.
  * After it has absorbed the corrupt "path" into its directory iteration state, all subsequent filenames become corrupt and cannot be opened, since their paths will be completely messed up.
- After a corrupt "forward-slash path" has been absorbed by MSVC, all subsequent file `.exists()` checks will return True even though the paths are totally invalid. This is yet another bug in MSVC. There are most likely other bugs related to this corrupted directory iteration state.
  * Trying to actually open and read the falsely "existing" file will of course fail, and ends up as `READ FAILED (bad stream on open)` in my test app's logs.

The Chinese filename, when corrupted, generates the exact broken "path prefix" that you can see breaking all the MSVC-based applications (seen in this test application's Proton MSVC log and in the game logs).

I have done tests with just the Russian file. MSVC successfully iterates past it, because its corrupted name doesn't contain any forward slashes and therefore doesn't break the internal state of the MSVC directory iterator.

It took me a dozen hours of painful debugging to narrow it down to Proton's sandboxing layer.

- Plain system "wine64" doesn't have any issues.
- Proton's own internal "wine" binary (executed manually) doesn't have any issues.
- Proton with sandboxing is broken.
- If I set the AppID to 0 (Steam's internal "master" compatdata ID), Proton disables its sandbox and successfully runs without issues, which proves that the filesystem sandbox is the issue.
- All tests were performed on a Linux host with a BTRFS filesystem, but the Proton bug is most likely filesystem-agnostic since the filenames come from the Linux kernel regardless of underlying filesystem.


## To Valve:

The bug is very serious. Let's fix it and preferably backport it to all past Proton versions that use sandboxing.

This bug affects any application or game that needs to iterate past Unicode filenames. Those files are very common in various game mods; usually because authors write "readme" files in their own native languages. The bug *breaks* any app/game that tries to iterate past such files.

After it's been fixed, people will be able to play modded games such as Skyrim without filesystem corruption issues.


## To Microsoft:

If anyone has contacts at Microsoft, please let them know about the two minor but nasty bugs in their MSVC runtime. Would be nice to fix those bugs to make Microsoft Visual C++'s runtime as robust as GNU/MINGW's C++ runtime.

They are famously difficult to reach, so I won't try. Help would be appreciated.
