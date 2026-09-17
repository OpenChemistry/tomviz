import tomviz.operators


class RemoveLabels(tomviz.operators.Operator):

    def transform(self, dataset, labels='', renumber=False):
        """Send the listed labels of a label map to the background.

        Labels are listed by value, separated by commas, with runs
        written as "first-last": "3, 5, 10-14". With renumbering on,
        the labels that remain are renumbered 1, 2, 3... in ascending
        order of their old values; otherwise they keep their values, so
        the names and colors given to them in a Label Map visualization
        still line up.
        """
        import numpy as np

        array = dataset.active_scalars
        if array is None:
            raise RuntimeError('No data array found!')
        if not np.issubdtype(array.dtype, np.integer):
            raise RuntimeError('Remove Labels needs an integer label map; '
                               'the active scalars are %s' % array.dtype)

        remove = _parse_labels(labels)
        result = array.copy()
        if remove:
            result[np.isin(result, remove)] = 0

        if renumber:
            result = _renumbered(result)

        dataset.active_scalars = np.asfortranarray(result)


def _parse_labels(text):
    """Label values from a list like "3, 5, 10-14", sorted and unique."""
    import re

    # "3 - 5" is one run, so close it up before splitting on whitespace
    text = re.sub(r'(\d+)\s*-\s*(\d+)', r'\1-\2', str(text or ''))
    values = set()
    for token in re.split(r'[,;\s]+', text):
        if not token:
            continue
        span = re.fullmatch(r'(\d+)-(\d+)', token)
        if span:
            first, last = int(span.group(1)), int(span.group(2))
            values.update(range(min(first, last), max(first, last) + 1))
        elif re.fullmatch(r'\d+', token):
            values.add(int(token))
        else:
            raise RuntimeError(
                'Cannot read "%s" as a label or a range of labels; use '
                'values like "3, 5, 10-14"' % token)
    return sorted(values)


def _renumbered(array):
    """The remaining labels as 1..N in ascending order of value, in the
    narrowest unsigned type that holds them; 0 stays the background."""
    import numpy as np

    largest = int(array.max()) if array.size else 0
    smallest = int(array.min()) if array.size else 0
    if smallest >= 0 and largest < 2**24:
        # A lookup table is far cheaper than sorting the volume
        present = np.bincount(array.ravel(), minlength=largest + 1) > 0
        present[0] = False
        lookup = np.cumsum(present)
        lookup[~present] = 0
        count = int(lookup.max())
        result = lookup[array]
    else:
        values, inverse = np.unique(array, return_inverse=True)
        new_values = np.cumsum(values != 0)
        new_values[values == 0] = 0
        count = int(new_values.max())
        result = new_values[inverse].reshape(array.shape)

    if count < 2**8:
        dtype = np.uint8
    elif count < 2**16:
        dtype = np.uint16
    else:
        dtype = np.uint32
    return result.astype(dtype)
