#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <immintrin.h>
#include <omp.h>

static inline int idx3(int x, int y, int z, int Nx, int Ny) {
    return (z * Ny + y) * Nx + x;
}

static void inicializar(double *u_prev, double *u, int Nx, int Ny, int Nz) {
    int total = Nx * Ny * Nz;

    for (int p = 0; p < total; p++) {
        u[p] = 0.0;
        u_prev[p] = 0.0;
    }

    double cx = 0.5 * (double)(Nx - 1);
    double cy = 0.5 * (double)(Ny - 1);
    double cz = 0.5 * (double)(Nz - 1);
    double sigma = 0.10 * (double)Nx;

    for (int z = 1; z < Nz - 1; z++) {
        for (int y = 1; y < Ny - 1; y++) {
            for (int x = 1; x < Nx - 1; x++) {
                double dx = ((double)x - cx) / sigma;
                double dy = ((double)y - cy) / sigma;
                double dz = ((double)z - cz) / sigma;

                double valor = exp(-(dx * dx + dy * dy + dz * dz));

                int p = idx3(x, y, z, Nx, Ny);
                u[p] = valor;
                u_prev[p] = valor;
            }
        }
    }
}

static void paso_scalar(const double *u_prev,
                        const double *u,
                        double *u_next,
                        int Nx,
                        int Ny,
                        int Nz,
                        double cfl2) {
    for (int z = 1; z < Nz - 1; z++) {
        for (int y = 1; y < Ny - 1; y++) {
            for (int x = 1; x < Nx - 1; x++) {
                int p = idx3(x, y, z, Nx, Ny);

                double lap =
                    u[idx3(x - 1, y, z, Nx, Ny)] +
                    u[idx3(x + 1, y, z, Nx, Ny)] +
                    u[idx3(x, y - 1, z, Nx, Ny)] +
                    u[idx3(x, y + 1, z, Nx, Ny)] +
                    u[idx3(x, y, z - 1, Nx, Ny)] +
                    u[idx3(x, y, z + 1, Nx, Ny)] -
                    6.0 * u[p];

                u_next[p] = 2.0 * u[p] - u_prev[p] + cfl2 * lap;
            }
        }
    }
}

static void paso_openmp(const double *u_prev,
                        const double *u,
                        double *u_next,
                        int Nx,
                        int Ny,
                        int Nz,
                        double cfl2) {
    #pragma omp parallel for collapse(2) schedule(static)
    for (int z = 1; z < Nz - 1; z++) {
        for (int y = 1; y < Ny - 1; y++) {
            for (int x = 1; x < Nx - 1; x++) {
                int p = idx3(x, y, z, Nx, Ny);

                double lap =
                    u[idx3(x - 1, y, z, Nx, Ny)] +
                    u[idx3(x + 1, y, z, Nx, Ny)] +
                    u[idx3(x, y - 1, z, Nx, Ny)] +
                    u[idx3(x, y + 1, z, Nx, Ny)] +
                    u[idx3(x, y, z - 1, Nx, Ny)] +
                    u[idx3(x, y, z + 1, Nx, Ny)] -
                    6.0 * u[p];

                u_next[p] = 2.0 * u[p] - u_prev[p] + cfl2 * lap;
            }
        }
    }
}

