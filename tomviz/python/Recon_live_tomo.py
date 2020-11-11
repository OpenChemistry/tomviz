import tomviz.operators
import numpy as np
import paramiko
import h5py


class liveTomoOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, hostName=None, userName=None, password=None,
                  port=None, filename=None, remoteDirectory=None,
                  localDirectory=None):

        transport = paramiko.Transport((hostName, port))
        transport.connect(username=userName, password=password)
        sftpConnection = paramiko.SFTPClient.from_transport(transport)
        sftpConnection.chdir(remoteDirectory)

        # Create child for recon
        child = dataset.create_child_dataset()
        lastMtime = 0
        firstLoad = True
        while True:

            if self.canceled:
                break

            # Check is new volume is available
            (newVolume, lastMtime) = check_for_new_volume(sftpConnection, 
                                      filename, localDirectory, lastMtime)
            if newVolume:
                try:
                    file = h5py.File(localDirectory + filename, 'r')
                    if firstLoad:
                        recon = np.zeros(file['recon'].shape, dtype=np.float32)
                        firstLoad = False
                    recon[:, :, :] = file['recon'][:, :, :]
                    child.active_scalars = recon
                    self.progress.data = child
                    file.close()
                except: 
                    continue

        sftpConnection.close() # Close connection

        # One last update of the child data
        child.active_scalars = recon

        returnValues = {}
        returnValues["reconstruction"] = child
        return returnValues


def check_for_new_volume(sftp, filename, localDirectory, lastModifiedTime):
    import time

    #Check if Coordinates has been updates.
    for f in sftp.listdir_attr():

        if (f.filename == filename):
            if (f.st_mtime > lastModifiedTime):

                time.sleep(5) # Wait Until Volume is ready for download

                print("Downloading %s..." % f.filename)
                sftp.get(f.filename, localDirectory + f.filename)

                lastModifiedTime = f.st_mtime # Update last modified time
                return (True, lastModifiedTime)
    return (False, lastModifiedTime)
