import numpy as np

# Given an MRC file containing one or more 2D iamges, applies CTF operations on the them. 
# This program adopts the conventions of the following paper:
# J. A. Mindell and N. Grigorieff, "Accurate determination of local defocus and specimen tilt in electron microscopy," J. Struct. Biol., vol. 142, no. 3, pp. 334-347, Jun. 2003.
# (which is the same adopted in FREALIGN, RELION, CTFFIND3/4 and Gctf, among others)

def transform(dataset, apix=None, df1=None, df2=None, ast=None, ampcon=None, cs=None, kev=None, invert=None, ctf_method=None):

	tiltSeries = dataset.active_scalars
	Nproj = tiltSeries.shape[2]

	methods = ('ctf_multiply', 'phase_flip', 'wiener_filter')

	# Main loop
	for i in range(Nproj):
		tiltSeries[:,:,i] = correct_CTF(tiltSeries[:,:,i],df1,df2,ast,ampcon,cs,kev,apix,methods[ctf_method])

	dataset.active_scalars = tiltSeries

# Applies CTF correction to image
def correct_CTF(img,DF1,DF2,AST, AmpCon, Cs, kV, apix,method):

	# Cs - Hard constant

	# Direct CTF correction would invert the image contrast. 
	# By default we won't do that, hence the negative sign:
	CTFim = -CTF(img.shape, DF1, DF2, AST, AmpCon, Cs, kV, apix)

	if method == 'invert_contrast':
		CTFim *= -1
		
	FT = np.fft.rfftn(img)
	if method == 'phase_flip':
		s = np.sign(CTFim)
		CTFcor = np.fft.irfftn(FT*s)
	if method == 'ctf_multiply':
		CTFcor = np.fft.irfftn(FT * CTFim)
	if method == 'wiener_filter':
		CTFcor = (FT * CTFim) / (CTFim * CTFim + 1.0)

	return CTFcor

# Generates 2D CTF Function 
# Underfocus is positive following conventions of FREALIGN and most packages (in Angstroms)
# B is B-factor
# rfft is to compute only half of the FFT (i.e. real data) if True, or full FFT if False.
def CTF(imsize, DF1, DF2, AST, AmpCon, Cs, kV, apix, B, rfft=True):

	Cs *= 1e7 # Convert Cs to Angstroms

	if DF2 == None: 
		DF2 = DF1

	AST *= -np.pi / 180

	WL = ElectronWavelength(kV)

	w1 = np.sqrt(1 - AmpCon * AmpCon)
	w2 = AmpCon

	xmesh = np.fft.fftfreq(imsize[0])
	if rfft:
		ymesh = np.fft.rfftfreq(imsize[1])
	else:
		ymesh = np.fft.fftfreq(imsize[1])

	xmeshtile = np.tile(xmesh, [len(ymesh),1]).T
	ymeshtile = np.tile(ymesh, [len(xmesh),1])

	rmesh = np.sqrt(xmeshtile * xmeshtile + ymeshtile * ymeshtile) / apix
	amesh = np.nan_to_num(np.arctan2(ymeshtile,xmeshtile))

	rmesh2 = rmesh * rmesh

	# From Mindell & Grigorieff, JSB 2003:
	DF = 0.5 * ( DF1 + DF2 + (DF1 - DF2) * np.cos(2 * (amesh - AST)) )
	Xr = np.nan_to_num(np.pi * WL * rmesh2 * (DF - 0.5 * WL * WL * rmesh2 * Cs))

	CTFim = -w1 * np.sin(Xr) - w2 * np.cos(Xr)

	# Apply B-factor only if necessary
	if B != 0:
		CTFim = CTFim * np.exp(-B * rmesh2 / 4)

	return CTFim

# Calculate the Electron Wavelength is Angstroms
def ElectronWavelength(kV):
	return 12.2639 / np.sqrt(kV * 1e3 + 0.97856 * (kV**2))
