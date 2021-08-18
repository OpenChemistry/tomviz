"""
This module provides an interface to the SER file format written by FEI and Therm0 Fischer's program TIA.

It reads STEM and TEM images and other datasets.

It is based on information provided by Dr Chris Boothroyd and work by Peter Ercius' original serReader code in Matlab.

Note
----
General users:
    Use the simplified ser.serReader() function to load the data and meta
    data as a python dictionary.

Advanced users and developers:
    Access the file internals through the ser.fileSER() class.

"""

import xml.etree.ElementTree as ET
from pathlib import Path
import os  # TODO: Remove os and use pathlib instead.

import numpy as np


class NotSERError(Exception):
    """Exception if a file is not in SER file format.

    """
    pass


class fileSER:
    """ Class to represent SER files (read only).

    Attributes
    ----------
    _file_hdl : file
        The open file as a raw stream.
    _emi : dict
        A dictionary of metadata from the EMI file accompanying the SER file.
    filename : str
        A string of the file name of the SER file.
    head : dict
        Header information for the SER file as a dictionary. Provides direct access to data offsets and other
        internal file information.

    Note
    ----
    For most users, we suggest using the ser.serReader() function to
    load the full data set into memory. Otherwise, this class provides
    low level access to the SER file data and metadata and internals.

    Examples
    --------
    Read data from a single image into memory using the low level API.

    >>> import matplotlib.pyplot as plt
    >>> import ncempy.io as nio
    >>> with nio.ser.fileSER('filename.ser') as ser1:
    >>>    data, metadata = ser1.getDataset(0)

    SER files are internally structured such that each image in a series is a
    different data set. Thus, time series data should be read as the
    following:

    >>> with ser.fileSER('filename_1.ser') as ser1:
    >>>     image0, metadata0 = ser1.getDataset(0)
    >>>     image1, metadata1 = ser1.getDataset(1)
    """

    _dictByteOrder = {0x4949: 'little endian'}
    '''dict : Information on byte order.'''

    _dictSeriesVersion = {0x0210: '< TIA 4.7.3', 0x0220: '>= TIA 4.7.3'}
    '''dict : Information on file format version.'''

    _dictDataTypeID = {0x4120: '1D datasets', 0x4122: '2D images'}
    '''dict : Information on data type.'''

    _dictTagTypeID = {0x4152: 'time only', 0x4142: 'time and 2D position'}
    '''dict : Information on tag type.'''

    _dictDataType = {1: ' <u1', 2: '<u2', 3: '<u4', 4: '<i1', 5: '<i2', 6: '<i4', 7: '<f4', 8: '<f8', 9: '<c8',
                     10: '<c16'}
    '''dict : Information on data format.'''

    def __init__(self, filename, verbose=False):
        """Init opening the file and reading in the header.

        Parameters
        ----------
            filename : str
                Name of the SER file.

            verbose : bool, optional
                True to get extensive output while reading the file.

        """
        # necessary declarations, if something fails
        self._file_hdl = None
        self._emi = None
        self.filename = filename
        self.head = None

        # check filename type
        if isinstance(self.filename, str):
            pass
        elif isinstance(self.filename, Path):
            self.filename = str(self.filename)
        else:
            raise TypeError('Filename is supposed to be a string or pathlib.Path')

        # try opening the file
        try:
            self._file_hdl = open(filename, 'rb')
        except IOError:
            print('Error reading file: "{}"'.format(filename))
            raise
        except:
            raise

        # read header
        self.head = self.readHeader(verbose)

        # read emi file if exists
        self._read_emi()

    def __del__(self):
        """ Close the file stream in destructor.

        """
        if not self._file_hdl.closed:
            self._file_hdl.close()

    def __enter__(self):
        """ Implement python's with statement

        """
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        """Implement python's with statement
        and close the file via __del__()

        """
        self.__del__()
        return None

    def __str__(self):
        return 'ncempy SER data set'

    def readHeader(self, verbose=False):
        """Read and return the SER files header.

        Parameters
        ----------
            verbose: bool
                True to get extensive output while reading the file.

        Returns
        -------
            : dict
                 The header of the SER file as dict.

        """

        # prepare empty dict to be populated while reading
        head = {}

        # go back to beginning of file
        self._file_hdl.seek(0, 0)

        # read 3 int16
        data = np.fromfile(self._file_hdl, dtype='<i2', count=3)

        # ByteOrder (only little Endian expected)
        if not data[0] in self._dictByteOrder:
            raise RuntimeError('Only little Endian implemented for SER files')
        head['ByteOrder'] = data[0]
        if verbose:
            print('ByteOrder:\t"{:#06x}",\t{}'.format(data[0], self._dictByteOrder[data[0]]))

        # SeriesID, check whether TIA Series Data File   
        if not data[1] == 0x0197:
            raise NotSERError('This is not a TIA Series Data File (SER)')
        head['SeriesID'] = data[1]
        if verbose:
            print('SeriesID:\t"{:#06x},\tTIA Series Data File'.format(data[1]))

        # SeriesVersion
        if not data[2] in self._dictSeriesVersion:
            raise RuntimeError('Unknown TIA version: "{:#06x}"'.format(data[2]))
        head['SeriesVersion'] = data[2]
        if verbose:
            print('SeriesVersion:\t"{:#06x}",\t{}'.format(data[2], self._dictSeriesVersion[data[2]]))
        # version dependent file format for below
        if head['SeriesVersion'] == 0x0210:
            offset_dtype = '<i4'
        else:
            # head['SeriesVersion']==0x220:
            offset_dtype = '<i8'

        # read 4 int32
        data = np.fromfile(self._file_hdl, dtype='<i4', count=4)

        # DataTypeID
        if not data[0] in self._dictDataTypeID:
            raise RuntimeError('Unknown DataTypeID: "{:#06x}"'.format(data[0]))
        head['DataTypeID'] = data[0]
        if verbose:
            print('DataTypeID:\t"{:#06x}",\t{}'.format(data[0], self._dictDataTypeID[data[0]]))

        # TagTypeID
        if not data[1] in self._dictTagTypeID:
            raise RuntimeError('Unknown TagTypeID: "{:#06x}"'.format(data[1]))
        head['TagTypeID'] = data[1]
        if verbose:
            print('TagTypeID:\t"{:#06x}",\t{}'.format(data[1], self._dictTagTypeID[data[1]]))

        # TotalNumberElements
        if not data[2] >= 0:
            raise RuntimeError('Negative total number of elements: {}'.format(data[2]))
        head['TotalNumberElements'] = data[2]
        if verbose:
            print('TotalNumberElements:\t{}'.format(data[2]))

        # ValidNumberElements
        if not data[3] >= 0:
            raise RuntimeError('Negative valid number of elements: {}'.format(data[3]))
        head['ValidNumberElements'] = data[3]
        if verbose:
            print('ValidNumberElements:\t{}'.format(data[3]))

        # OffsetArrayOffset, sensitive to SeriesVersion
        data = np.fromfile(self._file_hdl, dtype=offset_dtype, count=1)
        head['OffsetArrayOffset'] = data[0]
        if verbose:
            print('OffsetArrayOffset:\t{}'.format(data[0]))

        # NumberDimensions
        data = np.fromfile(self._file_hdl, dtype='<i4', count=1)
        if not data[0] >= 0:
            raise RuntimeError('Negative number of dimensions')
        head['NumberDimensions'] = data[0]
        if verbose:
            print('NumberDimensions:\t{}'.format(data[0]))

        # Dimensions array
        dimensions = []
        for i in range(head['NumberDimensions']):
            if verbose:
                print('reading Dimension {}'.format(i))
            this_dim = {}

            # DimensionSize
            data = np.fromfile(self._file_hdl, dtype='<i4', count=1)
            this_dim['DimensionSize'] = data[0]
            if verbose:
                print('DimensionSize:\t{}'.format(data[0]))

            data = np.fromfile(self._file_hdl, dtype='<f8', count=2)

            # CalibrationOffset
            this_dim['CalibrationOffset'] = data[0]
            if verbose:
                print('CalibrationOffset:\t{}'.format(data[0]))

            # CalibrationDelta
            this_dim['CalibrationDelta'] = data[1]
            if verbose:
                print('CalibrationDelta:\t{}'.format(data[1]))

            data = np.fromfile(self._file_hdl, dtype='<i4', count=2)

            # CalibrationElement
            this_dim['CalibrationElement'] = data[0]
            if verbose:
                print('CalibrationElement:\t{}'.format(data[0]))

            # DescriptionLength
            n = data[1]

            # Description
            data = np.fromfile(self._file_hdl, dtype='<i1', count=n)
            data = ''.join(map(chr, data))
            this_dim['Description'] = data
            if verbose:
                print('Description:\t{}'.format(data))

            # UnitsLength
            data = np.fromfile(self._file_hdl, dtype='<i4', count=1)
            n = data[0]

            # Units
            data = np.fromfile(self._file_hdl, dtype='<i1', count=n)
            data = ''.join(map(chr, data))
            this_dim['Units'] = data
            if verbose:
                print('Units:\t{}'.format(data))

            dimensions.append(this_dim)

        # save dimensions array as tuple of dicts in head dict
        head['Dimensions'] = tuple(dimensions)

        # Offset array
        self._file_hdl.seek(head['OffsetArrayOffset'], 0)

        # DataOffsetArray
        data = np.fromfile(self._file_hdl, dtype=offset_dtype, count=head['ValidNumberElements'])
        head['DataOffsetArray'] = data.tolist()
        if verbose:
            print('reading in DataOffsetArray')

        # TagOffsetArray
        data = np.fromfile(self._file_hdl, dtype=offset_dtype, count=head['ValidNumberElements'])
        head['TagOffsetArray'] = data.tolist()
        if verbose:
            print('reading in TagOffsetArray')

        return head

    def _checkIndex(self, i):
        """ Check index i for sanity, otherwise raise Exception.

        Parameters
        ----------
            i: int
                Index.

        """

        # check type
        if not isinstance(i, int):
            raise TypeError('index supposed to be integer')

        # check whether in range
        if i < 0 or i >= self.head['ValidNumberElements']:
            raise IndexError('Index out of range accessing element {} of {} valid elements'.format(i + 1, self.head[
                'ValidNumberElements']))

        return

    def getDataset(self, index, verbose=False):
        """ Retrieve data and meta data for one image or spectra
        from the file.

        Parameters
        ----------
            index: int
                Index of dataset.
            verbose: bool, optional
                True to get extensive output while reading the file.

        Returns
        -------
            dataset: tuple, 2 elements in form (data metadata)
                Tuple contains data as np.ndarray and metadata
                (pixel size, etc.) as a dict.

        """

        # check index, will raise Exceptions if not
        try:
            self._checkIndex(index)
        except:
            raise

        if verbose:
            print('Getting dataset {} of {}.'.format(index, self.head['ValidNumberElements']))

        # go to dataset in file
        self._file_hdl.seek(self.head['DataOffsetArray'][index], 0)

        # read meta
        meta = {}

        # number of calibrations depends on DataTypeID
        if self.head['DataTypeID'] == 0x4120:
            n = 1
        elif self.head['DataTypeID'] == 0x4122:
            n = 2
        else:
            raise RuntimeError('Unknown DataTypeID')

        # read in the calibrations    
        cals = []
        for i in range(n):
            if verbose:
                print('Reading calibration {}'.format(i))

            this_cal = {}

            data = np.fromfile(self._file_hdl, dtype='<f8', count=2)

            # CalibrationOffset
            this_cal['CalibrationOffset'] = data[0]
            if verbose:
                print('CalibrationOffset:\t{}'.format(data[0]))

            # CalibrationDelta
            this_cal['CalibrationDelta'] = data[1]
            if verbose:
                print('CalibrationDelta:\t{}'.format(data[1]))

            data = np.fromfile(self._file_hdl, dtype='<i4', count=1)

            # CalibrationElement
            this_cal['CalibrationElement'] = data[0]
            if verbose:
                print('CalibrationElement:\t{}'.format(data[0]))

            cals.append(this_cal)

        meta['Calibration'] = tuple(cals)

        data = np.fromfile(self._file_hdl, dtype='<i2', count=1)

        # DataType
        meta['DataType'] = data[0]

        if not data[0] in self._dictDataType:
            raise RuntimeError('Unknown DataType: "{}"'.format(data[0]))
        if verbose:
            print('DataType:\t{},\t{}'.format(data[0], self._dictDataType[data[0]]))

        dataset = None  # in case something goes wrong

        if self.head['DataTypeID'] == 0x4120:
            # 1D data element

            data = np.fromfile(self._file_hdl, dtype='<i4', count=1)
            # ArrayLength
            data = data.tolist()
            meta['ArrayShape'] = data
            if verbose:
                print('ArrayShape:\t{}'.format(data))

            dataset = np.fromfile(self._file_hdl, dtype=self._dictDataType[meta['DataType']],
                                  count=meta['ArrayShape'][0])

        elif self.head['DataTypeID'] == 0x4122:
            # 2D data element

            data = np.fromfile(self._file_hdl, dtype='<i4', count=2)
            # ArrayShape
            data = data.tolist()
            meta['ArrayShape'] = data
            if verbose:
                print('ArrayShape:\t{}'.format(data))

            # dataset
            dataset = np.fromfile(self._file_hdl, dtype=self._dictDataType[meta['DataType']],
                                  count=meta['ArrayShape'][0] * meta['ArrayShape'][1])
            dataset = dataset.reshape(meta['ArrayShape'][::-1])  # needs to be reversed for little endian data

            dataset = np.flipud(dataset)

        return dataset, meta

    def _getTag(self, index, verbose=False):
        """Retrieve tag from data file.

        Parameters
        ----------
            index: int
                Index of tag.
            verbose: bool
                True to get extensive output while reading the file.

        Returns
        -------
            tag: dict
                Tag as a python dictionary.

        """

        # check index, will raise Exceptions if not
        try:
            self._checkIndex(index)
        except:
            raise

        if verbose:
            print('Getting tag {} of {}.'.format(index, self.head['ValidNumberElements']))

        tag = {}

        try:
            # bad tagoffsets occurred pointing to the end of the file

            # go to dataset in file
            self._file_hdl.seek(self.head['TagOffsetArray'][index], 0)

            data = np.fromfile(self._file_hdl, dtype='<i4', count=2)

            # TagTypeID
            tag['TagTypeID'] = data[0]

            # only proceed if TagTypeID is the same like in the file header (bad TagOffsetArray issue)
            if tag['TagTypeID'] == self.head['TagTypeID']:
                if verbose:
                    print('TagTypeID:\t"{:#06x}",\t{}'.format(data[0], self._dictTagTypeID[data[0]]))

                # Time    
                tag['Time'] = data[1]
                if verbose:
                    print('Time:\t{}'.format(data[1]))

                # check for position
                if tag['TagTypeID'] == 0x4142:
                    data = np.fromfile(self._file_hdl, dtype='<f8', count=2)

                    # PositionX
                    tag['PositionX'] = data[0]
                    if verbose:
                        print('PositionX:\t{}'.format(data[0]))

                    # PositionY
                    tag['PositionY'] = data[1]
                    if verbose:
                        print('PositionY:\t{}'.format(data[1]))
            else:
                # otherwise raise to get to default tag
                raise
        except:
            tag['TagTypeID'] = 0
            tag['Time'] = 0
            tag['PositionX'] = np.nan
            tag['PositionY'] = np.nan

        return tag

    def _createDim(self, size, offset, delta, element):
        """Create dimension labels for conversion to EMD
        from information in the SER file.

        Parameters
        ----------
            size: int
                Number of elements.
            offset: float
                Value at indicated element.
            delta: float
                Difference between elements.
            element: int
                Indicates the element of value offset.

        Returns
        -------
            dim: np.ndarray
                Dimension vector as array.

        """

        # if element is out off range, map it back into defined
        if element >= size:
            element = size - 1
            offset = offset - (element - (size - 1)) * delta

        dim = np.array(range(size)).astype('f8')
        dim = dim * delta
        dim += (offset - dim[element])

        # some weird shifting, positionx is +0.5, positiony is -0.5
        # doing this during saving
        # dim += 0.5*delta

        return dim

    def _read_emi(self):
        # Generate emi file string
        # and test for file existence.

        emi_file = self.filename[:-6] + '.emi'
        if not os.path.exists(emi_file):
            self._emi = None
        else:
            self._emi = read_emi(emi_file)