static void paso_avx(const double *u_prev,
                     const double *u,
                     double *u_next,
                     int Nx,
                     int Ny,
                     int Nz,
                     double cfl2) {
    __m256d dos = _mm256_set1_pd(2.0);
    __m256d seis = _mm256_set1_pd(6.0);
    __m256d a = _mm256_set1_pd(cfl2);

    int nvec = (Nx - 2) / 4;

    for (int z = 1; z < Nz - 1; z++) {
        for (int y = 1; y < Ny - 1; y++) {
            for (int b = 0; b < nvec; b++) {
                int x = 1 + 4 * b;
                int p = idx3(x, y, z, Nx, Ny);

                __m256d uc = _mm256_loadu_pd(&u[p]);
                __m256d old = _mm256_loadu_pd(&u_prev[p]);

                __m256d xm = _mm256_loadu_pd(&u[idx3(x - 1, y, z, Nx, Ny)]);
                __m256d xp = _mm256_loadu_pd(&u[idx3(x + 1, y, z, Nx, Ny)]);
                __m256d ym = _mm256_loadu_pd(&u[idx3(x, y - 1, z, Nx, Ny)]);
                __m256d yp = _mm256_loadu_pd(&u[idx3(x, y + 1, z, Nx, Ny)]);
                __m256d zm = _mm256_loadu_pd(&u[idx3(x, y, z - 1, Nx, Ny)]);
                __m256d zp = _mm256_loadu_pd(&u[idx3(x, y, z + 1, Nx, Ny)]);

                __m256d lap = _mm256_add_pd(xm, xp);
                lap = _mm256_add_pd(lap, ym);
                lap = _mm256_add_pd(lap, yp);
                lap = _mm256_add_pd(lap, zm);
                lap = _mm256_add_pd(lap, zp);
                lap = _mm256_sub_pd(lap, _mm256_mul_pd(seis, uc));

                __m256d next = _mm256_sub_pd(_mm256_mul_pd(dos, uc), old);
                next = _mm256_add_pd(next, _mm256_mul_pd(a, lap));

                _mm256_storeu_pd(&u_next[p], next);
            }

            for (int x = 1 + 4 * nvec; x < Nx - 1; x++) {
                int p = idx3(x, y, z, Nx, Ny);

                double lap =
                    u[idx3(x - 1, y, z, Nx, Ny)] +
                    u[idx3(x + 1, y, z, Nx, Ny)] +
                    u[idx3(x, y - 1, z, Nx, Ny)] +
                    u[idx3(x, y + 1, z, Nx, Ny)] +
                    u[idx3(x, y, z - 1, Nx, Ny)] +
                    u[idx3(x, y, z + 1, Nx, Ny)] -
                    6.0 * u[p];

                u_next[p] = 2.0 * u[p] - u_prev[p] + cfl2 * lap;
            }
        }
    }
}

static void paso_openmp_avx(const double *u_prev,
                            const double *u,
                            double *u_next,
                            int Nx,
                            int Ny,
                            int Nz,
                            double cfl2) {
    __m256d dos = _mm256_set1_pd(2.0);
    __m256d seis = _mm256_set1_pd(6.0);
    __m256d a = _mm256_set1_pd(cfl2);

    int nvec = (Nx - 2) / 4;

    #pragma omp parallel for collapse(2) schedule(static)
    for (int z = 1; z < Nz - 1; z++) {
        for (int y = 1; y < Ny - 1; y++) {
            for (int b = 0; b < nvec; b++) {
                int x = 1 + 4 * b;
                int p = idx3(x, y, z, Nx, Ny);

                __m256d uc = _mm256_loadu_pd(&u[p]);
                __m256d old = _mm256_loadu_pd(&u_prev[p]);

                __m256d xm = _mm256_loadu_pd(&u[idx3(x - 1, y, z, Nx, Ny)]);
                __m256d xp = _mm256_loadu_pd(&u[idx3(x + 1, y, z, Nx, Ny)]);
                __m256d ym = _mm256_loadu_pd(&u[idx3(x, y - 1, z, Nx, Ny)]);
                __m256d yp = _mm256_loadu_pd(&u[idx3(x, y + 1, z, Nx, Ny)]);
                __m256d zm = _mm256_loadu_pd(&u[idx3(x, y, z - 1, Nx, Ny)]);
                __m256d zp = _mm256_loadu_pd(&u[idx3(x, y, z + 1, Nx, Ny)]);

                __m256d lap = _mm256_add_pd(xm, xp);
                lap = _mm256_add_pd(lap, ym);
                lap = _mm256_add_pd(lap, yp);
                lap = _mm256_add_pd(lap, zm);
                lap = _mm256_add_pd(lap, zp);
                lap = _mm256_sub_pd(lap, _mm256_mul_pd(seis, uc));

                __m256d next = _mm256_sub_pd(_mm256_mul_pd(dos, uc), old);
                next = _mm256_add_pd(next, _mm256_mul_pd(a, lap));

                _mm256_storeu_pd(&u_next[p], next);
            }
        }
    }
}

static double checksum(const double *u, int total) {
    double s = 0.0;

    #pragma omp parallel for reduction(+:s)
    for (int p = 0; p < total; p++) {
        s += fabs(u[p]);
    }

    return s;
}

