# phytium-linux-kernel (Armbian mirror)

Automated **read-only mirror** of
[gitee.com/phytium_embedded/phytium-linux-kernel](https://gitee.com/phytium_embedded/phytium-linux-kernel),
so the Armbian build can fetch the Phytium kernel from GitHub instead of Gitee.

- The kernel source lives on the **[`linux-6.6`](../../tree/linux-6.6)** branch, mirrored verbatim from upstream.
- This **`armbian-mirror`** branch (the default) carries only this README and the sync workflow —
  it is *not* part of the kernel and must not be mirrored over.
- Synchronisation runs daily (and on demand) via
  [`.github/workflows/sync-mirror.yml`](.github/workflows/sync-mirror.yml); it force-updates
  `linux-6.6` to match upstream and never touches this branch.

Do not push changes here — upstream is Gitee.
