# `anl-ms3-v2.1.2`

Babeltrace **v2.1.2** plus the `src.ctf.lttng-archive` component class, which
reads a recording session's trace archives as the session writes them.

Base: [`v2.1.2`](https://github.com/efficios/babeltrace/releases/tag/v2.1.2)
(commit `fe2ecfdfd`), untouched below the commits listed here.

## What is on top of v2.1.2

- **`Add src.ctf.lttng-archive component class`** — Philippe Proulx, EfficiOS.
  Carried forward unchanged from `anl-ms3`; only its base changes on a rebase.

- **`Use LTTNGCTL_CFLAGS`** — Simon Marchi, EfficiOS. The build-system half of
  the component: it includes `<lttng/lttng.h>` to find the recording session,
  so the ctf plugin needs lttng-ctl's compile flags. Upstream has no reason to
  carry this, since it has no component that includes that header.

- **`Fix: ctf: crash reading a recording session's trace archives live`** —
  ANL. Three defects that made the component unusable against a live session:
  an uninitialised message-queue length that corrupted memory (silently, since
  its guard compiles out in release builds), an assertion on a data stream file
  the tracer had created but not yet written to, and a missing
  `get_supported_mip_versions` method that kept the component out of MIP 1
  graphs.

## Rebasing onto a newer release

    git rebase --onto v<new> v2.1.2 anl-ms3-v2.1.2

The component lives in its own `src/plugins/ctf/la-src/` directory and creates
no field classes of its own, so it rebases cleanly unless the shared CTF
message iterator changes shape.

Verify with the suite in THAPI's `archive-tests/`.
