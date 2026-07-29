# FCCU fault 66 (NOC SSI parity) on i.MX95 A0 during NOC power-up

Escalation summary for NXP. Prepared from the `miketsukerman/imx-sm` fork of
`nxp-imx/imx-sm`.

## Summary

On an Advantech AOM5521 board carrying **i.MX95 A0** silicon, the System Manager
enters a reset loop during its own init. Every reset is an FCCU reaction to
fault **66** (`DEV_SM_FAULT_NOC_SSI`, "parity fault from SSIs in NOC").

Reported reset record (diagnostics added locally, see *Diagnostics* below):

```
Reset request: reason=fccu, errId=66
  extInfo[0] = 0x00000009   boot stage 9  = DEV_SM_BOOT_STAGE_PWRUP
  extInfo[1] = 0x00000013   power domain 19 = DEV_SM_PD_NOC
  extInfo[2] = 0x00000001   DEV_SM_SiVerGet() = DEV_SM_SIVER_A1 (Rev A)
```

The fault therefore fires inside the `DEV_SM_Init()` power-up loop while the
SM is servicing `DEV_SM_PD_NOC`. Note that NOC is **already powered** when the
SM starts, so no SRC power transition takes place: the loop calls
`DEV_SM_PowerStateGet()` and then `DEV_SM_PowerUpPost(19)` →
`DEV_SM_NocConfigLoad()` directly. Silicon revision detection is correct.

The reset loop is fully deterministic — DDR OEI and TCM OEI complete with
`err = 0` on every iteration, and the SM then resets with the identical record
before printing its banner.

## When it started

The fault is not reproducible on firmware built before commit
`21ca4f727c58550f1cf6fd34232b535864736fc2` — **SM-378 "Enable mission and parity
faults"** (authored 2026-02-03, committed 2026-04-09).

`21ca4f7` changes the `CVfccuCfg` "Faults Enabled" bitmap in
`components/SAF/devices/MIMX95/src/eMcem_Cfg.c`:

```c
      (uint32)0xFFFC000BUL,
-     (uint32)0x00000003UL,   /* faults 32-63 */
-     (uint32)0x00000000UL    /* faults 64-95 */
+     (uint32)0x7C000003UL,
+     (uint32)0x0000003FUL
```

Word[2] `0x00000000 → 0x0000003F` arms the whole SSI parity group for the first
time: 64 `AON_SSI`, 65 `WAKE_SSI`, **66 `NOC_SSI`**, 67 `M7_SSI`, 68 `DDR_SSI`,
69 `NPU_SSI`. (Bitmap convention — word[k] bit N enables fault 32k+N — verified
against `57ae3c8` / SM-413, which disables fault 61 by clearing word[1] bit 29.)

Branch ancestry on this fork:

| Branch                 | Contains `21ca4f7` | Behaviour on AOM5521 A0 |
|------------------------|--------------------|-------------------------|
| `adv-lf-6.18.2-1.0.0`  | no                 | boots                   |
| `adv-lf-6.18.20-2.0.0` | yes                | fault-66 reset loop     |

The same commit also adds NOCMIX NIU-timeout register writes to
`DEV_SM_NocConfigLoad()` in `devices/MIMX95/sm/dev_sm_config.c`:

```c
static const uint32_t s_timeoutData[] = {
    SM_CFG_FN(0x000000c0U, 12U), 0x7U,
    SM_CFG_FN(0x000000c0U, 12U), 0x8007U,
    SM_CFG_END
};
status = CONFIG_Load((const uint32_t*) BLK_CTRL_NOCMIX_BASE, s_timeoutData);
```

so SM-378 carries **two** candidate mechanisms, executing in exactly the stage
and domain where the fault is reported.

## The two candidate mechanisms

- **(A) Unmasking only.** The NOC SSI parity condition already existed on this
  board during NOC power-up and was silently masked before SM-378. Implies a
  real, previously hidden bus-integrity condition on A0, not a software
  regression.
