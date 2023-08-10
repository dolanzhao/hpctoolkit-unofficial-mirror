import collections.abc
import typing

from .abc import CompilerBase as Compiler
from .proxy import ProxyPackage as Package
from .proxy import ProxySpack as Spack
from .util import Version

__all__ = ("Version", "Compiler", "Package", "Spack")

C = typing.TypeVar("C", bound=Compiler)


def preferred_compiler(compilers: collections.abc.Iterable[C]) -> C:
    """Fetch the "most preferred" compiler from the set."""
    result: C | None = None
    for c in compilers:
        if result is None or c.preferred_over(result):
            result = c
    if result is None:
        raise ValueError("Expected at least one Compiler, got none")
    return result