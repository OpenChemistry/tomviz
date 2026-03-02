import tomviz.operators

import numpy as np
import scipy


def deconv_admm(g, psf, mu):
# Fast ADMM_TV/L2 algorithm based on "An Augmented Lagrangian Method for Total Variation Video Restoration", 
# Stanley H. Chan, Student Member, IEEE, Ramsin Khoshabeh, Student Member, IEEE, Kristofor B. Gibson, Student Member, IEEE, Philip E. Gill, and Truong Q. Nguyen, Fellow, IEEE 
# IEEE TRANSACTIONS ON IMAGE PROCESSING, VOL. 20, NO. 11, NOVEMBER 2011     
# Note: For a cyclic H, H = F^-1*diag(Fh)*F, where h is the first column of H
# H^H = F^-1*diag(conj(Fh))*F

    #mu = 1/sigma**2
    G = np.fft.fft2(g)
    sz = np.shape(g)
    h_sz = np.shape(psf)
    h = np.pad(psf,((0,sz[0]-h_sz[0]),(0,sz[1]-h_sz[1])))
    h = np.roll(h,[-(h_sz[0]//2),-(h_sz[1]//2)],[0,1])
    H = np.fft.fft2(h,sz)
    f = g
    ux = np.zeros(sz)
    uy = np.zeros(sz)
    yx = np.zeros(sz)
    yy = np.zeros(sz)
    rho_r = 2
    Dx = np.fft.fft2(np.asarray([[-1,1],[0,0]]),sz)
    Dy = np.fft.fft2(np.asarray([[-1,1],[0,0]]).T,sz)
    HtH = np.abs(H)**2
    DxtDx = np.abs(Dx)**2
    DytDy = np.abs(Dy)**2
    cov = 1
    tol = 1e-4
    itr = 0
    max_iter = 50
    HtG = np.conj(H)*G
    vx, vy = der_im(f)
    rnorm = np.sum(np.sqrt(vx.ravel()**2 + vy.ravel()**2))
    
    loss = np.zeros((max_iter,1))
    rdiff = np.zeros((max_iter,1))
    
    while cov > tol and itr < max_iter :
    
        dxt_ux, dyt_uy = der_t(ux,uy) 
        dxt_yx, dyt_yy = der_t(yx,yy)
        tmp1 = mu*HtG + np.fft.fft2(rho_r*(dxt_ux + dyt_uy)-(dxt_yx+dyt_yy))
        tmp2 = mu*HtH + rho_r*(DxtDx + DytDy)
        
        f_old = f
        f = np.real(np.fft.ifft2(tmp1/tmp2))
        #f = np.fft.ifft2(H*G/HtH)
        vx, vy = der_im(f)
        vvx = vx + (1/rho_r)*yx
        vvy = vy + (1/rho_r)*yy
        
        # anisotropic form
        #ux = np.fmax(np.abs(vvx)-1/rho_r,0)*np.sign(vvx)
        #uy = np.fmax(np.abs(vvy)-1/rho_r,0)*np.sign(vvy)
        
        # isotropic form
        v = np.sqrt(vvx**2+vvy**2)
        v[v==0] = 1
        v = np.fmax(v-1/rho_r,0)/v
        ux = v*vvx
        uy = v*vvy
        
        yx = yx - rho_r*(ux - vx)
        yy = yy - rho_r*(uy - vy)
        
        cov = np.linalg.norm(f - f_old)/np.linalg.norm(f)
        cost = (mu/2)*np.sum((g.ravel() - conv_2d(f,psf).ravel())**2) \
                + np.sum(np.sqrt(vx.ravel()**2 + vy.ravel()**2))
        
        #print("Iter = %d\tConvegence = %6.4e\tCost = %6.4e\trho_r = %d" %(itr,cov,cost,rho_r)) 
        
        rnorm_old = rnorm
        rnorm = np.sum(np.sqrt((ux.ravel() - vx.ravel())**2 + (uy.ravel() - vy.ravel())**2))
        if rnorm > 0.7*rnorm_old:
            rho_r = rho_r*2
        
        loss[itr,0] = cost
        rdiff[itr,0] = cov
        itr += 1
           
    return f, g, loss, rdiff
        
def der_im(f):
    dx_f = np.diff(f,1,1)
    dy_f = np.diff(f,1,0)
    dx_f = np.concatenate((dx_f,np.reshape(f[:,0]-f[:,-1],[-1,1])),1)
    dy_f = np.concatenate((dy_f,np.reshape(f[0,:]-f[-1,:],[1,-1])),0)    
    return dx_f, dy_f
def der_t(ux,uy):
    dxt_ux = np.concatenate((np.reshape(ux[:,-1]-ux[:,0],[-1,1]),-np.diff(ux,1,1)),1)
    dyt_uy = np.concatenate((np.reshape(uy[-1,:]-uy[0,:],[1,-1]),-np.diff(uy,1,0)),0)
    return dxt_ux,dyt_uy
def conv_2d(f,h):
    sz = np.shape(f)
    h_sz = np.shape(h)
    h = np.pad(h,((0,sz[0]-h_sz[0]),(0,sz[1]-h_sz[1])))
    h = np.roll(h,[-(h_sz[0]//2),-(h_sz[1]//2)],[0,1])
    H = np.fft.fft2(h)
    F = np.fft.fft2(f)
    g = np.fft.ifft2(F*H)
    return np.real(g)
def deconv_2d(g,h):
    sz = np.shape(g)
    h_sz = np.shape(h)
    h = np.pad(h,((0,sz[0]-h_sz[0]),(0,sz[1]-h_sz[1])))
    h = np.roll(h,[-(h_sz[0]//2),-(h_sz[1]//2)],[0,1])
    H = np.fft.fft2(h)
    HtH = np.abs(H)**2
    G = np.fft.fft2(g)
    f = np.fft.ifft2(np.conj(H)*G/HtH)
    return np.real(f)

def deconv_admm_NN(g, psf, mu, rho_r, rho_o, tol, alpha, beta, max_iter):

# Fast ADMM_TV/L2 algorithm based on "An Augmented Lagrangian Method for Total Variation Video Restoration", 
# Stanley H. Chan, Student Member, IEEE, Ramsin Khoshabeh, Student Member, IEEE, Kristofor B. Gibson, Student Member, IEEE, Philip E. Gill, and Truong Q. Nguyen, Fellow, IEEE 
# IEEE TRANSACTIONS ON IMAGE PROCESSING, VOL. 20, NO. 11, NOVEMBER 2011
# With non_negative constraint
# Note: For a cyclic H, H = F^-1*diag(Fh)*F, where h is the first column of H
# H^H = F^-1*diag(conj(Fh))*F

    #rho_r = ops.rho_r
    #rho_o = ops.rho_o
    #max_iter = ops.max_iter
    
    G = np.fft.fft2(g)
    sz = np.shape(g)
    h_sz = np.shape(psf)
    h = np.pad(psf,((0,sz[0]-h_sz[0]),(0,sz[1]-h_sz[1])))
    h = np.roll(h,[-(h_sz[0]//2),-(h_sz[1]//2)],[0,1])
    H = np.fft.fft2(h)
    f = g
    ux = np.zeros(sz)
    uy = np.zeros(sz)
    yx = np.zeros(sz)
    yy = np.zeros(sz)
    
    t = np.zeros(sz)
    q = np.zeros(sz)
    
    
    
    Dx = np.fft.fft2(np.asarray([[-1,1],[0,0]]),sz)
    Dy = np.fft.fft2(np.asarray([[-1,1],[0,0]]).T,sz)
    HtH = np.abs(H)**2
    DxtDx = np.abs(Dx)**2
    DytDy = np.abs(Dy)**2
    cov = 1
    itr = 0
    
    HtG = np.conj(H)*G
    vx, vy = der_im(f)
    rnorm = np.sum(np.sqrt(vx.ravel()**2 + vy.ravel()**2))
    onorm = np.linalg.norm(f,'fro')
    while cov > tol and itr < max_iter :
    
        dxt_ux, dyt_uy = der_t(ux,uy) 
        dxt_yx, dyt_yy = der_t(yx,yy)
        tmp1 = mu*HtG + np.fft.fft2(rho_r*(dxt_ux + dyt_uy)-(dxt_yx+dyt_yy)+rho_o*t-q)
        tmp2 = mu*HtH + rho_r*(DxtDx + DytDy) + rho_o
        
        f_old = f
        f = np.real(np.fft.ifft2(tmp1/tmp2))
        #f = np.fft.ifft2(H*G/HtH)
        vx, vy = der_im(f)
        vvx = vx + (1/rho_r)*yx
        vvy = vy + (1/rho_r)*yy
        
        # anisotropic form
        #ux = np.fmax(np.abs(vvx)-1/rho_r,0)*np.sign(vvx)
        #uy = np.fmax(np.abs(vvy)-1/rho_r,0)*np.sign(vvy)
        
        # isotropic form
        v = np.sqrt(vvx**2+vvy**2)
        v[v==0] = 1
        v = np.fmax(v-1/rho_r,0)/v
        ux = v*vvx
        uy = v*vvy
        
        yx = yx - rho_r*(ux - vx)
        yy = yy - rho_r*(uy - vy)
        
        t = np.fmax(f + (1/rho_o)*q, 0)
        q = q - rho_o*(t - f)
        
        
        cov = np.linalg.norm(f - f_old)/np.linalg.norm(f)
        cost = (mu/2)*np.sum((g.ravel() - conv_2d(f,psf).ravel())**2) \
                + np.sum(np.sqrt(vx.ravel()**2 + vy.ravel()**2))
        
        print("Iter = %d\tConvegence = %6.4e\tCost = %6.4e\trho_r = %4.2f\trho_o = %4.2f" %(itr,cov,cost,rho_r,rho_o)) 
        
        rnorm_old = rnorm
        rnorm = np.sum(np.sqrt((ux.ravel() - vx.ravel())**2 + (uy.ravel() - vy.ravel())**2))
        if rnorm > alpha*rnorm_old:
            rho_r = rho_r*beta

        onorm_old = onorm
        onorm = np.linalg.norm(f-t,'fro')
        if onorm > alpha*onorm_old:
            rho_o = rho_o*beta
        
        itr += 1
        
        #cov = 0
        
    return f, t

def anscombe(in_data):
    out_data = 2*np.sqrt(in_data+3.0/8.0)
    return out_data

def inv_anscombe(in_data):
    out_data = (in_data/2.0)**2 - 1.0/8.0
    return out_data

def gauss(sz,w):
    l = len(sz)
    #print(l)
    if l == 1:
        x = np.arange(0,sz[0],1)
        out = np.exp(-(x-sz[0]//2)**2/(2*w**2))
    elif l == 2:
        y = np.arange(0,sz[0],1)
        x = np.arange(0,sz[1],1)
        X, Y = np.meshgrid(x,y)
        out = np.exp(-(Y-sz[0]//2)**2/(2*w**2))*np.exp(-(X-sz[1]//2)**2/(2*w**2))
    elif l == 3:
        y = np.arange(0,sz[0],1)
        x = np.arange(0,sz[1],1)
        z = np.arange(0,sz[2],1)
        X, Y, Z = np.meshgrid(x, y, z)
        out = np.exp(-(Y-sz[0]//2)**2/(2*w**2))*np.exp(-(X-sz[1]//2)**2/(2*w**2))\
              *np.exp(-(Z-sz[2]//2)**2/(2*w**2))
    out = out/np.sum(out.ravel())
    return out

#def deconv(im_in,psf,sigma,max_iter, method='ADMM-TV'):
#    if method == 'ADMM-TV':
#        im_out = deconv_admm(im_in,psf,1/(2*sigma**2))
#    elif method == 'APG-TV':
        
def conv_mat_direct(im_sz,psf):
    psf_sz = psf.shape
    psf_tot = np.size(psf)
    cent = [psf_sz[0]//2,psf_sz[1]//2]
    im_tot = im_sz[0]*im_sz[1]
    locs_tot = psf_sz[0]*psf_sz[1]*im_tot
    full_j_ind = np.zeros((locs_tot,1),dtype='int')
    full_i_ind = np.zeros((locs_tot,1),dtype='int')
    full_data = np.zeros((locs_tot,1),dtype='float')
    
    locs = np.arange(0,im_tot)
    locs = np.reshape(locs,im_sz)
    
    cnt_numel = 0
    for row in range(im_sz[0]):
        for col in range(im_sz[1]):
            row_ind = row*im_sz[1]+col
            row_locs1 = np.arange(-cent[0]+row+im_sz[0],im_sz[0],1,dtype='int')
            row_locs2 = np.arange(0,-cent[0]+row+psf_sz[0]-im_sz[0],1,dtype='int')
            row_locs3 = np.arange(np.fmax(-cent[0]+row,0),np.fmin(-cent[0]+row+psf_sz[0],im_sz[0]),1,dtype='int')                 
            row_locs  = np.concatenate((row_locs1,row_locs3,row_locs2)) 
            
            col_locs1 = np.arange(-cent[1]+col+im_sz[1],im_sz[1],1,dtype='int')
            col_locs2 = np.arange(0,-cent[1]+col+psf_sz[1]-im_sz[1],1,dtype='int')
            col_locs3 = np.arange(np.fmax(-cent[1]+col,0),np.fmin(-cent[1]+col+psf_sz[1],im_sz[1]),1,dtype='int')
            col_locs  = np.concatenate((col_locs1,col_locs3,col_locs2)) 
            
            curr_locs = locs[np.ix_(row_locs,col_locs)]
            #print(np.size(col_locs))
            full_i_ind[cnt_numel:cnt_numel+psf_tot, 0] = row_ind
            full_j_ind[cnt_numel:cnt_numel+psf_tot,0] = curr_locs.ravel()
            full_data[cnt_numel:cnt_numel+psf_tot,0] = psf.ravel()
            
            cnt_numel = cnt_numel + psf_tot
             
    H = scipy.sparse.coo_matrix((full_data.ravel(),(full_i_ind.ravel(),full_j_ind.ravel())),(im_tot,im_tot))
    return H

def conv_mat_dilation(scale,lr_im_sz,psf):
    
    hr_im_sz = np.array(lr_im_sz)*np.array(scale)
    psf_sz = np.shape(psf)
    psf_tot = np.size(psf)
    cent = [psf_sz[0]//2,psf_sz[1]//2]
    hr_im_tot = hr_im_sz[0]*hr_im_sz[1]
    lr_im_tot = lr_im_sz[0]*lr_im_sz[1]
    locs_tot = psf_sz[0]*psf_sz[1]*lr_im_tot
    full_j_ind = np.zeros((locs_tot,1),dtype='int')
    full_i_ind = np.zeros((locs_tot,1),dtype='int')
    full_data = np.zeros((locs_tot,1),dtype='float')
    
    locs = np.arange(0,hr_im_tot)
    locs = np.reshape(locs,hr_im_sz)
    
    cnt_numel = 0
    
    for lr_row in range(lr_im_sz[0]):
        for lr_col in range(lr_im_sz[1]):
            row_ind = lr_row*lr_im_sz[1]+lr_col
            hr_row = lr_row*scale[0]
            hr_col = lr_col*scale[1]
            row_locs1 = np.arange(-cent[0]+hr_row+hr_im_sz[0],hr_im_sz[0],1,dtype='int')
            row_locs2 = np.arange(0,-cent[0]+hr_row+psf_sz[0]-hr_im_sz[0],1,dtype='int')
            row_locs3 = np.arange(np.fmax(-cent[0]+hr_row,0),np.fmin(-cent[0]+hr_row+psf_sz[0],hr_im_sz[0]),1,dtype='int')                 
            row_locs  = np.concatenate((row_locs1,row_locs3,row_locs2)) 
            
            col_locs1 = np.arange(-cent[1]+hr_col+hr_im_sz[1],hr_im_sz[1],1,dtype='int')
            col_locs2 = np.arange(0,-cent[1]+hr_col+psf_sz[1]-hr_im_sz[1],1,dtype='int')
            col_locs3 = np.arange(np.fmax(-cent[1]+hr_col,0),np.fmin(-cent[1]+hr_col+psf_sz[1],hr_im_sz[1]),1,dtype='int')
            col_locs  = np.concatenate((col_locs1,col_locs3,col_locs2)) 
            
            curr_locs = locs[np.ix_(row_locs,col_locs)]
            #print(np.size(col_locs))
            full_i_ind[cnt_numel:cnt_numel+psf_tot, 0] = row_ind
            full_j_ind[cnt_numel:cnt_numel+psf_tot,0] = curr_locs.ravel()
            full_data[cnt_numel:cnt_numel+psf_tot,0] = psf.ravel()
            
            cnt_numel = cnt_numel + psf_tot
             
    H = scipy.sparse.coo_matrix((full_data.ravel(),(full_i_ind.ravel(),full_j_ind.ravel())),(lr_im_tot,hr_im_tot))
    return H
def sp_conv_mat(scale,lr_im_sz,psf,conv_med=['dilation','dilation']):
    if conv_med ==['dilation','dilation']:
        return conv_mat_dilation(scale,lr_im_sz,psf)
    else:
        if scale[0]*scale[1] > 1:
            hr_im_sz = np.array(lr_im_sz)*np.array(scale)
            H_hr = conv_mat_direct(hr_im_sz,psf)
            if conv_med[0] == 'dilation':
                row_psf = np.ones((1,1))  
            else: # averaging
                row_psf = np.ones((scale[0],1), dtype='float')/scale[0]
            H_row = conv_mat_dilation([scale[0],1],[lr_im_sz[0],hr_im_sz[1]],row_psf)
            if conv_med[1] == 'dilation':
                col_psf = np.ones((1,1))  
            else: # averaging
                col_psf = np.ones((1,scale[1]),dtype='float')/scale[1]
            H_col = conv_mat_dilation([1,scale[1]],[lr_im_sz[0],lr_im_sz[1]],col_psf)
            
            return H_col@(H_row@H_hr)
        else:
            return conv_mat_direct(lr_im_sz,psf)
        
def sp_conv_mat_v1(scale,lr_im_sz,psf,conv_med=['dilation','dilation']):
    # this version is slightly faster
    if conv_med ==['dilation','dilation']:
        return conv_mat_dilation(scale,lr_im_sz,psf)
    elif scale[0]*scale[1] == 1:
        return conv_mat_direct(lr_im_sz,psf) 
    elif conv_med ==['dilation','average']:
        hr_im_sz = np.array(lr_im_sz)*np.array(scale)
        H = conv_mat_dilation([scale[0],1],[lr_im_sz[0],hr_im_sz[1]],psf)
        if scale[1] > 1:
            col_psf = np.ones((1,scale[1]),dtype='float')/scale[1]
            H_col = conv_mat_dilation([1,scale[1]],[lr_im_sz[0],lr_im_sz[1]],col_psf)
            H = H_col@H
        return H
    elif conv_med ==['average','dilation']:
        hr_im_sz = np.array(lr_im_sz)*np.array(scale)
        H = conv_mat_dilation([1,scale[1]],[hr_im_sz[0],lr_im_sz[1]],psf)
        if scale[0] > 1:
            row_psf = np.ones((scale[0],1),dtype='float')/scale[1]
            H_row = conv_mat_dilation([scale[0],1],[lr_im_sz[0],lr_im_sz[1]],row_psf)
            H = H_row@H
        return H
    elif conv_med == ['average','average']:
        hr_im_sz = np.array(lr_im_sz)*np.array(scale)
        H = conv_mat_direct(hr_im_sz,psf)
        avg_psf = np.ones(scale,dtype='float')/(scale[0]*scale[1])
        H_avg = conv_mat_dilation(scale,lr_im_sz,avg_psf)
        H = H_avg@H
        return H
    else:
        return conv_mat_dilation(scale,lr_im_sz,psf)
                   

def deconv_apg_tv(im,psf,scale,mu,conv_med,max_iter):
    from skimage.transform import rescale

    #max_iter = 100
    beta = 0.5
    tol = 1e-4
    lr_im_sz = np.shape(im)
    hr_im_sz = [lr_im_sz[0]*scale[0],lr_im_sz[1]*scale[1]]
    
    H = sp_conv_mat_v1(scale,lr_im_sz,psf,conv_med = conv_med)
    y = im.ravel()
    HtH = H.T@H
    Hty = H.T@y
    original = rescale(im,scale)
    x = original.ravel()
    a = 1
    w = x
    
    loss = np.zeros((max_iter,1))
    rdiff = np.zeros((max_iter,1))
    
    for i in range(max_iter):
        grad = HtH@w - Hty
        for k in range(10):
            w_new = w - a*grad
            x_new, _, l,r = deconv_admm(np.reshape(w_new,hr_im_sz),[[1]],mu)
            x_new = x_new.ravel()
            L_new = 0.5*np.sum((H@x_new - y)**2)
            Q = 0.5*np.sum((H@w - y)**2) + grad.T@(x_new-w)+1/(2*a)*np.linalg.norm(x_new - w)**2
            if L_new <= Q:
                break
            else:
                a = a*beta
        
        covg = np.linalg.norm(x-x_new)/np.linalg.norm(x_new)
        w = x_new + (i/(i+3.0))*(x_new-x)
        x = x_new
        
        loss[i,0] = L_new
        rdiff[i,0] = covg
        
        if covg < tol or k == 9:
            break
    return np.reshape(w,hr_im_sz), original, loss, rdiff

def deconv_apg_bm3d(im,psf,scale,mu,conv_med,max_iter):
    import bm3d
    from skimage.transform import rescale

    beta = 0.5
    tol = 1e-4
    effective_sigma = mu
    
    lr_im_sz = np.shape(im)
    hr_im_sz = [lr_im_sz[0]*scale[0],lr_im_sz[1]*scale[1]]
    
    H = sp_conv_mat_v1(scale,lr_im_sz,psf,conv_med = conv_med)
    y = im.ravel()
    HtH = H.T@H
    Hty = H.T@y
    original = rescale(im,scale)
    x = original.ravel()
    a = 1
    w = x
    
    loss = np.zeros((max_iter,1))
    rdiff = np.zeros((max_iter,1))
    
    for i in range(max_iter):
        grad = HtH@w - Hty
        for k in range(10):
            w_new = w - a*grad
            x_new = bm3d.bm3d(np.reshape(w_new,hr_im_sz),effective_sigma)
            x_new = x_new.ravel()
            L_new = 0.5*np.sum((H@x_new - y)**2)
            Q = 0.5*np.sum((H@w - y)**2) + grad.T@(x_new-w)+1/(2*a)*np.linalg.norm(x_new - w)**2
            if L_new <= Q:
                break
            else:
                a = a*beta
        
        covg = np.linalg.norm(x-x_new)/np.linalg.norm(x_new)
        w = x_new + (i/(i+3.0))*(x_new-x)
        x = x_new
        loss[i,0] = L_new
        rdiff[i,0] = covg
        if  k == 9 or covg < tol:
            break
        
    return np.reshape(w,hr_im_sz), original, loss, rdiff

def deconv_apg_bm3d_poisson(im,psf,scale,mu,conv_med,max_iter):
    import bm3d

    ep = 0.1
    beta = 0.5
    tol = 1e-4
    effective_sigma = mu
    
    lr_im_sz = np.shape(im)
    hr_im_sz = [lr_im_sz[0]*scale[0],lr_im_sz[1]*scale[1]]
    
    H = sp_conv_mat_v1(scale,lr_im_sz,psf,conv_med = conv_med)
    y = im.ravel()
    I_col = scipy.sparse.coo_matrix(
        np.ones((lr_im_sz[0]*lr_im_sz[1],1),dtype='float'))
    
    HtI = H.T@I_col
    #Hty = H.T@y
    x = rescale(im,scale).ravel().reshape((hr_im_sz[0]*hr_im_sz[1],1))
    a = 1
    w = x
    
    loss = np.zeros((max_iter,1))
    rdiff = np.zeros((max_iter,1))
    
    for i in range(max_iter):
        grad = HtI - H.T@(y/(H@w+ep))
        for k in range(10):
            w_new = w - a*grad
            w_new_trans = anscombe(w_new)
            w_max = np.max(w_new_trans)
            
            x_new_trans = bm3d.bm3d(np.reshape(w_new_trans/w_max,hr_im_sz),1.0/w_max)
            x_new_trans = x_new_trans*w_max
            
            x_new = inv_anscombe(x_new_trans)
            
            x_new = x_new.ravel()
            Hx = H@x_new
            L_new = 0.5*np.sum((Hx - y*np.log(Hx)))
            Hw = H@w
            Q = 0.5*np.sum((Hw - y*np.log(Hw))) + grad.T@(x_new-w)+1/(2*a)*np.linalg.norm(x_new - w)**2
            if L_new <= Q:
                break
            else:
                a = a*beta
        
        covg = np.linalg.norm(x-x_new)/np.linalg.norm(x_new)
        w = x_new + (i/(i+3.0))*(x_new-x)
        x = x_new
        loss[i,0] = L_new
        rdiff[i,0] = covg
        if  k == 9 or covg < tol:
            break
        
    return np.reshape(w,hr_im_sz), loss, rdiff

def deconv(im,psf,scale,mu,conv_med = ['dilation','dilation'], deconv_med='ADMM_TV',max_iter = 50):
    
    if deconv_med == 'ADMM_TV':
        if scale[0]*scale[1] != 1:
            print('ADMM_TV does not support upscaling; forcing scale = [1,1].')
        out, original, loss, rdiff = deconv_admm(im,psf,mu)
    elif deconv_med == 'APG_TV':
        out, original, loss, rdiff = deconv_apg_tv(im,psf,scale,mu,conv_med = conv_med,max_iter=max_iter)
    elif deconv_med == 'APG_BM3D':
        out, original, loss, rdiff = deconv_apg_bm3d(im,psf,scale,mu,conv_med = conv_med,max_iter=max_iter)
    else:
        raise Exception("Unknown deconvolution method: {deconv_med}")

    return out, original, loss, rdiff

def PSNR(ori_im, rec_im):
    y1 = np.asarray(ori_im)
    y2 = np.asarray(rec_im)
    if y1.max() < 1.5:
        y1 = y1*255
    if y2.max() < 1.5:
        y2 = y2*255
    err = np.sqrt(np.mean((y1.ravel()-y2.ravel())**2))
    
    psnr = 20*np.log10(255/err)
    return psnr
def RMSE(ori_im, rec_im):
    y1 = np.asarray(ori_im)
    y2 = np.asarray(rec_im)
    if y1.max() < 1.5:
        y1 = y1*255
    if y2.max() < 1.5:
        y2 = y2*255
    rmse = np.linalg.norm(y1.ravel()-y2.ravel())/np.sqrt(y1.size)
    return rmse


class DeconvolutionDenoise(tomviz.operators.CancelableOperator):

    def transform(self, dataset, probe=None, selected_scalars=None,
                  method='APG_BM3D', fast_axis_scanning="dilation",
                  slow_axis_scanning="average",
                  max_iter=8, scale_x=1, scale_y=1, mu=1.0,
                  axis=2, probe_kernel=11):
        """Deconvolution-based denoising using a probe and a selected
        regularization method.
        """
        import numpy as np  # noqa: F811

        if probe is None:
            raise Exception("A probe dataset is required.")

        if selected_scalars is None:
            selected_scalars = (dataset.active_name,)

        if method == 'ADMM_TV' and (scale_x != 1 or scale_y != 1):
            print('ADMM_TV does not support upscaling; forcing scale = [1., 1.]')
            scale_x = 1
            scale_y = 1

        all_scalars_set = set(dataset.scalars_names)
        selected_scalars_set = set(selected_scalars)

        axis_index = axis

        probe_scalars = None

        for name in probe.scalars_names:
            if "amplitude" in name.lower():
                probe_scalars = probe.scalars(name)
                break

        if probe_scalars is None:
            raise Exception("There are no scalars named 'amplitude' in the selected probe dataset.")

        dataset_scan_ids = dataset.scan_ids
        probe_scan_ids = probe.scan_ids
        dataset_tilt_angles = dataset.tilt_angles

        dataset_scan_id_to_slice = {}
        probe_scan_id_to_slice = {}

        dataset_shape = dataset.active_scalars.shape
        probe_shape = probe.active_scalars.shape

        dataset_n_slices = dataset_shape[axis_index]
        probe_n_slices = probe_shape[axis_index]

        output_scan_ids = []
        output_tilt_angles = []

        if dataset_scan_ids is None or probe_scan_ids is None:
            for i in range(dataset_n_slices):
                dataset_scan_id_to_slice[i] = i

            for i in range(probe_n_slices):
                probe_scan_id_to_slice[i] = i

        else:
            for i, scan_id in enumerate(dataset_scan_ids):
                dataset_scan_id_to_slice[scan_id] = i

            for i, scan_id in enumerate(probe_scan_ids):
                probe_scan_id_to_slice[scan_id] = i

        slice_indices = []
        for i, (scan_id, dataset_slice_index) in enumerate(dataset_scan_id_to_slice.items()):
            probe_slice_index = probe_scan_id_to_slice.get(scan_id)
            
            if probe_slice_index is None:
                continue

            slice_indices.append((dataset_slice_index, probe_slice_index))
            output_scan_ids.append(scan_id)

            if dataset_tilt_angles is not None:
                output_tilt_angles.append(dataset_tilt_angles[i])

        for name in selected_scalars:
            scalars = dataset.scalars(name)
            if scalars is None:
                continue

            n_slices = len(slice_indices)

            self.progress.value = 0
            self.progress.maximum = n_slices
            self.progress.message = f"Array: {name}"

            output_shape = list(scalars.shape)
            scale = [scale_x, scale_y]
            j = 0

            for i in range(len(output_shape)):
                if i == axis_index:
                    output_shape[i] = n_slices
                else:
                    output_shape[i] = output_shape[i] * scale[j]
                    j += 1

            output_scalars = np.empty(output_shape)

            for output_slice_index, (dataset_slice_index, probe_slice_index) in enumerate(slice_indices):
            # for slice_index in range(n_slices):
                self.progress.value = output_slice_index

                dataset_slice_indexing_list = [slice(None)] * scalars.ndim
                dataset_slice_indexing_list[axis_index] = dataset_slice_index
                dataset_slice_indexing = tuple(dataset_slice_indexing_list)

                probe_slice_indexing_list = [slice(None)] * scalars.ndim
                probe_slice_indexing_list[axis_index] = probe_slice_index
                probe_slice_indexing = tuple(probe_slice_indexing_list)

                output_slice_indexing_list = [slice(None)] * scalars.ndim
                output_slice_indexing_list[axis_index] = output_slice_index
                output_slice_indexing = tuple(output_slice_indexing_list)

                scalars_slice = scalars[dataset_slice_indexing]
                probe_slice = probe_scalars[probe_slice_indexing]

                prb_sz = np.shape(probe_slice)
                pw = probe_kernel
                psf = probe_slice[
                    prb_sz[0]//2-pw//2:prb_sz[0]//2-pw//2+pw,
                    prb_sz[1]//2-pw//2:prb_sz[1]//2-pw//2+pw
                ]**2
                psf = psf/np.sum(psf.ravel())

                w, scaled_slice, loss, rdiff = deconv(scalars_slice, psf, scale, mu, [fast_axis_scanning, slow_axis_scanning], method, max_iter)

                output_scalars[output_slice_indexing] = w


            self.progress.value = n_slices

            dataset.set_scalars(name, output_scalars)

        
        # Remove scalars that were not selected from the output dataset
        for scalar_name in (all_scalars_set - selected_scalars_set):
            dataset.remove_scalars(scalar_name)
        
        # Set the scan ids on the output
        if dataset_scan_ids is not None:
            dataset.scan_ids = np.array(output_scan_ids)
        
        # Set the tilt angles on the output
        if dataset_tilt_angles is not None:
            dataset.tilt_angles = np.array(output_tilt_angles)

