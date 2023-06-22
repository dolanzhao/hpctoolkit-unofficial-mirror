import argparse
import collections.abc
import contextlib
import os
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import tarfile
import textwrap
import time
from pathlib import Path

from .action import Action, SummaryResult, action_sequence
from .buildsys import Build, CheckInstallManifest, FreshTestData, Install, Setup, Test
from .configuration import (
    Compilers,
    ConcreteSpecification,
    Configuration,
    Specification,
    UnsatisfiableSpecError,
)
from .logs import FgColor, colorize, print_header, section

actions: dict[str, tuple[type[Action], ...]] = {
    "setup": (Setup,),
    "build": (Build,),
    "install": (Install,),
    "check-install": (CheckInstallManifest,),
    "test": (Test,),
}
for suite, cls in FreshTestData.suites.items():
    actions[f"fresh-testdata-{suite}"] = (cls,)


def parse_spec(spec: str) -> Specification:
    try:
        return Specification(spec)
    except ValueError as e:
        raise argparse.ArgumentTypeError(f"invalid spec '{spec}': {e}") from None


def parse_spec_unstrict(spec: str) -> Specification:
    try:
        return Specification(spec, strict=False)
    except ValueError as e:
        raise argparse.ArgumentTypeError(f"invalid spec '{spec}': {e}") from None


