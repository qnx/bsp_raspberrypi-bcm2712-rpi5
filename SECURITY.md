# Security guidance

This BSP is experimental software and should be treated as a development starting point, not as a complete product security configuration. The default image has been hardened to avoid direct unauthenticated administration, but an integrator remains responsible for the device's boot trust, network policy, identities, service abilities, storage permissions, update process, and physical interfaces.

For the source-level findings addressed by the hardening changes, including vulnerable and corrected code excerpts, see the [security remediation report](docs/security-remediation-report.html).

## Security-sensitive defaults

The default `images/rpi5.build` configuration intentionally:

- does not include or launch `qconn`, `pdebug`, or `devc-pty`;
- does not attach a shell to the debug UART;
- locks the built-in `root` and `qnxuser` password fields;
- emits `/etc/shadow` with mode `0600`;
- exposes `/dev/fan` only to root.

These defaults remove development conveniences that otherwise cross an authentication boundary. Do not reverse them in a production image.

## Production deployment checklist

### Identity and remote access

1. Provision unique credentials or SSH authorized keys per device. Never place a reusable password hash in the buildfile.
2. Review the `sshd_config` imported from `${QNX_TARGET}`. Disable password authentication and direct root login unless a documented product requirement and compensating controls exist.
3. Store unique SSH host keys in protected persistent storage. The sample build still creates ephemeral keys in `/dev/shmem`; that behavior is unsuitable when clients must authenticate the device reliably.
4. Apply ingress filtering before enabling any management service.

### Development-only debugging

`qconn` is not an authenticated production administration protocol. If it is required on a physically and logically controlled lab network, create a separate development image, include the three remote-debugging binaries, and launch `qconn -a -l` only behind an authenticated SSH tunnel. The `-a` option retains address-space randomization and `-l` limits the listener to local connections.

Keep development images, signing keys, and production images distinguishable. A runtime flag that silently turns a production image into a debugging image is not recommended.

### Boot trust

The Raspberry Pi firmware configuration, command line, DTB, IFS, and recovery/update path must be authenticated as one policy. Startup options can intentionally affect physical memory, MMIO, debugging, and CPU mitigations; an authenticated kernel with unauthenticated boot arguments is therefore not a complete trust boundary.

Use Raspberry Pi 5 secure boot with an organization-controlled key, sign every mutable boot artifact, and allowlist only the startup options needed by the product. Protect rollback state when the product threat model requires anti-rollback.

### Least privilege and device access

1. Add a QNX security policy that gives each service a distinct type and only the abilities it needs.
2. Restrict raw SDMMC and I2C resource-manager paths to trusted administrative services. Read-only access is not a substitute for an administrative policy for raw command interfaces.
3. Keep thermal control restricted and enforce a product-appropriate cooling failsafe above the resource manager.
4. Review all setuid utilities in the final IFS and remove those the product does not use.
5. Do not mount or create `/scripts/board_startup.sh` from attacker-writable storage; the customization hook runs with startup privileges.

### Temporary and persistent storage

The sample aliases `/tmp` and `/var/log` to `/dev/shmem`. For production, replace that arrangement with bounded storage or enforce product-specific quotas so an unprivileged process cannot consume all system memory. Apply appropriate owner, group, and sticky-bit modes at creation time.

Set a restrictive creation mask in each service's own launch context or wrapper when its files are not created with explicit modes. A standalone `umask` command in the mkifs startup script is not a global control: a mask belongs to the invoking process and is inherited only by that process's descendants.

Persist logs, host identity, account data, and security state only where confidentiality, integrity, wear, and recovery behavior meet the product requirements.

### Supply chain

Record the exact resolved QNX package versions, firmware revision, hashes, compiler/tool versions, and configuration used for every release. Generate an SBOM and build in an isolated environment with a sanitized `PATH` and trusted `QCONFIG`/template paths.

## Validation required before release

The repository cannot validate a product-specific security policy or imported binary behavior. A release should, at minimum:

- build all modified targets with the supported QNX SDP and warning policy;
- boot on Raspberry Pi 5 and exercise watchdog, serial, fan, I2C, SD/eMMC, and ASSD/RPMB paths in scope;
- verify that TCP port 8000 is closed and no unauthenticated console shell is available;
- verify account lockout, SSH policy, persistent host identity, and final filesystem modes;
- fuzz or boundary-test SDMMC devctls, FDT properties, mailbox replies, and launch-option parsers;
- test malformed, absent, and delayed firmware/peripheral responses; and
- inspect the generated IFS rather than relying only on the buildfile.

The code comments added at input-validation points document why the checks are security boundaries and should be retained with future refactoring.