- **(B) Active generation.** The NIU-timeout writes added by the same commit
  create the parity condition on A0. Note what those writes do: `SM_CFG_FN(0xC0,
  12)` fills all twelve `BLK_CTRL_NOCMIX` `NIU_TO_CTRL_*` registers (offsets
  0xC0–0xEC, one per target MIX: WAKEUP, CORTEXA, GIC700, NPU, GPU, CAMERA,
  DISPLAY_RT, NETC, MMU700, HSIO, DISPLAY_BE, VPU) with `CLK_DIV_RATIO = 7`,
  then repeats the fill with `0x8007`, i.e. pulsing `UPD` (bit 15) to commit the
  new ratio. Several of those target MIXes (NPU, GPU, CAMERA, DISPLAY, VPU,
  NETC, HSIO) are unpowered at this point in SM init. Whether committing a
  timeout ratio toward an unpowered MIX can raise an SSI parity error is A0
  register-level behaviour we cannot determine from the source.

## Hypotheses already eliminated

| Hypothesis | Result |
|---|---|
| MIX-level SSI transaction blocking skipped on Rev A in `DEV_SM_PowerStateSet()` | **Falsified, and inapplicable by construction.** Restoring the blocking on Rev A reproduced the fault with an identical signature. Two code-level reasons it could never have mattered: (1) at boot stage 9 the `DEV_SM_Init()` loop calls only `DEV_SM_PowerStateGet()` and, for an already-on domain, `DEV_SM_PowerUpPost()` — `DEV_SM_PowerStateSet()` is not on the path, and NOC is already powered when the SM starts; (2) on i.MX95 `PWR_MixSsiBlockingSet()`/`PWR_MixSsiBlockingUpdate()` only act on `PWR_MIX_SLICE_IDX_GPU` and are no-ops for every other MIX including NOC. |
| Board TRDC / BLK_CTRL configuration delta | **Falsified.** `config_trdc.h` and `config_bctrl.h` deltas are byte-identical between the working and broken board commits. |
| Silicon revision misdetection | **Falsified.** `DEV_SM_SiVerGet()` reports Rev A correctly (`extInfo[2] = 1`). |

## Experiments

### Experiment 1 — arm only fault 66 on the last known-good firmware

Branch `exp/fccu66-arm-only-on-working`, based on `adv-lf-6.18.2-1.0.0`
(`0138e429`). Single change, `components/SAF/devices/MIMX95/src/eMcem_Cfg.c`,
`CVfccuCfg` "Faults Enabled" word[2] `0x00000000UL` → `0x00000004UL` (bit 2 =
fault 64+2 = 66). Builds clean (`make config=mx95evk`).

- **Faults with `errId=66`** → mechanism (A): the condition pre-dates the entire
  Feb→Jun delta.
- **Boots cleanly** → mechanism (B): something in that window creates it.

> **Result: PENDING.** This experiment requires the AOM5521 A0 board; it could
> not be executed in the analysis environment. Fill in before filing.

### Experiment 2 — remove only the NIU-timeout writes (run only if 1 boots)

Branch `exp/fccu66-revert-niu-timeout`, based on `adv-lf-6.18.20-2.0.0`
(`e0d4afa`). Removes only the `s_timeoutData` / `CONFIG_Load` hunk that
`21ca4f7` added to `DEV_SM_NocConfigLoad()`; word[2] stays `0x0000003F` so fault
66 remains armed. Builds clean.

- **Boots** → the NIU-timeout writes generate the parity error (regression, local
  fix possible).
- **Still faults** → the enable word is the whole story; fall back to (A).

> **Result: PENDING** (depends on experiment 1).

Experiment 2 is also available as a build option on
`copilot/fix-fccu-fault-66-reset-loop` without switching branches:
`make config=mx95evk NOC_NIU_TIMEOUT=0` compiles out the same
`s_timeoutData`/`CONFIG_Load` hunk while leaving the FCCU enable word at
`0x0000003F`.

## Diagnostics used

Added locally on `copilot/fix-fccu-fault-66-reset-loop`, behind `SM_FAULT_DIAG`
(`FAULT_DIAG=1`, requires `DEBUG=1`):

