# Release Checklist

This checklist reflects the actions we follow before tagging a new ModbusCore
release. Automate steps where possible, but keep the list handy when running a
manual release.

## Pre-flight
- [ ] Confirm `CHANGELOG.md` has an entry for the release with correct date.
- [ ] Bump the `project()` version in the root `CMakeLists.txt` if necessary.
- [ ] Regenerate the version banner by configuring a clean build (`cmake --preset host-debug`).
- [ ] Verify all GitHub Actions workflows are green on the target branch.
- [ ] Check open issues/PRs for blockers and update milestones accordingly.

## Quality Gates
- [ ] Run `cmake --build --preset host-debug` followed by `ctest --preset all --output-on-failure`.
- [ ] Run the compat preset (`cmake --build --preset host-compat && ctest --preset compat --output-on-failure`).
- [ ] Execute hardware-in-the-loop smoke tests or capture the latest HIL report.
- [ ] Ensure examples compile with `-DMODBUS_PROFILE=TINY` and `-DMODBUS_PROFILE=FULL`.
- [ ] Capture footprint numbers (`cmake --build --preset host-footprint`).

## Packaging
- [ ] Build installable artifacts (`cmake --build --preset host-release --target install`).
- [ ] Regenerate documentation (`cmake --build --preset docs`).
- [ ] Upload artifacts to a draft GitHub release (libraries, headers, docs bundle).

## Announcement
- [ ] Push the tag (`git tag -a vX.Y.Z && git push origin vX.Y.Z`).
- [ ] Publish the GitHub release with changelog highlights.
- [ ] Notify downstream teams and update package indices if applicable (vcpkg/Conan/etc.).
