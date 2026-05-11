# CI Smoke Test

This repo owns the GitLab CI job for the software-only Bridge -> Hub smoke test.

## Files

- pipeline: `.gitlab-ci.yml`
- executed harness: `Home Automation Bridge/tools/integration_smoketest.ps1`

## What the CI job does

1. checks out the Hub repo
2. clones the Bridge repo as a sibling directory named `Home Automation Bridge`
3. runs the Bridge smoke-test harness, which builds both projects and exercises the fake camera/audio pipeline
4. stores Bridge and Hub logs as artifacts

## Runner requirement

The pipeline is tagged `windows`.

Your GitLab project needs a Windows runner with:

- PowerShell
- CMake
- a MinGW toolchain compatible with the current presets
- Python 3

## Configuration knobs

The pipeline exposes these variables:

- `BRIDGE_REPO_URL`
- `BRIDGE_REPO_BRANCH`
- `BRIDGE_REPO_DIR`

If the Bridge repo moves or its default branch changes, update those values in `.gitlab-ci.yml`.
