import os

try:
    from .load_output import list_elements, extract_elements  # noqa
    from .ic_names import ic_names  # noqa
    from .sids import filter_sids
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
