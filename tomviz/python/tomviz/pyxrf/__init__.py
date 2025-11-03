import os

try:
    from .load_output import list_elements, extract_elements  # noqa
    from .make_hdf5 import make_hdf5  # noqa
    from .process_projections import fix_python_paths, ic_names, process_projections  # noqa
    requirements_installed = True
except ImportError:
    requirements_installed = False

    import traceback
    import_error_exc = traceback.format_exc()

    if 'TOMVIZ_REQUIRE_PYXRF' in os.environ:
        # Raise an exception so we can be notified if PyXRF import fails
        raise


def installed():
    return requirements_installed


def import_error():
    if requirements_installed:
        return ''
    else:
        return import_error_exc