static void guardar_slice(FILE *file,
                          const double *u,
                          int Nx,
                          int Ny,
                          int Nz,
                          int step) {
    int z = Nz / 2;

    for (int y = 0; y < Ny; y++) {
        for (int x = 0; x < Nx; x++) {
            int p = idx3(x, y, z, Nx, Ny);
            fprintf(file, "%d,%d,%d,%.12e\n", step, x, y, u[p]);
        }
    }
}

static double ejecutar_modo(const char *modo,
                            int Nx,
                            int Ny,
                            int Nz,
                            int steps,
                            double cfl,
                            int guardar,
                            double *chk) {
    int total = Nx * Ny * Nz;

    double *u_prev = malloc((size_t)total * sizeof(double));
    double *u = malloc((size_t)total * sizeof(double));
    double *u_next = malloc((size_t)total * sizeof(double));

    if (u_prev == NULL || u == NULL || u_next == NULL) {
        printf("Error de memoria en 3D.\n");
        exit(1);
    }

    inicializar(u_prev, u, Nx, Ny, Nz);
    memset(u_next, 0, (size_t)total * sizeof(double));

    double cfl2 = cfl * cfl;

    FILE *snap = NULL;

    if (guardar) {
        snap = fopen("resultados/wave3d_slice_snapshots.csv", "w");
        fprintf(snap, "step,x,y,u\n");
        guardar_slice(snap, u, Nx, Ny, Nz, 0);
    }

    double t0 = omp_get_wtime();

    for (int n = 1; n <= steps; n++) {
        memset(u_next, 0, (size_t)total * sizeof(double));

        if (strcmp(modo, "scalar") == 0) {
            paso_scalar(u_prev, u, u_next, Nx, Ny, Nz, cfl2);
        } else if (strcmp(modo, "openmp") == 0) {
            paso_openmp(u_prev, u, u_next, Nx, Ny, Nz, cfl2);
        } else if (strcmp(modo, "avx") == 0) {
            paso_avx(u_prev, u, u_next, Nx, Ny, Nz, cfl2);
        } else {
            paso_openmp_avx(u_prev, u, u_next, Nx, Ny, Nz, cfl2);
        }

        double *tmp = u_prev;
        u_prev = u;
        u = u_next;
        u_next = tmp;

        if (guardar && n % 4 == 0) {
            guardar_slice(snap, u, Nx, Ny, Nz, n);
        }
    }

    double t1 = omp_get_wtime();

    if (snap != NULL) {
        fclose(snap);
    }

    *chk = checksum(u, total);

    free(u_prev);
    free(u);
    free(u_next);

    return t1 - t0;
}

int main(void) {
    int Nx = 48;
    int Ny = 48;
    int Nz = 48;
    int steps = 220;
    double cfl = 0.25;

    FILE *file = fopen("resultados/wave3d_benchmark.csv", "w");

    if (file == NULL) {
        printf("No se pudo abrir CSV 3D.\n");
        return 1;
    }

    fprintf(file, "mode,Nx,Ny,Nz,steps,cfl,threads,time_seconds,checksum\n");

    const char *modos[] = {"scalar", "openmp", "avx", "openmp_avx"};

    printf("Onda 3D por diferencias finitas\n");
    printf("Nx = %d, Ny = %d, Nz = %d, steps = %d, CFL = %.3f\n",
           Nx, Ny, Nz, steps, cfl);
    printf("Threads = %d\n\n", omp_get_max_threads());

    for (int m = 0; m < 4; m++) {
        double chk = 0.0;
        int guardar = strcmp(modos[m], "scalar") == 0;

        double tiempo = ejecutar_modo(modos[m], Nx, Ny, Nz, steps, cfl, guardar, &chk);

        fprintf(file,
                "%s,%d,%d,%d,%d,%.6f,%d,%.10f,%.12e\n",
                modos[m], Nx, Ny, Nz, steps, cfl,
                omp_get_max_threads(), tiempo, chk);

        printf("%-12s tiempo = %.6f s | checksum = %.8e\n",
               modos[m], tiempo, chk);
    }

    fclose(file);

    printf("\nArchivos 3D generados.\n");

    return 0;
}