def build_parser() -> argparse.ArgumentParser:
    def action(act: str) -> str:
        if act not in actions:
            print(f"Invalid action {act}, must be one of {', '.join(actions)}")
            raise ValueError()
        return act

    parser = argparse.ArgumentParser(
        description="Build front-end for HPCToolkit('s CI)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
This command sweeps the space of build configurations that satisfy any of the given SPECs.
Each build configuration is represented by a series of boolean-valued "variants," with true/false
being represented by `+` and `~` (like Spack spec syntax). The available variants are:
    +mpi:  Enable Prof-MPI support
    +debug:  Enable extra debug flags
    +papi:  Enable PAPI support
    +python:  Enable Python logical unwinding support
    +opencl:  Enable OpenCL support
    +cuda:  Enable Nvidia CUDA support
    +rocm:  Enable AMD ROCm support
    +level0:  Enable Intel Level Zero support
    +gtpin:  Enable Intel GTPin for instrumentation (requires +level0)
Each SPEC constrains the allowed configurations via a series of "clauses" of the form:
    +VARIANT:  The given VARIANT is enabled
    ~VARIANT:  The given VARIANT is disabled
    (VARIANTS...)[CONDITIONS...]: The listed VARIANTS satisfy all the CONDITIONS, of the form:
        +>N:  At least N of the listed VARIANTS are enabled
        +<N:  At most N of the listed VARIANTS are enabled
        +=N:  Exactly N of the listed VARIANTS are enabled
        ~>N, ~<N, ~=N: Same as above but for disabled
    !VARIANT:  The given VARIANT is explicitly unconstrined. This must be used to list any
               unconstrained VARIANTS, otherwise the SPEC is invalid.

The output from this program is summarized to only include warnings or errors, and is wrapped in
collapsible sections to allow for easy viewing from the GitLab UI.

Examples:
 - Build all non-MPI builds, using the latest version of dependencies
        ./dev buildfe -a build -s '~mpi' ci/dependencies/latest/

 - Build and install a single specific spec, keeping the install around afterwards
        ./dev buildfe -a install -ks '+mpi ~debug ...' ci/dependencies/latest/
""",
    )
    parser.add_argument(
        "depsenv",
        type=Path,
        help="Environment containing dependencies, as generated by ./dev setup",
    )
    parser.add_argument(
        "-C", "--configure-arg", action="append", help="Additional argument to pass to configure"
    )
    parser.add_argument(
        "-l", "--logs-dir", type=Path, help="Top-level directory to save log outputs to"
    )
    parser.add_argument(
        "-f",
        "--log-format",
        default="{0}",
        help="Format-string to use to generate the log output subdirectory",
    )
    parser.add_argument(
        "-a",
        "--action",
        required=True,
        type=action,
        action="append",
        help="Add an action to the of actions done by this sequence",
    )
    parser.add_argument(
        "-s",
        "--spec",
        metavar="SPEC",
        type=parse_spec,
        action="append",
        help="Only build variants matching this spec. Can be repeated to union specs",
    )
    parser.add_argument(
        "-u",
        "--unsatisfiable",
        metavar="SPEC",
        type=parse_spec_unstrict,
        action="append",
        help="Mark variants matching this spec as known to be unsatisfiable",
    )
    parser.add_argument(
        "-1",
        "--single-spec",
        action="store_true",
        default=False,
        help="Only build one spec, error if more than one matches",
    )
    parser.add_argument(
        "-k",
        "--keep",
        action="store_true",
        default=False,
        help="Keep the build and install directories around after the command completes. Implies --single-spec",
    )
    parser.add_argument(
        "-n",
        "--dry-run",
        action="store_true",
        default=False,
        help="Don't build, instead print the configuration space and action sequence",
    )
    parser.add_argument(
        "--ccache-stats",
        default=False,
        action="store_true",
        help="Report per-build ccache statistics. Note that this clears the ccache statistics",
    )
    parser.add_argument(
        "--fail-fast", action="store_true", help="Stop on the first failing configuration"
    )
    parser.add_argument(
        "--reproducible",
        action="store_true",
        help="Output reproduction instructions in the log",
    )
    parser.add_argument(
        "--reproduction-cmd",
        action="append",
        default=[],
        help="Add the given command to the reproduction instructions",
    )
    parser.add_argument(
        "--reproduction-volume",
        action="append",
        default=[],
        help="Add the given volume to the Podman command in the reproduction instructions",
    )
    parser.add_argument(
        "-c",
        "--compiler",
        help="configure: Use the given compiler instead of the system default",
    )
    parser.add_argument(
        "--fresh-unpack",
        default=False,
        action="store_true",
        help="fresh-testdata-*: Unpack the generated test data to the source directory",
    )
    parser.add_argument(
        "--test-junit-copyout",
        default=False,
        action="store_true",
        help="test: Copy JUnit XML results to the current directory",
    )
    parser.add_argument(
        "--test-opt",
        action="append",
        default=[],
        help="test: Pass the given argument to meson test",
    )
    return parser


def post_parse(args):
    Test().junit_copyout = args.test_junit_copyout
    Test().meson_opts = args.test_opt
    if args.keep or args.fresh_unpack or args.test_junit_copyout:
        args.single_spec = True
    args.action = action_sequence([act() for name in args.action for act in actions[name]])
    return args


def gen_variants(args) -> list[tuple[ConcreteSpecification, bool]]:
    result = [
        (cs, any(u.satisfies(cs) for u in (args.unsatisfiable or [])))
        for cs in ConcreteSpecification.all_possible()
        if not args.spec or any(s.satisfies(cs) for s in args.spec)
    ]
    assert result
    if args.single_spec and len(result) > 1:
        print("Multiple spec match but -1/--single-spec given, refine your parameters!")
        print("Specs that matched:")
        for cs, _ in result:
            print("  " + str(cs))
        sys.exit(2)
    return result


def configure(
    meson: Path, cargs: list[str], comp: Compilers, v: ConcreteSpecification, unsat: bool
) -> Configuration | None:
    try:
        cfg = Configuration(meson, cargs, comp, v)
    except UnsatisfiableSpecError as e:
        if not unsat:
            with colorize(FgColor.error):
                print(
                    f"## HPCToolkit {str(v)} is unsatisfiable, missing {e.missing}",
                    flush=True,
                )
        return None

    if unsat:
        with colorize(FgColor.error):
            print(
                f"## HPCToolkit {str(v)} was incorrectly marked as unsatisfiable",
                flush=True,
            )
    return cfg


def parse_stats(stdout):
    stats = {}
    for line in stdout.splitlines():
        parts = re.split(" {3,}|\t+", line)
        if len(parts) == 2:
            key, val = parts
            with contextlib.suppress(ValueError):
                stats[key] = int(val)
    return stats


def safe_div(a: int, b: int) -> float:
    if a == 0:
        return float("nan") if b == 0 else -float("inf")
    if b == 0:
        return float("inf")
    return a / b


def print_ccache_stats(header_prefix: str):
    if (ccache := shutil.which("ccache")) is None:
        return

    try:
        proc = subprocess.run(
            [ccache, "--print-stats"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            check=True,
        )
    except subprocess.CalledProcessError:
        # Older ccache doesn't support the machine-readable --print-stats, so just skip
        return

    stats = parse_stats(proc.stdout)
    d_hit, p_hit, miss = (
        stats["direct_cache_hit"],
        stats["preprocessed_cache_hit"],
        stats["cache_miss"],
    )
    total_hit = d_hit + p_hit
    total = total_hit + miss

    with section(
        header_prefix + f"Ccache statistics: {safe_div(total_hit, total)*100:.3f}%",
        color=FgColor.info,
        collapsed=True,
    ):
        print(f"Hit: {safe_div(total_hit, total)*100:.3f}% ({total_hit:d} of {total:d})")
        print(f"  Direct: {safe_div(d_hit, total_hit)*100:.3f}% ({d_hit:d} of {total_hit:d})")
        print(f"  Preprocessed: {safe_div(p_hit, total_hit)*100:.3f}% ({p_hit:d} of {total_hit:d})")
        print(f"Missed: {safe_div(miss, total)*100:.3f}% ({miss:d} of {total:d})")


def build(
    idx: int, t: int, variant: ConcreteSpecification, cfg: Configuration, args
) -> tuple[bool, dict]:
    # pylint: disable=too-many-locals

    if args.ccache_stats and (ccache := shutil.which("ccache")) is not None:
        subprocess.run([ccache, "--zero-stats"], stdout=subprocess.DEVNULL, check=True)

    print_header(f"[{idx:d}/{t:d}] ## Building for configuration " + str(variant) + " ...")

    srcdir = Path().absolute()
    logdir = None
    if args.logs_dir:
        logdir = args.logs_dir / args.log_format.format(variant.without_spaces)
        logdir.mkdir(parents=True)
        logdir = logdir.absolute()
    builddir = srcdir / "ci" / "build"
    installdir = srcdir / "ci" / "install"

    summary = SummaryResult()
    times = {}
    ok = True
    try:
        for i, a in enumerate(args.action):
            with section(
                f"  [{i+1}/{len(args.action)}] {a.header(cfg)}...",
                color=FgColor.header,
                collapsed=True,
            ):
                starttim = time.monotonic()
                r = a.run(
                    cfg, builddir=builddir, srcdir=srcdir, installdir=installdir, logdir=logdir
                )
                endtim = time.monotonic()
                tim = endtim - starttim
                print(f"  [{i+1}/{len(args.action)}] {a.name()} took {tim:.3f} seconds")
            with colorize(r.color()):
                print(f"  [{i+1}/{len(args.action)}] {r.summary()}")

            summary.add(a, r)
            times[a] = tim
            if not r.passed:
                ok = False
            if not r.completed:
                ok = False
                break
        else:
            if args.fresh_unpack:
                for a in args.action:
                    if any(isinstance(a, c) for c in FreshTestData.suites.values()):
                        with tarfile.open(builddir / a.tarball_path) as tarf:
                            tarf.extractall(srcdir)
    finally:
        if not args.keep:
            if builddir.exists():
                shutil.rmtree(builddir)
            if installdir.exists():
                shutil.rmtree(installdir)

    if args.ccache_stats:
        print_ccache_stats(f"  [-/{len(args.action)}] ")

    if args.reproducible:
        with (
            section("  Reproduction instructions", color=FgColor.header, collapsed=True)
            if summary.passed
            else contextlib.nullcontext()
        ):
            print(
                "To reproduce the above build, run the following commands from your local HPCToolkit checkout:"
            )
            print(textwrap.indent(reproduction_cmd(variant, args), "    "))
            if "CI_JOB_IMAGE" in os.environ:
                print(
                    "Note that the above uses container tags which may change. See top of job log for exact image digest used."
                )

    with colorize(summary.color()):
        print(f"[{summary.icon()}] {summary.summary()}")

    return ok, times


def print_stats(times: list[float], prefix: str = ""):
    if len(times) == 1:
        print(f"{prefix}Time: {times[0]:.3f} seconds")
    else:
        print(
            f"{prefix}Time: {statistics.mean(times):.3f} +- {statistics.stdev(times):.1f} seconds"
        )
        qtls = statistics.quantiles(times, n=5)
        print(f"{prefix}      20% < {qtls[0]:.3f} s | {qtls[3]:.3f} s < 20%")


def reproduction_cmd(variant: ConcreteSpecification, args) -> str:
    chev = "$"
    lines = []
    if "CI_JOB_IMAGE" in os.environ:
        podargs = [
            "--rm",
            "-it",
            "-v",
            "./:/hpctoolkit",
            "-v",
            "/hpctoolkit/.devenv",
            "--workdir",
            "/hpctoolkit",
        ]
        for vol in args.reproduction_volume:
            podargs.append("-v")
            podargs.append(vol)
        podargs.append(shlex.quote(os.environ["CI_JOB_IMAGE"]))
        lines.append(f"{chev} podman run {' '.join(podargs)}")
        chev = "#"

    for cmd in args.reproduction_cmd:
        lines.append(chev + " " + shlex.join(shlex.split(cmd)))

    feargs: list[str] = []
    if args.compiler:
        feargs.append("-c")
        feargs.append(args.compiler)
    if args.configure_arg:
        for arg in args.configure_arg:
            feargs.append("-C" + arg)
    feargs.append("-1s")
    feargs.append(str(variant))
    lines.append(f"{chev} ./dev populate {shlex.quote(str(args.depsenv))}")
    lines.append(
        f"{chev} ./dev buildfe -d {shlex.quote(str(args.depsenv))} -- {shlex.join(feargs)}"
    )
    return "\n".join(lines)


def main(meson: Path, in_args: collections.abc.Sequence[str] | None = None):
    args = post_parse(build_parser().parse_args(in_args))

    variants = gen_variants(args)

    cargs = [f"--native-file={args.depsenv / 'meson_native.ini'}"]
    if args.configure_arg:
        cargs.extend(args.configure_arg)
    comp = Compilers(cc=args.compiler)

    if args.dry_run:
        print(f"For each of the following {len(variants)} configurations:")
        for v, _ in variants:
            print("  " + str(v))
        print(f"Would run the following {len(args.action)} actions:")
        for a in args.action:
            print("  " + a.name())
        sys.exit(0)

    times: dict[Action, list[float]] = {}

    all_ok = True
    good_variants: list[tuple[Configuration, ConcreteSpecification]] = []
    for v, unsat in variants:
        cfg = configure(meson, cargs, comp, v, unsat)
        if cfg is None:
            all_ok = all_ok and unsat
        elif unsat:
            all_ok = False
        else:
            good_variants.append((cfg, v))

    if not all_ok:
        print("## Configuration errors detected, stopping!")
        sys.exit(2)

    for i, (cfg, v) in enumerate(good_variants):
        ok, this_times = build(i + 1, len(good_variants), v, cfg, args)
        all_ok = all_ok and ok

        for a, t in this_times.items():
            ts = times.get(a, [])
            ts.append(t)
            times[a] = ts

        if len(good_variants) > 1:
            print()  # Blank line between configuration stanzas

        if args.fail_fast and not all_ok:
            print("## Too many failures encountered, stopping!")
            break

    if all_ok and len(good_variants) > 1:
        with section("Performance statistics", color=FgColor.info, collapsed=True):
            for i, a in enumerate(args.action):
                if a in times:
                    print(f"{i+1}/{len(args.action)}: {a.name()}")
                    print_stats(times[a], "  ")

    sys.exit(0 if all_ok else 1)
