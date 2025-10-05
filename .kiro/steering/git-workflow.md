---
inclusion: always
---

# Git Workflow Management

## Commit and Push Requirements

After completing any development task, feature implementation, or significant code change:

1. **Always commit changes** using descriptive commit messages
2. **Always push commits** to the remote repository to ensure work is backed up
3. **Use clear, descriptive commit messages** that explain what was changed and why

## Implementation Protocol

```bash
# After completing any task:
git add .
git commit -m "Descriptive message about what was implemented/changed"
git push
```

## Commit Message Guidelines

- Use present tense ("Add feature" not "Added feature")
- Be specific about what was changed
- Include context when helpful
- Examples:
  - "Add progress bar to download frame with proper positioning"
  - "Fix label alignment issues in main dialog"
  - "Implement Settings dialog with file/folder browsing"

## When to Commit and Push

- After implementing any new feature
- After fixing bugs or layout issues
- After refactoring code or reorganizing files
- After updating documentation or configuration
- Before switching to work on different functionality
- At the end of any development session

This ensures all work is properly versioned and backed up, preventing loss of progress and maintaining a clear development history.