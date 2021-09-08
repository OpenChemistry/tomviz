
//
//  ctvlib.cpp
//
//
//  Created by Hovden Group on 5/6/19.
//  Copyright © 2019 Jonathan Schwartz. All rights reserved.
//

#include "ctvlib.h"

#include <vtk_eigen.h>

#include VTK_EIGEN(CORE)
#include VTK_EIGEN(SparseCore)

#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>

#include <random>

typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
  Mat;

ctvlib::ctvlib(int Ns, int Nray, int Nproj)
{
  // Intialize all the Member variables.
  Nslice = Ns;
  Ny = Nray;
  Nz = Nray;
  Nrow = Nray * Nproj;
  Ncol = Ny * Nz;

  A.resize(Nrow, Ncol);
  b.resize(Nslice, Nrow);
  g.resize(Nslice, Nrow);

  // Initialize all the Slices in Recon as Zero.
  recon = new Eigen::VectorXf[Nslice]; // Final Reconstruction.

  // Initialize the 3D Matrices as zeros.
  tbb::parallel_for(0, Nslice, 1,
                    [&](int s) { recon[s] = Eigen::VectorXf::Zero(Ny * Nz); });
}

int ctvlib::get_Nslice()
{
  return Nslice;
}

int ctvlib::get_Nray()
{
  return Ny;
}

// Temporary copy for measuring 3D TV - Derivative.
void ctvlib::initialize_recon_copy()
{
  temp_recon = new Eigen::VectorXf[Nslice];

  tbb::parallel_for(0, Nslice, 1, [&](int s) {
    temp_recon[s] = Eigen::VectorXf::Zero(Ny * Nz);
  });
}

// Temporary copy for measuring 3D TV - Derivative.
void ctvlib::initialize_tv_recon()
{
  tv_recon = new Eigen::VectorXf[Nslice];

  tbb::parallel_for(
    0, Nslice, 1, [&](int s) { tv_recon[s] = Eigen::VectorXf::Zero(Ny * Nz); });
}

// Import tilt series (projections) from Python.
void ctvlib::set_tilt_series(Mat inData)
{
  b = inData;
}

// Regular or Stochastic ART Reconstruction.
void ctvlib::ART(float beta)
{
  tbb::parallel_for(0, Nslice, 1, [&](int s) {
    for (int j = 0; j < Nrow; j++) {
      float a = (b(s, j) - A.row(j).dot(recon[s])) / innerProduct(j);
      recon[s] += A.row(j).transpose() * a * beta;
    }
  });

  positivity();
}

// Regular or Stochastic ART Reconstruction.
void ctvlib::randART(float beta)
{
  std::vector<int> A_index = calc_proj_order(Nrow);

  tbb::parallel_for(0, Nslice, 1, [&](int s) {
    for (int j = 0; j < Nrow; j++) {
      j = A_index[j];
      float a = (b(s, j) - A.row(j).dot(recon[s])) / innerProduct(j);
      recon[s] += A.row(j).transpose() * a * beta;
    }
  });

  positivity();
}

std::vector<int> ctvlib::calc_proj_order(int n)
{
  std::vector<int> a(n);
  for (int i = 0; i < n; i++) {
    a[i] = i;
  }

  std::random_device rd;
  std::mt19937 gRd(rd());
  shuffle(a.begin(), a.end(), gRd);

  return a;
}

// Calculate Lipshits Gradient (for SIRT).
float ctvlib::lipschits()
{
  Eigen::VectorXf f = Eigen::VectorXf::Random(Ncol);
  for (int i = 0; i < 15; i++) {
    f = A.transpose() * (A * f) / f.norm();
  }
  return f.norm();
}

// SIRT Reconstruction.
void ctvlib::SIRT(float beta)
{
  tbb::parallel_for(0, Nslice, 1, [&](int s) {
    recon[s] += A.transpose() * (b.row(s).transpose() - A * recon[s]) * beta;
  });
  positivity();
}

// Remove Negative Voxels.
void ctvlib::positivity()
{
  tbb::parallel_for(0, Nslice, 1, [&](int i) {
    recon[i] = (recon[i].array() < 0).select(0, recon[i]);
  });
}

// Row Inner Product of Measurement Matrix.
void ctvlib::normalization()
{
  innerProduct.resize(Nrow);

  tbb::parallel_for(0, Nrow, 1,
                    [&](int i) { innerProduct(i) = A.row(i).dot(A.row(i)); });
}

// Create Local Copy of Reconstruction.
void ctvlib::copy_recon()
{
  memcpy(temp_recon, recon, sizeof(recon));
}

// Measure the 2 norm between temporary and current reconstruction.
float ctvlib::matrix_2norm()
{
  float L2;
  L2 = tbb::parallel_reduce(
    tbb::blocked_range<int>(0, Nslice), 0.0,
    [&](tbb::blocked_range<int>& r, float L2loc) {
      for (int s = r.begin(); s < r.end(); ++s) {
        L2loc += (recon[s].array() - temp_recon[s].array()).square().sum();
      }
      return L2loc;
    },
    std::plus<float>());
  return sqrt(L2);
}