- `g_bootStage` / `g_bootStageDomain` tracking through `DEV_SM_Init()` and
  `DEV_SM_PowerStateSet()`.
- For faults 64–69, the reset record reports boot stage, power domain and
  silicon version as `extInfo[0..2]` (the SSI parity faults carry no hardware
  syndrome).
- Interrupt-safe polled UART output plus a fault-storm counter, so faults can be
  reported without applying the configured reaction.

## Questions for NXP

1. Does **ERR053263** — or another A0/A1 erratum — apply to the SSI parity fault
   group (64–69) in the same way it does to fault 61 (`DEV_SM_FAULT_M33_AXBS`),
   which SM-413 (`57ae3c8`) disables outright?
2. Should SM-378's `CVfccuCfg` word[2] value be revised for Rev A silicon (e.g.
   the SSI parity group left masked on A0/A1, as fault 61 is masked on all
   revisions)?
3. Are the NOCMIX NIU-timeout writes added by SM-378
   (`BLK_CTRL_NOCMIX + 0xC0`, 12 registers, values `0x7` then `0x8007`) valid on
   A0 silicon, and is the write ordering relative to NOC power-up / TRDC-N load
   correct there?
   In particular: is it legal to pulse `NIU_TO_CTRL_<MIX>.UPD` for a MIX that is
   currently powered down, or must those registers only be programmed for
   powered MIXes (for example from the per-MIX `*ConfigLoad()` after that MIX is
   powered up)?
4. Was SM-378 validated on A0/A1 silicon, or on B0 only?

## Caveats

- Whether the NIU-timeout writes can themselves raise an SSI parity error, and
  whether the SSI parity group is functional on A0, are **register-level A0
  behaviours not derivable from this repository**. They are stated here as open
  questions, not conclusions.
- Several `Rev A does not support ...` comments in `dev_sm_power.c`,
  `dev_sm_config.c` and `dev_sm_system.c` in this fork originate from local
  commits written without access to the i.MX95 A0 reference manual or errata.
  The corresponding behaviour is unchanged from upstream; the claims themselves
  are not independently verified.

## Appendix — exact experiment diffs

### Experiment 1

```diff
--- a/components/SAF/devices/MIMX95/src/eMcem_Cfg.c
+++ b/components/SAF/devices/MIMX95/src/eMcem_Cfg.c
@@ -198,7 +198,10 @@ const eMcem_CVfccuInstanceCfgType CVfccuCfg =
         { /*!< Faults Enabled */
             (uint32)0xFFFC000BUL,
             (uint32)0x00000003UL,
-            (uint32)0x00000000UL
+            /* DIAGNOSTIC ONLY: arm fault 66 (DEV_SM_FAULT_NOC_SSI, bit 2 of
+               word[2] = fault 64+2) and nothing else. Baseline value is
+               0x00000000UL. Do not merge. */
+            (uint32)0x00000004UL
         },
```

### Experiment 2

```diff
--- a/devices/MIMX95/sm/dev_sm_config.c
+++ b/devices/MIMX95/sm/dev_sm_config.c
@@ -428,12 +428,6 @@ int32_t DEV_SM_NocConfigLoad(void)
     static const uint32_t s_configData[] = SM_NOC_CONFIG;
-    static const uint32_t s_timeoutData[] =
-    {
-        SM_CFG_FN(0x000000c0U, 12U), 0x7U,
-        SM_CFG_FN(0x000000c0U, 12U), 0x8007U,
-        SM_CFG_END
-    };
@@ -475,12 +469,9 @@ int32_t DEV_SM_NocConfigLoad(void)
-    /* Initialize the NIU timeout registers to maximum value. */
-    if (status == SM_ERR_SUCCESS)
-    {
-        status = CONFIG_Load((const uint32_t*) BLK_CTRL_NOCMIX_BASE,
-            s_timeoutData);
-    }
+    /* DIAGNOSTIC ONLY: the NOCMIX NIU timeout writes added by SM-378
+       (21ca4f7) are removed here to test whether they generate the
+       FCCU fault 66 (NOC SSI parity) seen on i.MX95 A0. Do not merge. */
```
