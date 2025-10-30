# UI/UX Pattern Guidelines

## Strictly Prohibited UI Patterns

### Progress Dialog Prohibition
- **NEVER create popup progress dialogs during any operation**
- **NEVER use `CreateProgressDialog`, `UpdateProgressDialog`, `DestroyProgressDialog`**
- **NEVER show modal dialogs for progress indication**
- The `ProgressDialog` system was explicitly removed for being terrible UX
- Any code that creates popup progress dialogs must be immediately removed

### Required Progress Patterns
- Use the main window's integrated progress bar only
- Progress updates must be non-blocking and asynchronous
- Background operations must not freeze or block the main UI
- Progress information should be shown in status text within the main window
- Use `UpdateMainProgressBar` and `SetProgressBarMarquee` for progress indication

### Background Operation Rules
- All downloads and yt-dlp operations must run in background threads
- Main UI thread must remain responsive at all times
- Use IPC messaging system for thread-to-UI communication
- Never block the main thread waiting for subprocess completion
- Use non-blocking progress callbacks and window messages

## User Experience Principles

### Download Experience
- Downloads should feel seamless and non-intrusive
- Users should be able to continue using the application during downloads
- Progress should be visible but not dominating the interface
- No popup windows should interrupt the user's workflow
- Cancel operations should be easily accessible without modal dialogs

### Error Handling
- Use unified dialog system for errors, not custom progress dialogs
- Error dialogs should be informative but not blocking ongoing operations
- Provide clear, actionable error messages with solutions
- Allow users to dismiss errors and continue working

## Implementation Guidelines

### Threading Model
- Main UI thread handles all UI updates and user interaction
- Background worker threads handle yt-dlp execution and file operations
- Use Windows messaging system for cross-thread communication
- Implement proper thread synchronization without blocking UI

### Progress Reporting
- Progress updates should be frequent but not overwhelming
- Use percentage-based progress when possible
- Show meaningful status messages (not just "Processing...")
- Provide ETA and speed information when available
- Handle indeterminate progress gracefully

## Code Review Checklist

Before implementing any UI changes, verify:
- [ ] No modal progress dialogs are created
- [ ] Main UI remains responsive during all operations
- [ ] Background threads are used for long-running operations
- [ ] Progress is shown in main window controls only
- [ ] Error handling uses unified dialog system
- [ ] User can cancel operations without modal dialogs
- [ ] No blocking waits in the main UI thread