def read_emi(filename):
    """Read the meta data from an emi file.

    Parameters
    ----------
        filename: str or pathlib.Path
            Path to the emi file.

    Returns
    -------
        : dict
            Dictionary of experimental metadata stored in the EMI file.
    """

    # check filename type
    if isinstance(filename, str):
        pass
    elif isinstance(filename, Path):
        filename = str(filename)
    else:
        raise TypeError('Filename is supposed to be a string or pathlib.Path')

    # try opening the file
    try:
        # open file for reading bytes, as binary and text are intermixed
        with open(filename, 'rb') as f_emi:
            emi_data = f_emi.read()
    except IOError:
        print('Error reading file: "{}"'.format(filename))
        raise
    except:
        raise

    # dict to store _emi stuff
    _emi = {}

    # need anything readable from <ObjectInfo> to </ObjectInfo>
    # collect = False
    # data = b''
    # for line in f_emi:
    #    if b'<ObjectInfo>' in line:
    #        collect = True
    #    if collect:
    #        data += line.strip()
    #    if b'</ObjectInfo>' in line:
    #        collect = False

    # close the file
    # f_emi.close()

    metaStart = emi_data.find(b'<ObjectInfo>')
    metaEnd = emi_data.find(b'</ObjectInfo>')  # need to add len('</ObjectInfo>') = 13 to encompass this final tag

    root = ET.fromstring(emi_data[metaStart:metaEnd + 13])

    # strip of binary stuff still around
    # data = data.decode('ascii', errors='ignore')
    # matchObj = re.search('<ObjectInfo>(.+?)</ObjectInfo', data)
    # try:
    #    data = matchObj.group(1)
    # except:
    #    raise RuntimeError('Could not find _emi metadata in specified file.')

    # parse metadata as xml
    # root = ET.fromstring('<_emi>' + data + '</_emi>')

    # single items
    _emi['Uuid'] = root.findtext('Uuid')
    _emi['AcquireDate'] = root.findtext('AcquireDate')
    _emi['Manufacturer'] = root.findtext('Manufacturer')
    _emi['DetectorPixelHeight'] = root.findtext('DetectorPixelHeight')
    _emi['DetectorPixelWidth'] = root.findtext('DetectorPixelWidth')

    # Microscope Conditions
    grp = root.find('ExperimentalConditions/MicroscopeConditions')

    for elem in grp:
        _emi[elem.tag] = _parseEntry_emi(elem.text)

    # Experimental Description
    grp = root.find('ExperimentalDescription/Root')

    for elem in grp:
        _emi['{} [{}]'.format(elem.findtext('Label'), elem.findtext('Unit'))] = _parseEntry_emi(
            elem.findtext('Value'))

    # AcquireInfo
    grp = root.find('AcquireInfo')

    for elem in grp:
        _emi[elem.tag] = _parseEntry_emi(elem.text)

    # DetectorRange
    grp = root.find('DetectorRange')

    for elem in grp:
        _emi['DetectorRange_' + elem.tag] = _parseEntry_emi(elem.text)

    return _emi


