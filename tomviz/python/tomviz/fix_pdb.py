import pdb
import sys


class FixedPdb(pdb.Pdb):
    """
    Since we re-direct stdin, stdout, and stderr in other parts of
    the application, pdb can't interpret things like arrow keys
    and auto-complete correctly.
    This class fixes the issue by getting pdb to always use
    the default stdout and stderr.
    """
    def set_trace(self, *args, **kwargs):
        self._use_default_pipes()
        return super().set_trace(*args, **kwargs)

    def do_continue(self, *args, **kwargs):
        self._restore_pipes()
        return super().do_continue(*args, **kwargs)

    def _use_default_pipes(self):
        self._prev_stdout = sys.stdout
        self._prev_stderr = sys.stderr
        self._prev_stdin = sys.stdin
        sys.stdout = sys.__stdout__
        sys.stderr = sys.__stderr__
        sys.stdin = sys.__stdin__

    def _restore_pipes(self):
        sys.stdout = self._prev_stdout
        sys.stderr = self._prev_stderr
        sys.stdin = self._prev_stdin


def fix_pdb():
    if not isinstance(pdb.Pdb, FixedPdb):
        pdb.Pdb = FixedPdb
