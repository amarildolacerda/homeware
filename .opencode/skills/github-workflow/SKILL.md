---
name: github-workflow
description: GitHub workflow automation for issues, PRs, releases, and repository management. Use when user asks to create/manage issues, PRs, releases, or perform GitHub operations.
---

# GitHub Workflow Skill

## Quick Commands

### Issues
```bash
# List issues
gh issue list

# Create issue
gh issue create --title "Title" --body "Description" --label "bug"

# View issue
gh issue view <number>

# Close issue
gh issue close <number>
```

### Pull Requests
```bash
# List PRs
gh pr list

# Create PR
gh pr create --title "Title" --body "Description" --base main

# View PR
gh pr view <number>

# Review PR
gh pr review <number> --approve

# Merge PR
gh pr merge <number> --squash
```

### Releases
```bash
# List releases
gh release list

# Create release
gh release create v1.0.0 --title "v1.0.0" --notes "Release notes"

# Upload asset
gh release upload v1.0.0 ./firmware.bin
```

### Repository
```bash
# View repo
gh repo view

# Clone repo
gh repo clone owner/repo

# Create branch
git checkout -b feature/new-feature

# Push branch
git push -u origin feature/new-feature
```

## Workflow Patterns

### Feature Branch Workflow
1. Create branch: `git checkout -b feature/xxx`
2. Make changes
3. Commit: `git add . && git commit -m "feat: description"`
4. Push: `git push -u origin feature/xxx`
5. Create PR: `gh pr create`
6. Review and merge

### Issue-Driven Development
1. Create issue with requirements
2. Create branch from issue: `gh issue develop <number>`
3. Implement changes
4. Reference issue in PR: `Closes #<number>`
5. Merge PR (auto-closes issue)

### Release Process
1. Update version in code
2. Commit: `git commit -am "chore: bump version to vX.Y.Z"`
3. Create tag: `git tag vX.Y.Z`
4. Push: `git push origin vX.Y.Z`
5. Create release: `gh release create vX.Y.Z`

## Integration with Project

For AgriSense IoT project:
- Follow AGENTS.md conventions
- Use `dev` branch for development
- Create PRs to `dev` (not directly to `main`)
- Tag releases with semantic versioning
- Update firmware versions in all components