def _parseEntry_emi(value):
    """Auxiliary function to parse string entry to int, float or np.string_().

    Parameters
    ----------
        value : str
            String containing an int, float or string.

    Returns
    -------
        : int or float or str
            Entry value as int, float or string.

    """

    # try to parse as int
    try:
        p = int(value)
    except ValueError:
        # if not int, then try float
        try:
            p = float(value)
        except ValueError:
            # if neither int nor float, stay with string
            p = np.string_(str(value))

    return p


def serReader(filename):
    """Simple function to parse the file and read all datasets. This is a one function implementation to load all data
     in a ser file.

    Parameters
    ----------
        filename : str
            The filename of the SER file containing the data.

    Returns
    -------
        dataOut : dict
            A dictionary containing the data and meta data.
            The data is accessed using the 'data' key and is a 1, 2, 3, or 4
            dimensional numpy ndarray.

    Examples
    --------
        Load a single image data set and show the image:

            >>> import ncempy.io as nio
            >>> ser1 = nio.ser.serReader('filename_1.ser')
            >>> plt.imshow(ser1['data'])  # show the single image from the data file
    """
    # Open the file and init the class
    with fileSER(filename) as f1:
        if f1.head['ValidNumberElements'] > 0:
            # Get the first data set to setup the arrays
            data, metaData = f1.getDataset(0)

            metaData['filename'] = filename  # save the file name in the output dictionary

            npType = f1._dictDataType[metaData['DataType']]

            if f1.head['DataTypeID'] == 0x4120:
                # Spectra as 1D single spectra, 2D line scan or 3D spectrum image
                numSpectra = f1.head['ValidNumberElements']
                spectraSize = data.shape[0]

                # Read in all spectra
                temp = np.zeros((numSpectra, spectraSize), dtype=npType)  # C-style ordering
                for ii in range(0, numSpectra):
                    data0, meta1 = f1.getDataset(ii)
                    temp[ii, :] = data0

                if f1.head['NumberDimensions'] > 1:
                    # Spectrum map
                    scanI = f1.head['Dimensions'][0]['DimensionSize']
                    scanJ = f1.head['Dimensions'][1]['DimensionSize']
                    temp = temp.reshape((scanJ, scanI, spectraSize))  # operations on spectra are fastest
                else:
                    temp = np.squeeze(temp)

                # Setup the energy loss axis for convenience
                eDelta = metaData['Calibration'][0]['CalibrationDelta']
                eOffset = metaData['Calibration'][0]['CalibrationOffset']
                eLoss = np.linspace(0, (spectraSize - 1) * eDelta, spectraSize) + eOffset

                dataOut = {'data': temp, 'eLoss': eLoss, 'eOffset': eOffset, 'eDelta': eDelta,
                           'scanCalibration': f1.head['Dimensions']}
            elif f1.head['DataTypeID'] == 0x4122:
                # Images as 2D or 3D image series
                temp = np.empty([f1.head['ValidNumberElements'], data.shape[0], data.shape[1]], dtype=npType)
                for ii in range(0, f1.head['ValidNumberElements']):
                    data0, metadata0 = f1.getDataset(ii)
                    temp[ii, :, :] = data0  # get the next dataset

                temp = np.squeeze(temp)  # remove singular dimensions

                dataOut = {'data': temp, 'pixelSize': [], 'pixelUnit': [], 'pixelOrigin': []}

                # Setup some simple meta data

                for cal in metaData['Calibration']:
                    dataOut['pixelSize'].append(cal['CalibrationDelta'])
                    dataOut['pixelOrigin'].append(cal['CalibrationOffset'])
                    dataOut['pixelUnit'].append('m')
                dataOut['filename'] = filename  # save the file name

            # Add experimental metadata, if exists
            if f1._emi:
                dataOut['metadata'] = f1._emi
        else:
            dataOut = {}
            print('No data set found')
    return dataOut
