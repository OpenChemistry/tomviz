def transform(dataset, selected_scalars=None):
    if selected_scalars is None:
        selected_scalars = (dataset.active_name,)

    if len(selected_scalars) == 0:
        raise RuntimeError("At least one scalar array must be selected to keep.")

    to_remove = [n for n in dataset.scalars_names if n not in selected_scalars]
    for name in to_remove:
        dataset.remove_scalars(name)
