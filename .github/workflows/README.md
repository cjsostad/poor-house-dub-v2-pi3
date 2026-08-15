# GitHub Actions Workflows

## Test Suite Workflow

The `test.yml` workflow automatically runs the test suite on every push and pull request.

### Triggers

The workflow runs on:
- **Push** to `main` or `develop` branches
- **Pull Requests** targeting `main` or `develop` branches
- **Manual trigger** via GitHub Actions UI (workflow_dispatch)

### Jobs

#### 1. Test Job
Runs the full test suite across multiple Node.js versions.

**Matrix Strategy:**
- Node.js 18.x
- Node.js 20.x

**Steps:**
1. ✅ Checkout code
2. ✅ Setup Node.js with caching
3. ✅ Install dependencies (`npm ci`)
4. ✅ Run tests (`npm test`)
5. ✅ Generate coverage report
6. ✅ Upload coverage to Codecov (optional)
7. ✅ Upload coverage artifact (30-day retention)
8. ✅ Comment test results on PR (Node 20.x only)

#### 2. Lint Job
Basic code quality checks.

**Steps:**
1. ✅ Checkout code
2. ✅ Setup Node.js
3. ✅ Install dependencies
4. ✅ Syntax validation

#### 3. Test Status Check
Final status check that requires both test and lint jobs to pass.

### Coverage Reports

Coverage reports are:
- Generated on every run
- Uploaded as artifacts (available for 30 days)
- Optionally sent to Codecov
- Summarized in PR comments

### PR Comments

The workflow automatically comments on PRs with:
- ✅ Test status
- 📊 Coverage summary table
- 🤖 Node.js version used

Example comment:
```markdown
## ✅ Test Suite Passed

All tests completed successfully!

### Coverage Summary

| Metric | Coverage |
|--------|----------|
| Statements | 85.2% |
| Branches | 78.6% |
| Functions | 91.4% |
| Lines | 85.9% |

---
🤖 Automated test run on Node.js 20.x
```

### Status Badge

Add this badge to your README.md to show test status:

```markdown
[![Test Suite](https://github.com/parkredding/poor-house-dub-v2/actions/workflows/test.yml/badge.svg)](https://github.com/parkredding/poor-house-dub-v2/actions/workflows/test.yml)
```

Result: 
[![Test Suite](https://github.com/parkredding/poor-house-dub-v2/actions/workflows/test.yml/badge.svg)](https://github.com/parkredding/poor-house-dub-v2/actions/workflows/test.yml)

### Viewing Results

1. **On GitHub:**
   - Go to the "Actions" tab in your repository
   - Click on a workflow run to see details
   - Download coverage artifacts if needed

2. **On Pull Requests:**
   - PR checks section shows pass/fail status
   - Automated comment shows coverage details
   - Click "Details" to see full logs

3. **Coverage Report:**
   - Download the coverage artifact
   - Extract and open `coverage/lcov-report/index.html`
   - View detailed line-by-line coverage

### Local Testing

To test the same way CI does:

```bash
# Clean install (like CI)
npm ci

# Run tests
npm test

# Generate coverage
npm run test:coverage
```

### Troubleshooting

**If tests fail in CI but pass locally:**
1. Ensure you're using `npm ci` instead of `npm install`
2. Check Node.js version compatibility
3. Look for timing-dependent test failures
4. Check for missing environment variables

**If coverage upload fails:**
- This is expected if Codecov isn't configured
- The step is set to `continue-on-error: true`
- Coverage artifacts are still uploaded

**If PR comments don't appear:**
- Check workflow permissions in repo settings
- Ensure `GITHUB_TOKEN` has write access
- Look for errors in the GitHub Script step

### Customization

#### Change Node.js Versions
Edit the matrix in `test.yml`:
```yaml
strategy:
  matrix:
    node-version: [18.x, 20.x, 22.x]  # Add/remove versions
```

#### Change Trigger Branches
Edit the `on` section:
```yaml
on:
  push:
    branches: [ main, develop, staging ]  # Add branches
  pull_request:
    branches: [ main ]
```

#### Disable Coverage Comments
Remove or comment out the "Comment Test Results on PR" step.

#### Add Slack Notifications
Add a step after tests:
```yaml
- name: Notify Slack
  uses: 8398a7/action-slack@v3
  if: failure()
  with:
    status: ${{ job.status }}
    webhook_url: ${{ secrets.SLACK_WEBHOOK }}
```

### Required Secrets (Optional)

For Codecov integration:
- `CODECOV_TOKEN` - Get from codecov.io

For Slack notifications:
- `SLACK_WEBHOOK` - Your Slack webhook URL

### Performance

Typical run times:
- **Test Job**: ~2-3 minutes per Node.js version
- **Lint Job**: ~1 minute
- **Total**: ~5-7 minutes (parallel execution)

### Best Practices

✅ **Do:**
- Keep tests fast (under 5 minutes total)
- Use `npm ci` for consistent installs
- Cache dependencies (already configured)
- Test on multiple Node.js versions
- Upload coverage reports

❌ **Don't:**
- Commit `node_modules/`
- Skip tests in CI
- Ignore failing tests
- Remove coverage reports

### Next Steps

1. **Push this workflow** to your repository
2. **Make a test PR** to see it in action
3. **Add the badge** to your README.md
4. **Set up Codecov** (optional) for coverage tracking
5. **Configure branch protection** to require passing tests

### Branch Protection Rules

Recommended settings:
1. Go to Settings → Branches → Add rule
2. Branch name pattern: `main`
3. Enable: "Require status checks to pass before merging"
4. Select: "Test Suite / Run Tests"
5. Enable: "Require branches to be up to date before merging"

This ensures all PRs pass tests before merging! 🚀
