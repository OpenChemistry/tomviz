import os

try:
    from .ptycho import (
        gather_ptycho_info,
        get_use_and_versions_from_csv,
        filter_sid_list,
        load_stack_ptycho,
    )
    requirements_installed = True
except ImportError:
    requirements_installed = False

    import traceback
    import_error_exc = traceback.format_exc()

    if 'TOMVIZ_REQUIRE_PTYCHO' in os.environ:
        # Raise an exception so we can be notified if Ptycho import fails
        raise


def installed():
    return requirements_installed


def import_error():
    if requirements_installed:
        return ''
    else:
        return import_error_exc
