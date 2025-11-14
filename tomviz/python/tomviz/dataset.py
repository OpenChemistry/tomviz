from abc import ABC, abstractmethod

import numpy as np


class Dataset(ABC):
    """Abstract base class for tomviz Dataset objects.

    The Dataset is the standard object that is passed to operators within tomviz.
    It provides a unified interface for accessing and manipulating tilt image
    stacks and volumetric data, including scalar arrays, spacing information,
    tilt series metadata (such as tilt angles), and calibration data
    (dark/white fields).

    This object will always be automatically provided as the first argument in
    the `transform()` function within operators. For example, for the
    `Rotate` operator:

    .. code-block:: python

        def transform(dataset, rotation_angle=90.0, rotation_axis=0):
            # ...

            array = dataset.active_scalars

            # ...

    .. note::
       Concrete subclasses of this abstract base class are internal to tomviz.
       This documentation describes the public API that is accessible on
       Dataset instances passed to operators and other user-facing code.
       Users should interact with Dataset objects through this interface
       rather than instantiating subclasses directly.
    """
    @property
    @abstractmethod
    def active_scalars(self) -> np.ndarray:
        """The currently active scalar array data as a numpy array.

        The shape and dtype depend on the specific dataset.
        """
        pass

    @active_scalars.setter
    @abstractmethod
    def active_scalars(self, v: np.ndarray):
        """Set the currently active scalar array data."""
        pass

    @property
    @abstractmethod
    def active_name(self) -> str:
        """The name of the currently active scalar array."""
        pass

    @property
    @abstractmethod
    def num_scalars(self) -> int:
        """The total number of scalar arrays stored in this dataset."""
        pass

    @property
    @abstractmethod
    def scalars_names(self) -> list[str]:
        """List containing the name of each scalar array in the dataset."""
        pass

    @abstractmethod
    def scalars(self, name: str | None = None) -> np.ndarray:
        """Get a scalar array by name.

        :param name: The name of the scalar array to retrieve. If None, returns the
                     active scalar array.
        :return: The requested scalar array data.
        :raises KeyError: If the specified name does not exist in the dataset.
        """
        pass

    @abstractmethod
    def set_scalars(self, name: str, array: np.ndarray):
        """Add or update a scalar array in the dataset.

        :param name: The name to assign to this scalar array. If a field with this name
                     already exists, it will be overwritten.
        :param array: The scalar array data to store.
        """
        pass

    @property
    @abstractmethod
    def spacing(self) -> tuple[int, int, int]:
        """Voxel spacing in physical units for (x, y, z) dimensions.

        Units depend on the dataset but are typically in nanometers or similar
        physical units.
        """
        pass

    @spacing.setter
    @abstractmethod
    def spacing(self, v: tuple[int, int, int]):
        """Set the voxel spacing in physical units for (x, y, z) dimensions."""
        pass

    @property
    @abstractmethod
    def tilt_angles(self) -> np.ndarray | None:
        """Array of tilt angles for tomographic tilt series.

        Tilt angles are typically in degrees for each projection in a tilt series.
        Returns None if this is not a tilt series dataset or if tilt angles have
        not been set.
        """
        pass

    @tilt_angles.setter
    @abstractmethod
    def tilt_angles(self, v: np.ndarray | None):
        """Set the tilt angles for tomographic tilt series.

        Provide None to clear tilt angles.
        """
        pass

    @property
    @abstractmethod
    def tilt_axis(self) -> int | None:
        """The axis index around which tilting occurs in a tomographic tilt series.

        - 0 = x-axis
        - 1 = y-axis
        - 2 = z-axis
        - None = not applicable or not set
        """
        pass

    @property
    @abstractmethod
    def dark(self) -> np.ndarray | None:
        """Dark field calibration data.

        Dark field images are captured with no illumination and represent the
        baseline signal level of the detector (electronic noise, thermal noise, etc.).
        Returns None if not available.
        """
        pass

    @property
    @abstractmethod
    def white(self) -> np.ndarray | None:
        """White field (flat field) calibration data.

        White field images are captured with uniform illumination and no sample,
        representing the detector response and illumination variations. Used for
        flat-field correction. Returns None if not available.
        """
        pass

    @property
    @abstractmethod
    def file_name(self) -> str | None:
        """The original filename this dataset was loaded from.

        Returns None if the dataset was not loaded from a file or if the
        filename is not available.
        """
        return self._data_source.file_name

    @property
    @abstractmethod
    def metadata(self) -> dict:
        """Dictionary containing arbitrary metadata associated with the dataset.

        Can include acquisition parameters, instrument settings, timestamps, etc.
        """
        return self._data_source.metadata

    @abstractmethod
    def create_child_dataset(self) -> 'Dataset':
        """Create a new child dataset derived from this dataset.

        Child datasets typically share some properties with their parent but
        may represent derived or processed versions of the data.

        This is often used, for example, in reconstruction operators, for
        creating reconstruction output objects.

        :return: A new Dataset instance that is a child of this dataset.
        """
        pass