// Measure the 2 norm between experimental and reconstructed projections.
float ctvlib::data_distance()
{
  forward_projection();
  return (g - b).norm() / g.size(); // Nrow*Nslice,sum_{ij} M_ij^2 / Nrow*Nslice
}

// Foward project the data.
void ctvlib::forward_projection()
{
  tbb::parallel_for(0, Nslice, 1, [&](int s) {
    for (int i = 0; i < Nrow; i++) {
      g(s, i) = A.row(i).dot(recon[s]);
    }
  });
}

// Load Measurement Matrix from Python.
void ctvlib::loadA(Eigen::Ref<Mat> pyA)
{
  for (int i = 0; i < pyA.cols(); i++) {
    A.coeffRef(pyA(0, i), pyA(1, i)) += pyA(2, i);
  }
}

void ctvlib::update_proj_angles(Eigen::Ref<Mat> pyA, int Nproj)
{
  Nrow = Ny * Nproj;

  A.resize(Nrow, Ncol);
  b.resize(Nslice, Nrow);
  g.resize(Nslice, Nrow);

  for (int i = 0; i < pyA.cols(); i++) {
    A.coeffRef(pyA(0, i), pyA(1, i)) += pyA(2, i);
  }
}

// TV Minimization (Gradient Descent)
void ctvlib::tv_gd_3D(int ng, float dPOCS)
{
  float tvNorm, eps = 1e-8;

  // Calculate TV Derivative Tensor.
  for (int gIter = 0; gIter < ng; gIter++) {
    tvNorm = tbb::parallel_reduce(
      tbb::blocked_range<int>(0, Nslice), 0.0,
      [&](tbb::blocked_range<int>& r, float tvNormLoc) {
        for (int i = r.begin(); i < r.end(); ++i) {
          int ip = (i + 1) % Nslice;
          int im = (i - 1 + Nslice) % Nslice;
          for (int j = 0; j < Ny; j++) {
            for (int k = 0; k < Nz; k++) {

              int jk = j * Ny + k;
              int jp = ((j + 1) % Ny) * Ny + k;
              int jm = ((j - 1 + Ny) % Ny) * Ny + k;

              int kp = j * Ny + (k + 1) % Nz;
              int km = j * Ny + (k - 1 + Nz) % Nz;

              int jm_kp = ((j - 1 + Ny) % Ny) * Ny + (k + 1) % Nz;
              int jp_km = ((j + 1) % Ny) * Ny + (k - 1 + Nz) % Nz;

              float v1n = 3.0 * recon[i](jk) - recon[ip](jk) - recon[i](jp) -
                          recon[i](kp);
              float v1d = sqrt(
                eps +
                (recon[i](jk) - recon[ip](jk)) *
                  (recon[i](jk) - recon[ip](jk)) +
                (recon[i](jk) - recon[i](jp)) * (recon[i](jk) - recon[i](jp)) +
                (recon[i](jk) - recon[i](kp)) * (recon[i](jk) - recon[i](kp)));
              float v2n = recon[i](jk) - recon[im](jk);
              float v2d = sqrt(eps +
                               (recon[im](jk) - recon[i](jk)) *
                                 (recon[im](jk) - recon[i](jk)) +
                               (recon[im](jk) - recon[im](jp)) *
                                 (recon[im](jk) - recon[im](jp)) +
                               (recon[im](jk) - recon[im](kp)) *
                                 (recon[im](jk) - recon[im](kp)));
              float v3n = recon[i](jk) - recon[i](jm);
              float v3d = sqrt(eps +
                               (recon[i](jm) - recon[ip](jm)) *
                                 (recon[i](jm) - recon[ip](jm)) +
                               (recon[i](jm) - recon[i](jk)) *
                                 (recon[i](jm) - recon[i](jk)) +
                               (recon[i](jm) - recon[i](jm_kp)) *
                                 (recon[i](jm) - recon[i](jm_kp)));
              float v4n = recon[i](jk) - recon[i](km);
              float v4d = sqrt(eps +
                               (recon[i](km) - recon[ip](km)) *
                                 (recon[i](km) - recon[ip](km)) +
                               (recon[i](km) - recon[i](jp_km)) *
                                 (recon[i](km) - recon[i](jp_km)) +
                               (recon[i](km) - recon[i](jk)) *
                                 (recon[i](km) - recon[i](jk)));

              tv_recon[i](jk) = v1n / v1d + v2n / v2d + v3n / v3d + v4n / v4d;
              tvNormLoc += tv_recon[i](jk) * tv_recon[i](jk);
            }
          }
        }
        return tvNormLoc;
      },
      std::plus<float>());

    // Gradient Descent.
    tvNorm = sqrt(tvNorm);
    tbb::parallel_for(0, Nslice, 1,
                      [&](int l) { recon[l] -= dPOCS * tv_recon[l] / tvNorm; });
  }
  positivity();
}

// Return Reconstruction to Python.
Mat ctvlib::get_recon(int s)
{
  return recon[s];
}

// Return the projections.
Mat ctvlib::get_projections()
{
  return b;
}

// Restart the Reconstruction (Reset to Zero).
void ctvlib::restart_recon()
{
  for (int s = 0; s < Nslice; s++) {
     recon[s].setZero();
  }
}
