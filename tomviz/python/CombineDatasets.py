def transform(dataset, second_dataset):
    # Merge two datasets of identical shape into one dataset carrying
    # both scalar arrays. The primary input keeps its arrays and active
    # selection; the second dataset's active array is added alongside.

    primary = dataset.active_scalars
    secondary = second_dataset.active_scalars

    if primary is None or secondary is None:
        raise RuntimeError('Both datasets must have an active scalar array')

    if primary.shape != secondary.shape:
        raise RuntimeError(
            'Datasets must have the same shape to be combined: '
            f'{tuple(primary.shape)} vs {tuple(secondary.shape)}')

    # Name the new array after the second dataset's active array, made
    # unique against the arrays already present.
    name = second_dataset.active_name or 'Secondary'
    existing = set(dataset.scalars_names)
    unique_name = name
    counter = 1
    while unique_name in existing:
        counter += 1
        unique_name = f'{name} ({counter})'

    dataset.set_scalars(unique_name, secondary)
