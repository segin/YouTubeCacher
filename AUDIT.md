# YouTubeCacher Code Audit: Remediation Checklist

- **Baseline:** `main` at `8b425d3` (2026-09-29)
- **Scope:** all C sources, headers, resources and build files
- **Format:** each item is one requirement, written with an EARS pattern. Items follow INCOSE rules for well-formed requirements: one `shall` each, a unique ID, verifiable, and stating the needed behaviour rather than a fix.
- **Remediation status:** all 49 items are fixed on `main` (see each item's Resolved line). Every fix builds warning-free with clang for x86_64, i686 and aarch64, debug and release, and passes the unit tests; 20 are also covered by a unit test or a build run. None has been exercised in the running Windows application yet, so each item's T and D verification is still to do on Windows.
- **Verification status:** every open item has been checked against the code at least twice: once by the original review, and again by an independent adversarial check of each sub-claim. No item was refuted outright. Fifteen had sub-claims corrected, and those items are rewritten below. One new finding was added (FUN-017).

## How to read an item

```
- [ ] **ID**: requirement (EARS)
  - Rationale: the defect this requirement closes, and its failure scenario
  - Location: where the defect is today (file:line at the baseline)
  - Verification: T = Test, I = Inspection, A = Analysis, D = Demonstration
  - Confidence: see the table below
```

| Confidence | Meaning |
|---|---|
| Confirmed | Checked by hand against the code; build items were also run |
| Verified | An independent second check upheld every sub-claim |
| Corrected | The second check found part of the original wrong; the text below is the corrected version |

**EARS patterns used:**

| Pattern | Form |
|---|---|
| Ubiquitous | The YouTubeCacher application shall … |
| Event-driven | **When** \<trigger\>, the YouTubeCacher application shall … |
| State-driven | **While** \<state\>, the YouTubeCacher application shall … |
| Unwanted behaviour | **If** \<condition\>, **then** the YouTubeCacher application shall … |
| Optional feature | **Where** \<feature\>, the YouTubeCacher application shall … |

**ID prefixes:**

| Prefix | Area |
|---|---|
| SEC | Security |
| MEM | Memory and handle safety |
| CON | Concurrency |
| FUN | Functional correctness |
| RES | Resource leaks |
| BLD | Build |

Items are ordered by severity; within a severity, the most likely to be hit comes first.

---

## Critical and high

- [x] **CON-002**: When the main window is closed, the YouTubeCacher application shall terminate within 2 s.
  - Rationale: **every exit deadlocks.**
    1. `CleanupApplicationState` takes `stateLock` (`appstate.c:113`) and calls `CleanupCacheManager` (`:138`), which wakes the save thread and waits up to 5 s for it (`cache.c:141`).
    2. The save thread logs its exit (`cache.c:566`). `ThreadSafeDebugOutput` takes `g_debugOutputLock` (`threadsafe.c:163`), then `DebugOutput` → `GetDebugState` (`log.c:180`) blocks on `stateLock` (`appstate.c:269`).
    3. When the wait times out, the main thread logs (`cache.c:151`) and blocks on `g_debugOutputLock`.

    Each thread now holds the lock the other needs. The save thread always exists (`cache.c:119`), and the close path reaches this before it hides the window or calls `ExitProcess` (`ui.c:2704-2723`, `:2994`, `:3013-3020`).
  - Location: `appstate.c:113`, `:138`, `:269`; `cache.c:141`, `:151`, `:566`; `threadsafe.c:163`; `log.c:180`
  - Verification: T (close the main window; the process exits within 2 s)
  - Confidence: Confirmed
  - Resolved: `92f35ed`; checked by build and inspection

- [x] **MEM-001**: When a subprocess handle is closed, the YouTubeCacher application shall set every stored copy of that handle to `NULL` before any later cleanup can run.
  - Rationale: `hOutputWrite` and `hProcess` are closed, but the context fields are not cleared. `FreeSubprocessContext` then passes a `DuplicateHandle` check on whatever object has since reused the handle value, and closes it. This happens after every successful download.
  - Location: `parser.c:1121`, `:1148`, `:1158`, `:1403`, with the second close at `ytdlp.c:2321-2347`
  - Verification: I, and T with handle tracing under Application Verifier (Handles check)
  - Confidence: Confirmed
  - Resolved: `21ec3dd`; checked by build and inspection

- [x] **MEM-002**: When the thread-safe subprocess context is cleaned up, the YouTubeCacher application shall terminate the child process and join the output-reader thread before it marks the context uninitialized or frees it.
  - Rationale: `CleanupThreadSafeSubprocessContext` sets `initialized = FALSE` first. Cancel, Wait and ForceKill then return at once, so yt-dlp is never killed. The detached reader thread then uses the freed context and its deleted locks. The 5-minute worker timeout triggers this; `subproc.c:86-96` has a smaller window of the same bug.
  - Location: `threadsafe.c:293`, early returns at `:751`, `:789`, `:939`, reader handle closed at `:1150`, timeout at `:1333-1337`
  - Verification: T (let a download exceed the timeout; the child has exited and there are no access violations)
  - Confidence: Confirmed
  - Resolved: `fbaeef6`; checked by unit test (`test_threadsafe`)

- [x] **MEM-003**: When the YouTubeCacher application appends a video's delete-error details to the combined error text, it shall bound every write by the allocated size of the combined buffer.
  - Rationale: the buffer grows by `errorLen + 100` characters, but the ~71-character header, the `Video: <title>` line and two unbounded `wcscat` calls write more than that. Deleting a video whose file is locked and whose title is longer than about 19 characters overflows the heap.
  - Location: `ui.c:2445-2469`
  - Verification: T (multi-select delete of locked files with 200-character titles, under page heap)
  - Confidence: Confirmed
  - Resolved: `73bef8a`; checked by unit test (`test_delete_errors`, also under ASan/UBSan)

- [x] **SEC-001**: The YouTubeCacher application shall accept as the yt-dlp executable only a file type that `CreateProcessW` runs without a command interpreter.
  - Rationale: `.exe`, `.cmd`, `.bat`, `.py` and `.ps1` are accepted, both at validation and in the Settings browse filter. The path is re-checked before each download, and `.bat` still passes. A `.bat` or `.cmd` is passed straight to `CreateProcessW(NULL, cmdLine, …)`, so `cmd.exe` parses the command line. `EscapeCommandLineArgument` turns `https://www.youtube.com/watch?v=a"&calc&"` into `"…v=a\"&calc&\""`. `cmd.exe` doesn't treat `\` as an escape, so the quote closes and `&calc&` runs as a separate command (the BatBadBut class). `IsYouTubeURL` only checks the prefix, and clipboard auto-paste fills the field. `.py` and `.ps1` can't be launched at all.
  - Location: `ytdlp.c:139-143`, `:1994`, `:872-916`; `ui.c:905`; launch at `parser.c:1134`, `:1141` and `threadsafe.c:600`, `:669`; `uri.c:10`; `ui.c:81-83`
  - Verification: T (a `.bat` path is rejected; the crafted URL runs nothing)
  - Confidence: Verified
  - Resolved: `3feec35`; checked by unit test (`test_ytdlp_args`)

- [x] **FUN-001**: When the YouTubeCacher application loads the cache index from disk, it shall make every loaded entry findable by video ID.
  - Rationale: `LoadCacheFromFile` links entries into the list but not into `hashBuckets`, which `FindCacheEntry` searches. After a restart, play and delete find nothing. The startup folder scan then re-adds each video, so `cache_index.txt` gains another copy of every entry on each launch.
  - Location: `cache.c:423-426`; lookup at `cache.c:719-723`
  - Verification: T (save and reload; `FindCacheEntry` succeeds for each ID; the entry count is stable across two launches)
  - Confidence: Confirmed
  - Resolved: `7df7c51`; checked by build and inspection

- [x] **MEM-004**: When the multi-download dialog is cancelled or closed, the YouTubeCacher application shall release the dialog context only after every thread that references it has exited.
  - Rationale: `IDCANCEL` waits 5 s for the coordinator, then deletes `itemLock` and frees `ctx` anyway. A playlist resolve that takes longer later reads `ctx->hDialog` and enters the freed critical section.
  - Location: `dialogs.c:3439-3452`
  - Verification: T (cancel during a slow playlist resolve, under page heap)
  - Confidence: Confirmed
  - Resolved: `9af395d`; checked by build and inspection

- [x] **MEM-005**: While another thread may modify a yt-dlp output or session-log buffer, the YouTubeCacher application shall read that buffer only under the lock that guards it.
  - Rationale: the getters return raw pointers. The log viewer calls `wcslen`, `SetDlgItemTextW` and `EM_REPLACESEL` on them with no lock, while `AppendToYtDlpSessionLog` reallocates them under `ytdlpSessionLogLock`, which nothing outside `appstate.c` ever takes. The "last run" buffer starts at 64K characters, so it is reallocated early in a verbose run. `ClearYtDlpSessionLogLast` races with the readers too. `GetYtDlpOutputBuffer` has the same flaw on a separate buffer.
  - Location: getters at `appstate.c:605-615` (output buffer) and `:705-715` (session logs); writer at `appstate.c:643-697`; clear at `appstate.c:629-636`; readers at `dialogs.c:2670-2689`, `:2734-2783`; writer callers at `threadsafe.c:623-632`, `:767`, `:1077`, `:1231`
  - Verification: T (log viewer open during a download producing more than 64K characters, under page heap)
  - Confidence: Corrected
  - Resolved: `8ea6e07`; checked by build and inspection

- [x] **MEM-006**: The YouTubeCacher application shall read and modify the freed-memory record list only while holding `g_errorLock`.
  - Rationale: `IsFreedMemory` walks `g_freedMemoryList` without the lock, while `AddFreedMemoryRecord` frees nodes under it. Once the list reaches its 1000-entry cap, two threads freeing memory at the same time can crash.
  - Location: `memory.c:1700-1715`, `memory.c:1650`; called from `memory.c:480`
  - Verification: I, and T with a multithreaded free stress test
  - Confidence: Confirmed
  - Resolved: `fe414c6`; checked by build and inspection

- [x] **FUN-002**: When the heap returns an address that is still in the freed-memory record list, the YouTubeCacher application shall remove that address from the list.
  - Rationale: a reused address is never removed; the list only drops its oldest entry at 1000. Freeing a reallocated block is therefore reported as a double free and skipped: the block leaks and `error.log` fills with false errors. This is on in release builds. `g_doubleFreDetectionEnabled` starts `TRUE`, `InitializeMemoryManager` sets `leakDetectionEnabled` unconditionally, and no source file tests `MEMORY_RELEASE`.
  - Location: `memory.c:14`, `:82`, `:480-506`, `:637-659`, `:1657-1677`; `main.c:196`
  - Verification: T (malloc, free, malloc the same address, free again: no double-free report and no leak)
  - Confidence: Verified
  - Resolved: `7516b4b`; checked by unit test (`test_memory`)

---

## Medium

- [x] **CON-001**: The YouTubeCacher application shall keep each subprocess output-reader thread's line-assembly state private to that thread.
  - Rationale: both line accumulators are function-level `static` variables. The `threadsafe.c` one is shared by concurrent readers today:
    - Get Info has no in-progress guard, so two clicks run two readers.
    - Get Info followed by Download with no cached metadata starts a second one.
    - Get Info followed by a multi-download playlist resolve does the same.
    - A reader that outlives its 2 s join window overlaps the next run.

    The `parser.c` accumulator serves only the main download, which the Download/Cancel button serialises. It can overlap only when a new download starts right after a Cancel.
  - Location: `parser.c:1187-1188`; `threadsafe.c:965-966`, `:1150`; `ui.c:2061`, `:2220`; `dialogs.c:3054`
  - Verification: I, and T with two overlapping Get Info requests
  - Confidence: Corrected
  - Resolved: `1f537d0`, `e687669`; checked by build and inspection

- [x] **FUN-003**: When the yt-dlp process has exited and its pipe is drained, the YouTubeCacher application shall finish the output-reader loop within 1 s.
  - Rationale: the loop runs while `fillCounter > 0` and drains only at a `\n` or `\r`. Final output with no line terminator keeps it sleeping in 50 ms steps until the 24-hour timeout. Once the 8192-byte accumulator fills, `bytesToCopy` is 0 and every later read, terminators included, is discarded, with the same result. `ExecuteYtDlpRequestMultithreaded` then spins with no timeout, so the UI stays in the downloading state.
  - Location: `parser.c:1198`, `:1337`; `ytdlp.c:2421`
  - Verification: T (a child that writes an unterminated final line, and one that writes a 16 KB line)
  - Confidence: Verified
  - Resolved: `2972bd4`; checked by unit test of the UTF-8 boundary helper (`test_parser_output`); loop by inspection

- [x] **MEM-007**: When the YouTubeCacher application appends to the accumulated subprocess output, it shall grow the buffer before writing and bound the write by the buffer's actual size.
  - Rationale: the final partial line (up to 2047 characters) is appended with `wcscat` without checking capacity. Separately, `outputBufferSize` is raised before `SAFE_REALLOC`. When that fails, the old block is kept and the next `wcscat` overflows it.
  - Location: `parser.c:1353`; `parser.c:1288-1298`
  - Verification: T (cancel mid-line with output near capacity; fault-injected realloc failure)
  - Confidence: Verified
  - Resolved: `bba98d5`; checked by unit test (`test_parser_output`)

- [x] **FUN-004**: The YouTubeCacher application shall pass every subprocess output line to parsing and the stored output in full, whatever its length.
  - Rationale: lines longer than 2047 wide characters make `MultiByteToWideChar` fail, and the whole line is dropped. The threadsafe reader has two more losses:
    - It exits as soon as the process stops running, without draining the pipe.
    - It keeps a partial line only if it is under 8 bytes, and throws even that away when the next read brings the total to 8 bytes or more.

    The final `ERROR:` text and `--flat-playlist --print` entries go missing.
  - Location: `parser.c:1270`, `:1344`; `threadsafe.c:976`, `:1041-1043`, `:1064`, `:1094`
  - Verification: T (a child writing a 5000-character line, and output crossing a 4 KB boundary mid-line)
  - Confidence: Verified
  - Resolved: `4fe0cf4`, `ad2a0b9`; checked by unit test of the threadsafe.c reader (`test_threadsafe`); parser.c half by inspection

- [x] **MEM-008**: When the main window is destroyed, the YouTubeCacher application shall stop and join all worker threads before it frees shared application state or deletes locks.
  - Rationale: `WM_DESTROY` runs `CleanupApplicationState`, which frees the output and session-log buffers and deletes their locks, without stopping or joining the download, file-size or reader threads (`WM_CLOSE` doesn't either). Workers can touch freed state until `ExitProcess` ends the process. The `WinMain` cleanup after the message loop never runs on a normal exit, because `WM_DESTROY` calls `ExitProcess(0)` first.
  - Location: `ui.c:2704-2723`, `:2993-2995`, `:3020`; `appstate.c:152-178`
  - Verification: T (after CON-002 is fixed: exit during an active download, under page heap)
  - Confidence: Corrected
  - Resolved: `b15afbb`; checked by build and inspection

- [x] **SEC-002**: If a cache entry names a video or subtitle file outside the configured download folder, then the YouTubeCacher application shall not open or delete that file.
  - Rationale: paths are copied as-is from `cache_index.txt`, and `DeleteFileW` has no path check before it; there is no canonicalization or prefix test anywhere in the code. A crafted index, for example in a synced or shared folder, turns Delete into deleting any file the user can write. Today FUN-001 hides this, because lookups find the scan-rebuilt entry instead. Once FUN-001 is fixed, `AddCacheEntry`'s duplicate check rejects the scanned copy and the index path is used.
  - Location: `cache.c:393-395`, `:413-415`, `:771`, `:817`; duplicate check at `cache.c:604-607`
  - Verification: T (an index naming `..\..\x` or an absolute path outside the folder is rejected)
  - Confidence: Verified
  - Resolved: `bca04f1`; checked by unit test of the containment check (`test_cache_validation`); load/delete/play wiring by inspection

- [x] **RES-001**: When a yt-dlp run completes, the YouTubeCacher application shall close the process, thread and output-pipe handles it opened for that run.
  - Rationale: the cleanup sets `hProcess`, `hThread` and `hOutputRead` to `NULL` without closing them, and nothing else closes them. The closes in `ytdlp.c` are for the legacy context. Each run leaks three handles and keeps the dead process object alive.
  - Location: `threadsafe.c:331-336`
  - Verification: T (handle count is unchanged after 20 downloads)
  - Confidence: Verified
  - Resolved: `2ffc7f1`; checked by unit test (`test_threadsafe`)

- [x] **FUN-005**: When the Settings dialog saves the debug, log-file and auto-paste options, the YouTubeCacher application shall store each in the registry type that startup reads.
  - Rationale: the dialog writes `REG_DWORD`, but startup reads through `LoadSettingFromRegistry`, which accepts only `REG_SZ`. The options revert to debug off, log file off and auto-paste on at every restart. The dialog's own reader accepts any type, so a `REG_SZ "0"` left by an older version reads as 0x30 and shows as checked. The `REG_SZ` writer in `settings.c` is dead code.
  - Location: `ui.c:1151-1153` (write); `ui.c:1353-1362`, `settings.c:203` (startup read); `ui.c:977-983` (dialog read); `settings.c:373-390` (unused)
  - Verification: T (toggle each option, restart, and the value holds)
  - Confidence: Verified
  - Resolved: `7429dca`; checked by unit test of the registry helpers (`test_settings_registry`); dialog wiring by inspection

- [x] **FUN-006**: When the multi-download dialog processes a non-playlist video URL, the YouTubeCacher application shall download that video.
  - Rationale: the coordinator only resolves playlists and then posts `WM_MULTI_DL_ALL_DONE`. `MultiDlSingleDownloadThread` is declared but has no body or caller; commit `bc47e93` removed it together with its `CreateThread` call. Plain URLs finish with "0 succeeded, 0 failed". A resolved playlist counts as one success although none of its videos download. `failedCount` is never incremented, `maxConcurrent` is written but never read, and `hPauseEvent` is set but never waited on.
  - Location: `dialogs.c:3110-3158`, `:3376`, `:3382`, `:3519`, `:3549`; `YouTubeCacher.h:518`
  - Verification: D (a batch of three video URLs produces three files)
  - Confidence: Verified
  - Resolved: `ff05b91`; checked by build and inspection

- [x] **FUN-007**: When the Settings dialog closes with OK, the YouTubeCacher application shall validate and save each path exactly as it appears in its edit box.
  - Rationale: nothing assigns the components' `destroy` or `validate` fields.
    - `ValidateComponent` returns TRUE when `validate` is NULL, and `ValidateFileBrowser`/`ValidateFolderBrowser` are never called.
    - The saved value comes from `currentPath`, which only the "..." button updates. Nothing reads the edit box, so a typed path is discarded.
    - `DestroyComponentRegistry` skips the NULL `destroy` hooks, so every Settings open leaks the component structs.
  - Location: `components.c:79-80`, `:135`, `:926-931`; `ui.c:1125`, `:1206-1214`
  - Verification: T (a typed path is saved; a missing yt-dlp path is refused; no leak after 10 opens)
  - Confidence: Verified
  - Resolved: `ea743f9`; checked by build and inspection

- [x] **RES-002**: When a dialog is destroyed, the YouTubeCacher application shall release the DPI context, font manager and fonts created for that dialog.
  - Rationale: `UnregisterWindowForDPI` has no callers. Every registration creates a new context and font manager (1–2 fonts in most dialogs, 3 in About), and they are only freed by `DestroyDPIManager`, which normal exit never reaches (see MEM-008). GDI use grows with every dialog opened.
  - Location: `dpi.c:120`, `:172`; registration at `dialogs.c:242`, `:1235`, `:2087`, `:2640`, `:3222`; `ui.c:815`, `:1321`; `main.c:289`
  - Verification: T (GDI object count is stable across 50 dialog opens)
  - Confidence: Verified
  - Resolved: `a5bfb2a`; checked by build and inspection

- [x] **MEM-009**: The YouTubeCacher application shall keep the state of each open unified dialog instance separate from every other instance.
  - Rationale: `config` and `isExpanded` are `static`. While one unified dialog is modal, the main window still receives posted messages. The metadata worker's `WM_USER + 103` makes `ui.c:2735` open a second dialog with a stack-local config, and after it closes the outer dialog's Copy reads a dead stack frame. `error.c:914` passes a heap config that is freed as soon as its dialog returns, which turns the same path into a use-after-free. `isExpanded` is never reset, so it carries over to the next dialog.
  - Location: `dialogs.c:223`, `:229-234`, `:474`, `:510`; `ui.c:2735`, `:2798`, `:2815`; `ytdlp.c:1761`, `:1780`; `error.c:914`
  - Verification: T (open two nested unified dialogs, then Copy in the outer one, under page heap)
  - Confidence: Corrected
  - Resolved: `b88f9ef`; checked by build and inspection

- [x] **MEM-010**: Where `MEMORY_DEBUG` is enabled, the YouTubeCacher application shall free and reallocate each block according to how it was allocated, whether or not it is tracked.
  - Rationale: `SafeMalloc` always adds 16-byte guards and returns `raw + 16`, but `SafeFree` and `SafeRealloc` step back to `raw` only for tracked blocks. An untracked block, such as one allocated before the manager initializes, is passed to `free` or `realloc` as an interior pointer, which corrupts the heap. `SafeRealloc` also fills the untracked block with `0xCC`, because its `oldSize` is 0.
  - Location: `memory.c:139-189`, `:227`, `:396`, `:431-434`, `:555`
  - Verification: T (debug build: free and realloc a block allocated before initialization)
  - Confidence: Corrected
  - Resolved: `952ea92`; checked by unit test (`test_memory_debug`)

- [x] **MEM-011**: Where `MEMORY_DEBUG` is enabled, when `WinMain` returns, the YouTubeCacher application shall release the command-line buffer through the allocator path that allocated it.
  - Rationale: `WinMain` allocates `lpCmdLineW` with `SAFE_MALLOC` before `InitializeMemoryManager`, so the block is untracked. `SAFE_FREE` then hits MEM-010. Normal exit never reaches the free, because of `ExitProcess`. It does happen on `wWinMain`'s early returns: a second instance started with an argument (the realistic case), an initialization failure, an IPC failure, or `hDlg == NULL`.
  - Location: `main.c:176-188`, `:252`, `:318`, `:330`
  - Verification: T (debug build: launch a second instance with a URL argument while one is running)
  - Confidence: Corrected
  - Resolved: `9ccc948`; checked by unit test (`test_cmdline`); the allocation is gone

- [x] **CON-003**: The YouTubeCacher application shall initialize each thread context's critical section exactly once and shall not end a worker thread with `TerminateThread`.
  - Rationale: `CreateManagedThread` re-runs `memset` and `InitializeCriticalSection` on a context that `ytdlp.c:2187` already initialized, which leaks its debug info. `TerminateThread` at `threading.c:50` is reachable from `FreeSubprocessContext` when its 60 s wait times out. It can leave the heap or tracker lock held and deadlock every later allocation. The call at `:990` (`ForceTerminateThread`) is dead code.
  - Location: `threading.c:15`, `:20`, `:50`, `:926`, `:990`; `parser.c:1027`; `ytdlp.c:2187`
  - Verification: I
  - Confidence: Verified
  - Resolved: `1267725`; checked by build and inspection

- [x] **BLD-001**: The YouTubeCacher build shall give each toolchain and configuration its own object directory and output file name.
  - Rationale: `debugucrt64` and `releaseucrt64` build into `$(OBJ64_DIR)` and `$(TARGET64)`, the same as MINGW64. Running `make releaseucrt64` after `release64` therefore rebuilds nothing and ships the msvcrt build. Debug and release also share object directories. The UCRT targets are missing from `.PHONY`, and there is no `cleanucrt64`.
  - Location: `Makefile:9`, `:22`, `:90-98`, `:125-133`
  - Verification: T (`release64` then `releaseucrt64` rebuilds every object)
  - Confidence: Confirmed
  - Resolved: `11b00a2`; checked by run: separate objects and executables per variant and configuration, incremental rebuilds

---

## Low

- [x] **MEM-012**: The YouTubeCacher application shall read the cache hash table and cache entries only while holding the cache lock.
  - Rationale: `ui.c` calls `FindCacheEntry` without the lock while worker threads' `AddCacheEntry` modifies `hashBuckets`, which is a data race. `DeleteCacheEntryFilesDetailed` also reads `entry->title` after releasing the lock. That isn't a live use-after-free today, because entries are freed only on the UI thread and at exit, but it becomes one if removal ever moves to a worker.
  - Location: `cache.c:864-872`; `ui.c:2389`, `:2415`, `:2457`
  - Verification: I
  - Confidence: Corrected
  - Resolved: `bffdd76`; checked by build and inspection

- [x] **SEC-003**: If a cache index record has a subtitle count outside 0 to 100, then the YouTubeCacher application shall reject that record.
  - Rationale: the count is stored before the range check and never clamped. A negative count makes `totalFiles` 0 or less, so `errors` is never allocated, and a failed delete of the main file writes through `NULL`. A count above 100 is harmless today, because the subtitle loop is guarded, but `INT_MAX` would overflow `totalFiles`.
  - Location: `cache.c:407-408`, `:756-773`, `:816`
  - Verification: T (records with -5, 1000 and `INT_MAX` are rejected)
  - Confidence: Corrected
  - Resolved: `d9b8100`; checked by unit test (`test_cache_validation`)

- [x] **MEM-013**: If growing the multi-download item array fails, then the YouTubeCacher application shall leave the recorded capacity equal to the actual allocation and report the URLs it could not add.
  - Rationale: `capacity *= 2` runs before the realloc. After a failure, `ctx->itemCapacity` holds the doubled value, so the later playlist expansion skips its own realloc and writes past `items`. The remaining URLs are dropped without a message.
  - Location: `dialogs.c:3343-3345`, `:3375`, `:3567-3576`
  - Verification: T (fault-injected realloc failure)
  - Confidence: Verified
  - Resolved: `ef89593`; checked by build and inspection

- [x] **FUN-008**: If queuing a resolved playlist's videos fails, then the YouTubeCacher application shall count that playlist as failed and queue none of its URLs.
  - Rationale: the playlist item is marked COMPLETE and counted as succeeded before the realloc. When the realloc fails, no videos are queued, yet the edit-box line is still removed and the video URLs are still appended. Failed playlist resolutions post `ITEM_DONE` but never increment `failedCount`.
  - Location: `dialogs.c:3548-3605`, `:3092-3099`, `:3481-3500`
  - Verification: T (fault-injected failure; a resolve failure is counted)
  - Confidence: Corrected
  - Resolved: `a7309df`; checked by build and inspection

- [x] **MEM-014**: When the YouTubeCacher application reads a string setting from the registry, it shall return a string terminated within its buffer.
  - Rationale: a `REG_SZ` exactly `bufferSize*2` bytes long without a terminator comes back unterminated. A zero-length one returns TRUE without writing anything, leaving the caller's buffer uninitialized.
  - Location: `settings.c:196-205`
  - Verification: T (write edge-case registry values, then read them back)
  - Confidence: Verified
  - Resolved: `f577085`; checked by unit test (`test_settings_registry`)

- [x] **FUN-017**: When the Settings dialog closes with OK, the YouTubeCacher application shall save the custom yt-dlp arguments field.
  - Rationale: the `IDOK` handler never reads or saves `IDC_CUSTOM_ARGS_FIELD`. The `SaveSettings` function that would has no callers, so custom arguments can only be set by editing the registry. Found while checking FUN-009.
  - Location: `ui.c:1092-1158`; `settings.c:339` (unused)
  - Verification: T (enter arguments, click OK, reopen Settings, and they persist)
  - Confidence: Verified
  - Resolved: `74e5fca`; checked by build and inspection

- [x] **FUN-009**: The YouTubeCacher application shall read back custom yt-dlp arguments of any length that the registry holds.
  - Rationale: they are read into a 1024-character buffer. Longer values fail with `ERROR_MORE_DATA`, the buffer is cleared, and the arguments are silently dropped. Today only a registry edit can create such a value (see FUN-017).
  - Location: `ytdlp.c:162`, `:1882`
  - Verification: T (store and reload 2000 characters of arguments)
  - Confidence: Corrected
  - Resolved: `40855cd`; checked by build and inspection

- [x] **MEM-015**: When the YouTubeCacher application restores the saved window position, it shall read each value only into a buffer of the size and type that value requires.
  - Rationale: the five values are read into 4-byte `DWORD`s with `lpType` NULL, and `size` is set once and never reset. An oversized `_Left` doesn't overflow itself. It returns `ERROR_MORE_DATA` and grows `size`, so the next query writes the next value's full size into a 4-byte buffer. That overflows the stack when a later value is also larger than 4 bytes.
  - Location: `dpi.c:1012-1037`
  - Verification: T (set `_Left` and `_Top` to 64-byte REG_BINARY values, then launch)
  - Confidence: Corrected
  - Resolved: `58b5496`; checked by unit test (`test_settings_registry`)

- [x] **MEM-016**: When the About dialog is destroyed, the YouTubeCacher application shall delete only the fonts that the dialog itself created.
  - Rationale: when a DPI manager exists, the title and small fonts come from `GetFontForDPI` and belong to the DPI manager. `WM_DESTROY` deletes them while they stay registered, and `DestroyDPIManager` would delete them again. `WM_DPICHANGED` registers two more fonts each time without updating the stored ones; they grow like RES-002. The path without a DPI manager deletes its own fonts correctly.
  - Location: `dialogs.c:2323-2324`, `:2395-2407`, `:2447-2448`
  - Verification: I, and T (open and close About, then change DPI)
  - Confidence: Verified
  - Resolved: `bba7167`; checked by build and inspection

- [x] **SEC-004**: When a yt-dlp command is about to run, the YouTubeCacher application shall validate the custom arguments it will pass.
  - Rationale: validation runs only at startup, and it blocks only `--exec` and `--batch-file`, by substring; for example, `-a` passes. Each run re-reads the arguments from the registry without validating them. Today they change after startup only through registry edits (FUN-017).
  - Location: `ytdlp.c:851`, `:162`, `:2586`, `:2765`; `main.c:273`
  - Verification: T (arguments changed after startup are validated before the run)
  - Confidence: Verified
  - Resolved: `64b0200`; checked by unit test (`test_ytdlp_args`, 25 cases)

- [x] **FUN-010**: The YouTubeCacher application shall pass the download folder into yt-dlp's output template with every template metacharacter escaped.
  - Rationale: `outputPath` is inserted into the template without escaping `%`. yt-dlp appears to escape a lone `%` itself, so the realistic failures are folder names containing `%(` or `%%`, which are read as template syntax. That yt-dlp behaviour is from a reviewer's knowledge and has not been checked against its source.
  - Location: `ytdlp.c:933`
  - Verification: T (download into folders named `a%(title)s` and `100%%`)
  - Confidence: Corrected
  - Resolved: `5064c6a`; checked by unit test (`test_ytdlp_args`)

- [x] **FUN-011**: When the YouTubeCacher application asks yt-dlp to stop on a timeout or during cleanup, it shall use a signal that reaches a `CREATE_NO_WINDOW` child.
  - Rationale: `GenerateConsoleCtrlEvent(CTRL_C_EVENT, pid)` has no effect: a GUI process has no console, and the pid isn't a process-group leader. The user's Cancel is not affected, because it calls `TerminateProcess` directly. This path runs only on the 5-minute timeout and during context cleanup, where the graceful stop never happens.
  - Location: `threadsafe.c:299`, `:779`; `subproc.c:91`
  - Verification: T (trigger the timeout; yt-dlp receives the stop request and exits cleanly)
  - Confidence: Corrected
  - Resolved: `0cd0f2d`, `abc0e43`; checked by unit test (`test_threadsafe`)

- [x] **RES-003**: If `StartNonBlockingDownload` fails, then the YouTubeCacher application shall free the download request, remove the temporary directory it created, and return the UI to idle.
  - Rationale: only the two context structs are freed. The request's strings leak, the temporary directory stays on disk, and `WM_DOWNLOAD_COMPLETE` with NULL arguments just returns, leaving the UI in the downloading state.
  - Location: `ytdlp.c:2042`, `:2157`, `:2461-2465`; `ui.c:2871-2873`
  - Verification: T (fault-injected start failure)
  - Confidence: Verified
  - Resolved: `6261db8`; checked by build and inspection

- [x] **RES-004**: If an IPC message or window message carrying heap data cannot be delivered, then the YouTubeCacher application shall free that data.
  - Rationale: `SendStatusUpdate`, `SendTitleUpdate` and `SendDurationUpdate` duplicate their string, and nothing frees it when `SendIPCMessage` fails (queue full or shutting down); callers ignore the return value. The IPC worker clears the pointer even when `PostMessageW` fails.
  - Location: `threading.c:113-133`, `:304-340`, `:678-680`; callers at `parser.c:736`, `:782`, `:1427` and `threading.c:398`, `:419`
  - Verification: I
  - Confidence: Verified
  - Resolved: `a867d6d`; checked by build and inspection

- [x] **FUN-012**: When the application reads its command-line arguments, the YouTubeCacher application shall read them as UTF-16 from the operating system.
  - Rationale: the `WinMain` wrapper decodes the ANSI `lpCmdLine`, which is in the system code page, as `CP_UTF8`. The manifest doesn't set a UTF-8 active code page, so non-ASCII arguments are garbled.
  - Location: `main.c:314-322`
  - Verification: T (launch with a non-ASCII path argument)
  - Confidence: Verified
  - Resolved: `9ccc948`; checked by unit test (`test_cmdline`)

- [x] **FUN-013**: When a URL finishes in the multi-download dialog, the YouTubeCacher application shall remove only the edit-box line that exactly matches that URL.
  - Rationale: removal finds the URL with `wcsstr` and then deletes the containing line, so a longer URL that contains the finished one, and appears earlier, is removed instead. Because of FUN-006, the only caller reachable today is playlist-origin removal.
  - Location: `dialogs.c:3195`, `:3589`
  - Verification: T (queue `…?v=abcd` above `…?v=abc`; finish the second)
  - Confidence: Verified
  - Resolved: `41f8116`; checked by unit test (`test_multidl_lines`)

- [x] **FUN-014**: When the list's context menu is opened, the YouTubeCacher application shall place it at the pointer on any monitor, or at the selected item when opened by keyboard.
  - Rationale: coordinates are read with `LOWORD`/`HIWORD`, so negative monitor coordinates become about 65535. `lParam == -1` from Shift+F10 isn't handled, which puts the menu at (65535, 65535).
  - Location: `ui.c:1681-1682`
  - Verification: D (a monitor to the left of the primary display; Shift+F10)
  - Confidence: Verified
  - Resolved: `2778fe8`; checked by build and inspection

- [x] **FUN-015**: The YouTubeCacher application shall give each Settings control an ID that no other control in the same dialog uses.
  - Rationale: the component edit boxes use `ID + 1` (1015, 1018, 1021), the same IDs as the hidden resource Browse buttons. `SetDialogTabOrder` therefore finds the hidden buttons. The component "..." buttons use `ID + 2` (1016, 1019), the same IDs as `IDC_FOLDER_LABEL` and `IDC_PLAYER_LABEL`.
  - Location: `components.c:275`; `resource.h:41-48`; `ui.c:1002-1008`; `keyboard.c:38`
  - Verification: I
  - Confidence: Corrected
  - Resolved: `01a18d7`; checked by build and inspection

- [x] **FUN-016**: The YouTubeCacher application shall either handle every message type it posts or not post it.
  - Rationale: `IPC_MSG_VIDEO_INFO_COMPLETE` has no sender, and the `WM_USER + 101` it would post has no handler. `ui.c` handles only `+100` and `+103`.
  - Location: `threading.h:22`; `threading.c:157-159`; `ui.c:2726`, `:2735`
  - Verification: I
  - Confidence: Verified
  - Resolved: `d9a6910`; checked by build and inspection

- [x] **MEM-017**: When the YouTubeCacher application formats error technical details, it shall bound every write by the 4096-character buffer.
  - Rationale: appends use unbounded `wcscat`. The header can reach about 1,150 characters and each context-variable line 324, with up to 16 variables allowed. About 9 would overflow the buffer; current call sites add at most about 5.
  - Location: `error.c:652-658`, `:484`, `:834-836`; `error.h:109-112`, `:122`
  - Verification: A
  - Confidence: Verified
  - Resolved: `e90d2f5`; checked by build and inspection

- [x] **BLD-002**: Where `TOOLCHAIN_BIN` is not given, the cross build shall locate the llvm-mingw toolchain through an absolute path.
  - Rationale: `TOOLCHAIN_BIN ?= ~/llvm-mingw/bin` puts a literal `~` on `PATH`. A test Makefile with the same construct failed with exit 127 for a simple recipe and "command not found" through `/bin/sh`.
  - Location: `Makefile.cross:26-27`
  - Verification: T (`make -f Makefile.cross` with no toolchain on `PATH`)
  - Confidence: Confirmed
  - Resolved: `a969eef`; checked by run: `make -f Makefile.cross` with the default `$(HOME)/llvm-mingw`

- [x] **BLD-003**: The unit-test build shall use its own compiler default and include flags regardless of make's built-in variables and the calling environment.
  - Rationale: `CC ?= gcc` does nothing, because `CC` is a make built-in (`cc`). With `CFLAGS=-O2` in the environment, `CFLAGS ?=` loses `-I./include -DTEST_BUILD`, and `test_memory` and `test_parser_postprocess` then fail with `windows.h: No such file or directory`. CI works around both by passing `CC=gcc` and not setting `CFLAGS`.
  - Location: `tests/Makefile:1-3`
  - Verification: T (`CFLAGS=-O2 make -C tests` builds; plain `make -C tests` uses gcc)
  - Confidence: Confirmed
  - Resolved: `44e8c1f`; checked by run: `CFLAGS=-O2 make -C tests all`

- [x] **BLD-004**: When `make clean` runs in `tests/`, the build shall remove every file that a `tests/Makefile` rule generates.
  - Rationale: `clean` leaves `test_error` and `error_logic.c` on disk after `make test_error`. `tests/.gitignore` already covers these and the other generated files, so `git status` stays clean. `test_cache_cmdline` is a committed binary with no build rule.
  - Location: `tests/Makefile:102-103`
  - Verification: T (after `make test_error clean`, neither file exists)
  - Confidence: Corrected
  - Resolved: `3626eff`; checked by run: `make test_error clean`

---

## Resolved

- [x] **BLD-R01**: The YouTubeCacher sources shall compile at every commit on `main`. HEAD referenced the undefined `IPC_MSG_VIDEO_INFO_COMPLETE`. Fixed in `ca9b67e`.
- [x] **BLD-R02**: The YouTubeCacher sources shall compile without warnings under clang with `-Wall -Wextra -Werror`. `parser.c`'s `loopCount` was set but never used. Fixed in `ca9b67e`.
- [x] **BLD-R03**: The unit tests shall build and pass on every push. `test_memory` and `test_subproc` did not compile against `mock_windows.h`. Fixed, and added to CI, in `918aec4`.
- [x] **MEM-R01**: The YouTubeCacher application shall grow tracked allocations only with the tracked allocator. The multi-download list used plain `realloc` on a `SAFE_MALLOC` block, which corrupted the heap in debug builds. Fixed in `8b425d3`.
- [x] **BLD-R04**: Windows cross-builds for x86_64, i686 and aarch64, debug and release, shall run in CI. Added in `ca9b67e`.
