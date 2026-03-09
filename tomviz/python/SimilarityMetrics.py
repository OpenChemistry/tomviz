import tomviz.operators


class SimilarityMetrics(tomviz.operators.CancelableOperator):

    def transform(self, dataset, reference_dataset=None, selected_scalars=None, axis=2):
        """Compute similarity metrics between the current dataset and a
        reference dataset.
        """
        import numpy as np  # noqa: F811
        from skimage.transform import resize
        from skimage.metrics import structural_similarity, mean_squared_error

        if reference_dataset is None:
            raise Exception("A probe dataset is required.")

        if selected_scalars is None:
            selected_scalars = dataset.scalars_names

        axis_index = axis
        phase_scalars = None

        for name in reference_dataset.scalars_names:
            if "phase" in name.lower():
                phase_scalars = reference_dataset.scalars(name)
                break

        dataset_scalars_names = set(dataset.scalars_names)
        reference_scalars_names = set(reference_dataset.scalars_names)

        dataset_scan_ids = dataset.scan_ids
        reference_scan_ids = reference_dataset.scan_ids

        dataset_scan_id_to_slice = {}
        reference_scan_id_to_slice = {}

        dataset_shape = dataset.active_scalars.shape
        reference_shape = reference_dataset.active_scalars.shape

        dataset_n_slices = dataset_shape[axis_index]
        reference_n_slices = reference_shape[axis_index]

        if dataset_scan_ids is None or reference_scan_ids is None:
            for i in range(dataset_n_slices):
                dataset_scan_id_to_slice[i] = i

            for i in range(reference_n_slices):
                reference_scan_id_to_slice[i] = i

        else:
            for i, scan_id in enumerate(dataset_scan_ids):
                dataset_scan_id_to_slice[scan_id] = i

            for i, scan_id in enumerate(reference_scan_ids):
                reference_scan_id_to_slice[scan_id] = i

        slice_indices = []

        for i, (scan_id, dataset_slice_index) in enumerate(
            dataset_scan_id_to_slice.items()
        ):
            reference_slice_index = reference_scan_id_to_slice.get(scan_id)

            if reference_slice_index is None:
                continue

            slice_indices.append((dataset_slice_index, reference_slice_index))

        # When comparing slices between the two datasets we will resize them to a common size
        common_slice_shape = []
        for i in range(3):
            if i != axis_index:
                common_slice_shape.append(max(dataset_shape[i], reference_shape[i]))

        common_slice_shape = tuple(common_slice_shape)

        column_names = ["x"]
        all_table_data = None
        mse_columns = []
        ssim_columns = []

        for name in selected_scalars:
            scalars = dataset.scalars(name) if name in dataset_scalars_names else None
            if scalars is None:
                continue

            reference_scalars = (
                reference_dataset.scalars(name)
                if name in reference_scalars_names
                else phase_scalars
            )
            if reference_scalars is None:
                continue

            n_slices = len(slice_indices)

            self.progress.value = 0
            self.progress.maximum = n_slices
            self.progress.message = f"Array: {name}"

            mse_data = np.empty(n_slices)
            ssim_data = np.empty(n_slices)

            for output_slice_index, (
                dataset_slice_index,
                reference_slice_index,
            ) in enumerate(slice_indices):
                if self.canceled:
                    return

                self.progress.value = output_slice_index

                dataset_slice_indexing_list = [slice(None)] * scalars.ndim
                dataset_slice_indexing_list[axis_index] = dataset_slice_index
                dataset_slice_indexing = tuple(dataset_slice_indexing_list)

                reference_slice_indexing_list = [slice(None)] * scalars.ndim
                reference_slice_indexing_list[axis_index] = reference_slice_index
                reference_slice_indexing = tuple(reference_slice_indexing_list)

                scalars_slice = scalars[dataset_slice_indexing]
                reference_slice = reference_scalars[reference_slice_indexing]

                if scalars_slice.shape == common_slice_shape:
                    resized_scalars_slice = scalars_slice
                else:
                    resized_scalars_slice = resize(scalars_slice, common_slice_shape)

                if reference_slice.shape == common_slice_shape:
                    resized_reference_slice = reference_slice
                else:
                    resized_reference_slice = resize(
                        reference_slice, common_slice_shape
                    )

                # normalize the arrays
                resized_scalars_slice = (
                    resized_scalars_slice - np.min(resized_scalars_slice)
                ) / np.ptp(resized_scalars_slice)
                resized_reference_slice = (
                    resized_reference_slice - np.min(resized_reference_slice)
                ) / np.ptp(resized_reference_slice)

                mse_data[output_slice_index] = mean_squared_error(
                    resized_reference_slice, resized_scalars_slice
                )
                ssim_data[output_slice_index] = structural_similarity(
                    resized_reference_slice, resized_scalars_slice, data_range=1.0
                )

            self.progress.value = n_slices

            if all_table_data is None:
                all_table_data = np.arange(n_slices, dtype=float)

            column_names.append(f"{name} MSE")
            column_names.append(f"{name} SSIM")
            mse_columns.append(mse_data)
            ssim_columns.append(ssim_data)

        if all_table_data is None:
            raise RuntimeError("No scalars found!")

        n = len(all_table_data)
        num_cols = 1 + len(mse_columns) + len(ssim_columns)
        table_data = np.empty(shape=(n, num_cols))
        table_data[:, 0] = all_table_data
        for i, (mse_col, ssim_col) in enumerate(zip(mse_columns, ssim_columns)):
            table_data[:, 1 + 2 * i] = mse_col
            table_data[:, 2 + 2 * i] = ssim_col

        # Return similarity table as operator result
        return_values = {}
        axis_labels = ("Slice Index", "")
        log_flags = (False, False)
        table = tomviz.utils.make_spreadsheet(
            column_names, table_data, axis_labels, log_flags
        )
        return_values["similarity"] = table

        return return_values
