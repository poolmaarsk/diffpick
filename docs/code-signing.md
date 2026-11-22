# Code signing policy

Diffpick's Windows packages are currently unsigned. The project plans to apply
for free code signing from [SignPath.io](https://signpath.io/) with a certificate
provided by [SignPath Foundation](https://signpath.org/).

- Maintainer, reviewer, and release approver: Maroš Polák (GitHub: [poolmaarsk](https://github.com/poolmaarsk)).
- Signing requests will be approved manually and must come from the project's
  GitHub Actions build.
- Diffpick does not send comparison inputs, settings, or diagnostic logs to
  networked systems. It passes the chosen paths to the configured Meld command.
  Choosing a network path can access that location. Diagnostic logs are opt-in
  and stored locally.

## SignPath setup

1. Publish the MIT-licensed repository and its first unsigned release with the
   Windows ZIP built by GitHub Actions. Link this policy on the release page.
2. Enable multi-factor authentication for GitHub and SignPath. Apply through
   the [SignPath Foundation application](https://signpath.org/apply).
3. Once approved, connect the repository as a
   [GitHub trusted build system](https://docs.signpath.io/trusted-build-systems/github).
   Require verified origin and manual approval in the release signing policy.
4. Use [the artifact configuration](../packaging/windows/signpath.xml) with the
   existing `diffpick-windows-x64` Actions artifact. The outer ZIP is created by
   GitHub Actions and contains the portable ZIP. The only signing target is
   `diffpick.exe` inside the portable ZIP.
5. In GitHub, create a `signpath` environment restricted to the default
   branch. Add the environment secret `SIGNPATH_API_TOKEN` and environment
   variables `SIGNPATH_ORGANIZATION_ID`, `SIGNPATH_PROJECT_SLUG`,
   `SIGNPATH_SIGNING_POLICY_SLUG`, and `SIGNPATH_ARTIFACT_CONFIGURATION_SLUG`.
6. Run the CI workflow manually from the default branch with `sign_windows`
   enabled. CI passes the packaged executable's version to SignPath and
   verifies the returned timestamped signature before uploading the signed ZIP.
   Approve the request in SignPath when the build is ready for release.
