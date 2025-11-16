---
inclusion: always
---

# ChangeLog Update Guidelines

## When to Update

Update ChangeLog after implementing significant changes:

- New features or functionality
- Bug fixes and crash resolutions
- Performance improvements
- UI/UX changes
- Architecture or design changes
- Build system modifications
- Error handling improvements
- Thread safety enhancements

## Format Structure

```text
Version Number

Category Name:
- Change description with specific details
- Another change in same category

Next Category:
- Changes in this category
```

## Standard Categories (in order)

1. Memory Management
2. User Interface
3. Download System
4. Error Handling
5. Thread Safety and Concurrency
6. Performance
7. Cache Management
8. Crash Fixes
9. Logging and Debugging
10. Build System

Create new categories only when existing ones don't fit.

## Writing Entries

Start with action verbs: Add, Fix, Implement, Remove, Update, Improve, Enhance

Be specific with technical details:

- Include function names, file names, or specific issues when relevant
- Explain what changed and why
- Keep entries concise but informative
- Group related changes together

Good examples:

- `Fix download hang by processing carriage return line terminators`
- `Add thread-safe logging system with ThreadSafeDebugOutput functions`
- `Remove popup progress dialogs and use main window progress controls only`

Avoid vague entries:

- `Fix bug` (too vague)
- `Update code` (not specific)
- `Various improvements` (no detail)

## Version Management

- Always add new entries to the topmost version section in ChangeLog (the unreleased version)
- Add entries to existing categories within that version (newest at bottom of category)
- Never modify previous version entries
- Don't create new version sections without approval
