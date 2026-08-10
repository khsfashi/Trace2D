# Security Policy

Trace2D is a public pre-release C++20 2D engine. Security reports are welcome, especially for issues that could affect users who build, run, import, package, or inspect untrusted project/content data.

## Supported versions

During the public-alpha period, security fixes target the latest `main` branch and the latest published pre-release unless a release note explicitly says otherwise. Backports to older alpha builds are not guaranteed.

| Version | Supported |
| --- | --- |
| latest `main` | Yes |
| latest published pre-release | Best effort |
| older pre-releases | No guaranteed backport |

## Reporting a vulnerability

**Do not publish exploit details, secrets, proof-of-concept payloads, or sensitive crash data in a public issue.**

Preferred route:

1. Open the repository's **Security** tab.
2. Use **Report a vulnerability / Private vulnerability reporting** when that GitHub feature is available.
3. Include the affected commit/tag, platform/toolchain, minimal reproduction, expected impact, and whether the issue requires crafted project/asset/input data.

If private vulnerability reporting is unavailable, open a minimal public issue that requests a private contact channel **without including vulnerability details**. The maintainer can then move the discussion to an appropriate private channel.

## Useful report contents

When possible, include:

- affected Trace2D commit/tag,
- OS, architecture, compiler and build preset,
- whether the issue is headless, renderer/GPU, import/parser, packaging/build, MCP/tooling, or sample specific,
- smallest reproducible input/project/asset that demonstrates the issue,
- sanitizer or debugger output if available,
- whether arbitrary code execution, memory corruption, path traversal, denial of service, information disclosure, or supply-chain impact is plausible,
- any workaround that avoids exploitation without destroying evidence.

Do not include unrelated private files, credentials, tokens, or raw AI conversation history.

## Security boundaries

Trace2D's deterministic/Agent-verifiable design improves observability but is **not** a security sandbox. Unless a later contract explicitly states otherwise:

- authored project/scene/asset files should be treated as input to validate, not as trusted code,
- external game C++ is native code with the privileges of the process that runs it,
- MCP/Agent/tooling endpoints are developer tooling and must not be exposed as an unauthenticated remote service,
- build scripts, compilers, package managers and third-party SDKs remain part of the host developer environment,
- screenshots/replays/structured diagnostics are evidence artifacts, not secret-storage mechanisms.

## Disclosure and fixes

For a confirmed vulnerability, the project will aim to:

1. reproduce and classify the issue,
2. preserve a regression test or other evidence where practical,
3. fix the affected supported branch/release path,
4. disclose enough information for users to assess impact after a fix or coordinated disclosure point exists,
5. update dependency/security guidance when the root cause is in the supply chain rather than Trace2D code.

Security tooling such as CodeQL and OpenSSF Scorecard is defense in depth. A green scanner or a numeric score is not a claim that Trace2D is vulnerability-free.