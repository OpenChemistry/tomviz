import functools


class inject(object):
    def __init__(self, param):
        self._param = param

    def __call__(self, func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            return func(self._param, *args, **kwargs)

        return wrapper
