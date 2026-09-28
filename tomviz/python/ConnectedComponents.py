import tomviz.operators


class ConnectedComponents(tomviz.operators.Operator):

    def transform(self, dataset, background_value=0, connectivity=1,
                  min_size=1):
        """Label the connected components of the foreground.

        Every voxel whose value is not the background value is
        foreground, and each connected group of foreground voxels gets
        its own label. Connectivity picks what counts as touching: faces
        only (6 neighbours), faces and edges (18) or faces, edges and
        corners (26). Components with fewer voxels than the minimum size
        are returned to the background.

        The labels are numbered by size, smallest first, so the largest
        component carries the highest label. The output uses the
        narrowest unsigned integer type that holds every label.
        """
        import numpy as np
        from scipy import ndimage

        array = dataset.active_scalars
        if array is None:
            raise RuntimeError('No data array found!')

        connectivity = int(min(max(connectivity, 1), 3))
        structure = ndimage.generate_binary_structure(array.ndim,
                                                      connectivity)
        labels, count = ndimage.label(array != background_value,
                                      structure=structure)
        # Voxels per label; the background at index 0 is never a
        # component.
        sizes = np.bincount(labels.ravel(), minlength=count + 1)
        keep = sizes >= max(1, int(min_size))
        keep[0] = False
        kept = np.flatnonzero(keep)
        # Smallest component first, as the ITK-based version of this
        # operator numbered them. Stable so equal sizes keep scan order.
        order = kept[np.argsort(sizes[kept], kind='stable')]

        remap = np.zeros(count + 1, dtype=np.int64)
        remap[order] = np.arange(1, len(order) + 1)
        num_labels = len(order)
        if num_labels < 2**8:
            dtype = np.uint8
        elif num_labels < 2**16:
            dtype = np.uint16
        else:
            dtype = np.uint32
        result = remap.astype(dtype)[labels]
        dataset.active_scalars = np.asfortranarray(result)
