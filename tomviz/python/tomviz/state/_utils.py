import collections
from types import SimpleNamespace


def iter_paths(tree, parent_path=()):
    for path, node in tree.items():
        current_path = parent_path + (path,)
        if isinstance(node, collections.Mapping):
            for inner_path in iter_paths(node, current_path):
                yield inner_path
        else:
            yield current_path

def to_namespaces(dct):
    root = SimpleNamespace()
    for path in iter_paths(dct):
        prop_name = path[-1]
        path = path[:-1]
        current_namespace = root

        for p in path:
            if hasattr(current_namespace, p):
                current_namespace = getattr(current_namespace, p)
            else:
                new_namespace = SimpleNamespace()
                setattr(current_namespace, p, new_namespace)
                current_namespace = new_namespace

        v = dct
        for k in path + (prop_name,):
            v = v[k]

        setattr(current_namespace, prop_name, v)

    return root

def from_namespace(namespace):
    d = {}

    for a in attrs(namespace):
        v = getattr(namespace, a)
        if attrs(v):
            d[a] = from_namespace(v)
        else:
            d[a] = v

    return d

def attrs(o):
    return [x for x in dir(o) if not x.startswith('_') and not callable(getattr(o, x)) and not isinstance(o, (int, float, str, dict)) ]