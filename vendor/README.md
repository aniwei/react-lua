# React Main Mirror

This directory hosts the checked-out upstream `react-main` sources that serve as the
single source of truth for the C++ 1:1 translation. The build and translation
scripts look for the mirror at `vendor/react-main/` (falling back to the legacy
`react-main/` path if needed).

## Expected layout
```
vendor/
  react-main/          # git subtree / submodule / mirror of facebook/react
```

The repository currently uses a lightweight symlink that points to the existing
`react-main/` checkout. When promoting this to CI, replace the symlink with a
proper git submodule or mirror clone.

## Recommended workflows
- **Submodule**
  ```bash
  git submodule add https://github.com/facebook/react.git vendor/react-main
  git submodule update --init --recursive
  ```
- **Mirror clone (local only)**
  ```bash
  git clone https://github.com/facebook/react.git vendor/react-main
  ```
- **Updating to a new commit**
  ```bash
  cd vendor/react-main
  git fetch origin
  git checkout <desired-commit>
  cd ../..
  git add vendor/react-main
  git commit -m "chore: bump react-main mirror"
  ```

Keep the mirror pinned to a known commit to guarantee deterministic translation
results and stable parity reports.
