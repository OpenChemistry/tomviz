import numpy as np


# Given an dataset containing one or more 2D images,
# apply CTF operations on them.
def transform(dataset, apix=None, df1=None, df2=None, ast=None, ampcon=None,
              cs=None, kev=None, ctf_method=None, snr=None):

    tiltSeries = dataset.active_scalars

    methods = ('wiener_filter', 'ctf_multiply', 'phase_flip')

    # Main loop
    for i in range(tiltSeries.shape[2]):
        tiltSeries[:, :, i] = correct_CTF(tiltSeries[:, :, i], df1, df2, ast,
                                          ampcon, cs, kev, apix,
                                          methods[ctf_method], snr)

    dataset.active_scalars = tiltSeries


# Applies CTF correction to image
def correct_CTF(img, DF1, DF2, AST, AmpCon, Cs, kV, apix, method, snr):

    CTFim = -CTF(img.shape, DF1, DF2, AST, AmpCon, Cs, kV, apix)

    FTim = np.fft.rfftn(img)
    if method == 'wiener_filter':
        CTFcor = np.fft.irfftn((FTim * CTFim) / (CTFim * CTFim + snr))
    if method == 'phase_flip':
        s = np.sign(CTFim)
        CTFcor = np.fft.irfftn(FTim * s)
    if method == 'ctf_multiply':
        CTFcor = np.fft.irfftn(FTim * CTFim)

    return CTFcor


# Generates 2D CTF Function
# Underfocus is positive following conventions of FREALIGN and most packages
# DF - (in Microns)
def CTF(imSize, DF1, DF2, AST, AmpCon, Cs, kV, apix):

    Cs *= 1e7 # Convert Cs to Angstroms
    DF1 *= 1e4 # Convert defocii from microns to Angstroms
    DF2 *= 1e4

    AST *= -np.pi / 180

    WL = ElectronWavelength(kV)

    w1 = np.sqrt(1 - AmpCon * AmpCon)
    w2 = AmpCon

    xMesh = np.fft.fftfreq(imSize[0])
    yMesh = np.fft.rfftfreq(imSize[1])

    xMeshTile = np.tile(xMesh, [len(yMesh), 1]).T
    yMeshTile = np.tile(yMesh, [len(xMesh), 1])

    radialMesh = np.sqrt(xMeshTile * xMeshTile + yMeshTile * yMeshTile) / apix
    angleMesh = np.nan_to_num(np.arctan2(yMeshTile, xMeshTile))
    radialMeshSq = radialMesh * radialMesh

    DF = 0.5 * (DF1 + DF2 + (DF1 - DF2) * np.cos(2 * (angleMesh - AST)))
    Xr = np.nan_to_num(np.pi * WL * radialMeshSq *
                       (DF - 0.5 * WL * WL * radialMeshSq * Cs))

    CTFim = -w1 * np.sin(Xr) - w2 * np.cos(Xr)

    return CTFim


# Calculate the Electron Wavelength in Angstroms
def ElectronWavelength(keV):
    h = 6.626e-34
    eCharge = 1.6e-19
    c = 3e8     # m / s
    eRest = 511 # keV
    return h*c / np.sqrt(((2*eRest+keV)*keV)*(eCharge*eCharge)) * 1e7
