# Security Policy

## Supported versions

Security fixes are developed on the private canonical `main` branch and included
in the next reviewed public release. The newest public release is supported;
older releases receive no separate security backports unless a release notice
states otherwise.

| Version | Supported |
| --- | --- |
| Latest public release | Yes |
| Older releases | No |
| Unreleased private builds | Development only |

## Report a vulnerability privately

Do **not** open a public issue for a suspected vulnerability or include exploit
details, private data, credentials, or affected-user information in public
comments.

Use GitHub's private vulnerability-reporting form:

<https://github.com/DeviousVon/NetStats-Live/security/advisories/new>

Include:

- the affected version or commit;
- the Linux distribution, desktop, and display protocol;
- concise reproduction steps;
- expected and observed behavior;
- impact and attack prerequisites;
- logs or a minimal proof of concept with secrets and personal data removed.

If the private form is unavailable, submit the report through the CERT/CC
[VINCE vulnerability coordination platform](https://kb.cert.org/vince/),
identify this project and repository, and request private vendor coordination.
Do not publish vulnerability details in an issue while coordination is pending.

## Response targets

These are targets, not a guarantee:

- acknowledge a complete report within 7 calendar days;
- provide an initial assessment or request for more evidence within 14 days;
- coordinate disclosure after a fix or mitigation is available;
- credit the reporter when requested and legally possible.

## Scope

Security-relevant areas include settings and monthly-total persistence,
autostart file transactions, single-instance DBus authority, subprocess and
network-probe handling, clipboard/Klipper integration, package and desktop-file
installation, and release/publication integrity.

Reports about expected desktop limitations, unsupported environments, or
ordinary feature requests belong in the public issue tracker only when they do
not disclose a vulnerability.
