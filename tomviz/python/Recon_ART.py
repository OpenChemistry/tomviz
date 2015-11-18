import numpy as np
import scipy.sparse as ss
from tomviz import utils

def transform_scalars(dataset):
  """3D Reconstruct from a tilt series using Algebraic Reconstruction Technique (ART)"""
  ###Niter###
  
  #Get Tilt angles
  tiltAngles = utils.get_tilt_angles(dataset)

  #Get Tilt Series
  tiltSeries = utils.get_array(dataset)
  (Nslice,Nray,Nproj) = tiltSeries.shape

  if tiltSeries is None:
      raise RuntimeError("No scalars found!")

  #Generate measurement matrix
  A = parallelRay(Nray,1.0,tiltAngles,Nray,1.0) #A is a sparse matrix
  recon = np.zeros((Nslice,Nray,Nray))

  art3(A.todense(),tiltSeries,recon,Niter)

  # set the result as the new scalars.
  utils.set_array(dataset, recon)
  
  # Mark dataset as volume
  utils.mark_as_volume(dataset)

def art3(A,tiltSeries,recon,iterNum=1,beta=1.0):
    (Nslice,Nray,Nproj) = tiltSeries.shape

    (Nrow,Ncol) = A.shape
    rowInnerProduct = np.zeros(Nrow);
    row = np.zeros(Ncol)
    f = np.zeros(Ncol) #placeholder for 2d image
    
    #calculate row inner product
    for j in range(Nrow):
        row[:] = A[j,].copy()
        rowInnerProduct[j] = np.dot(row,row)

    for s in range(Nslice):
        f[:] = 0;
        b = tiltSeries[s,:,:].transpose().flatten()
        for i in range(iterNum):
            for j in range(Nrow):
                row[:] = A[j,].copy()
                row_f_product = np.dot(row,f)
                a = (b[j]-row_f_product)/rowInnerProduct[j]
                f = f + row*a*beta
        recon[s,:,:] = f.reshape((Nray,Nray))

    return recon


def parallelRay(Nside,pixelWidth,angles,Nray,rayWidth):
    np.seterr(all='ignore') #suppress warning messages that pops up when dividing zeros
    print 'Generating parallel-beam measurement matrix using ray-driven model'
    Nproj = angles.size #number of projections
    
    #ray coordinates at 0 degree
    offsets = np.linspace(-(Nray*1.0-1)/2,(Nray*1.0-1)/2,Nray)*rayWidth
    #intersection lines/Grid Coordinates
    xgrid = np.linspace(-Nside*0.5,Nside*0.5,Nside+1)*pixelWidth
    ygrid = np.linspace(-Nside*0.5,Nside*0.5,Nside+1)*pixelWidth
    #print xgrid
    #print ygrid
    #Initialize vectors that contain matrix elements and corresponding row/column numbers
    rows = np.zeros(2*Nside*Nproj*Nray)
    cols = np.zeros(2*Nside*Nproj*Nray)
    vals = np.zeros(2*Nside*Nproj*Nray)
    idxend = 0
    
    for i in range(0,Nproj): #loop over projection angles
        ang = angles[i]*np.pi/180.
        #points passed by rays at current angles
        xrayRotated = np.cos(ang)*offsets
        yrayRotated = np.sin(ang)*offsets
        xrayRotated[np.abs(xrayRotated)<1e-8]=0
        yrayRotated[np.abs(yrayRotated)<1e-8]=0
        
        a = -np.sin(ang); a = rmepsilon(a)
        b = np.cos(ang); b = rmepsilon(b)
        
        for j in range(0,Nray): #loop rays in current projection
            #print xrayRotated[j],yrayRotated[j]
            #Ray: y = tx * x + intercept
            t_xgrid = (xgrid - xrayRotated[j])/a
            y_xgrid = b*t_xgrid + yrayRotated[j]
            
            t_ygrid = (ygrid - yrayRotated[j])/b
            x_ygrid = a*t_ygrid + xrayRotated[j]
            #collect all points
            t_grid = np.append(t_xgrid,t_ygrid)
            xx = np.append(xgrid,x_ygrid)
            yy = np.append(y_xgrid,ygrid)
            #Sort the coordinates according to intersection time
            I = np.argsort(t_grid)
            xx = xx[I]
            yy = yy[I]
            
            #Get rid of points that are outside the image grid
            Ix = np.logical_and(xx>=-Nside/2.0*pixelWidth,xx<=Nside/2.0*pixelWidth)
            Iy = np.logical_and(yy>=-Nside/2.0*pixelWidth,yy<=Nside/2.0*pixelWidth)
            I = np.logical_and(Ix,Iy)
            xx = xx[I]; yy = yy[I]
            
            if (xx.size!=0 and yy.size!=0): #if the ray pass through the image grid
                #Get rid of double counted points
                I = np.logical_and(np.abs(np.diff(xx))<=1e-8,np.abs(np.diff(yy))<=1e-8)
                I2 = np.zeros(I.size+1)
                I2[0:-1]= I
                xx = xx[np.logical_not(I2)]
                yy = yy[np.logical_not(I2)]
                
                #Calculate the length within the cell
                length = np.sqrt(np.diff(xx)**2+np.diff(yy)**2)
                #Count number of cells the ray passes through
                numvals = length.size
                
                #Remove the rays that are on the boundary of the box in the top or to the right of the image grid
                check1 = np.logical_and(b==0,np.absolute(yrayRotated[j]-Nside/2*pixelWidth)<1e-15)
                check2 = np.logical_and(a==0,np.absolute(xrayRotated[j]-Nside/2*pixelWidth)<1e-15)
                check = np.logical_not(np.logical_or(check1,check2))
                
                if np.logical_and(numvals>0,check):
                    #Calculate corresponding indices in measurement matrix
                    #First, calculate the mid points coord. between two adjacent grid points
                    midpoints_x = rmepsilon(0.5*(xx[0:-1]+xx[1:]))
                    midpoints_y = rmepsilon(0.5*(yy[0:-1]+yy[1:]))
                    #print 'midpoints_x is:',midpoints_x
                    #print 'midpoints_y is:',midpoints_y
                    #Calculate the pixel index for mid points
                    pixelIndicex = (np.floor(Nside/2.0-midpoints_y/pixelWidth))*Nside + (np.floor(midpoints_x/pixelWidth+Nside/2.0))
                    #print 'pixelIndicex is:', pixelIndicex
               	    #create the indices to store the values to the measurement matrix
                    idxstart = idxend
               	    idxend = idxstart + numvals
               	    idx = np.arange(idxstart,idxend)
                    #store row numbers, column numbers and values
                    rows[idx] = i*Nray + j
                    cols[idx] = pixelIndicex
                    vals[idx] = length
            else:
                print "Ray No.", j+1, "at", angles[i], "degree is out of image grid!"

    #truncae excess zeros
    rows = rows[:idxend]
    cols = cols[:idxend]
    vals = vals[:idxend]
    A = ss.coo_matrix((vals,(rows,cols)),shape=(Nray*Nproj,Nside**2))
    return A

def rmepsilon(input):
    if (input.size>1):
        input[np.abs(input)<1e-10]=0;
    else:
        if np.abs(input)<1e-10:
            input = 0
    return input


