import matplotlib.pyplot as plt
import dm

fname = '/Users/jonathanschwartz/Desktop/library_demo_real_time/000_69.9937degrees_18.9159um_-47.1494um_-45.0593um_40000X_125mm.dm4'

file = dm.fileDM(fname)

im = file.getDataset(0)['data']

print(im.shape)

plt.imshow(im,cmap='gray'); plt.show()
