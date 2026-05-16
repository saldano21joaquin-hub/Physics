#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <immintrin.h>
#include <omp.h>

static void inicializar(double *u_prev, double *u, int N) {
    double centro = 0.5 * (double)(N - 1);
    double sigma = 0.055 * (double)N;

    for (int i = 0; i < N; i++) {
        double x = ((double)i - centro) / sigma;
        u[i] = exp(-x * x);
        u_prev[i] = u[i];
    }

    u[0] = 0.0;
    u[N - 1] = 0.0;
    u_prev[0] = 0.0;
    u_prev[N - 1] = 0.0;
}

static void paso_scalar(const double *u_prev,
                        const double *u,
                        double *u_next,
                        int N,
                        double cfl2) {
    u_next[0] = 0.0;
    u_next[N - 1] = 0.0;

    for (int i = 1; i < N - 1; i++) {
        double lap = u[i - 1] - 2.0 * u[i] + u[i + 1];
        u_next[i] = 2.0 * u[i] - u_prev[i] + cfl2 * lap;
    }
}

static void paso_openmp(const double *u_prev,
                        const double *u,
                        double *u_next,
                        int N,
                        double cfl2) {
    u_next[0] = 0.0;
    u_next[N - 1] = 0.0;

    #pragma omp parallel for schedule(static)
    for (int i = 1; i < N - 1; i++) {
        double lap = u[i - 1] - 2.0 * u[i] + u[i + 1];
        u_next[i] = 2.0 * u[i] - u_prev[i] + cfl2 * lap;
    }
}

static void paso_avx(const double *u_prev,
                     const double *u,
                     double *u_next,
                     int N,
                     double cfl2) {
    u_next[0] = 0.0;
    u_next[N - 1] = 0.0;

    __m256d dos = _mm256_set1_pd(2.0);
    __m256d a = _mm256_set1_pd(cfl2);

    int interior = N - 2;
    int bloques = interior / 4;

    for (int b = 0; b < bloques; b++) {
        int i = 1 + 4 * b;

        __m256d um = _mm256_loadu_pd(&u[i - 1]);
        __m256d uc = _mm256_loadu_pd(&u[i]);
        __m256d up = _mm256_loadu_pd(&u[i + 1]);
        __m256d old = _mm256_loadu_pd(&u_prev[i]);

        __m256d lap = _mm256_add_pd(um, up);
        lap = _mm256_sub_pd(lap, _mm256_mul_pd(dos, uc));

        __m256d next = _mm256_sub_pd(_mm256_mul_pd(dos, uc), old);
        next = _mm256_add_pd(next, _mm256_mul_pd(a, lap));

        _mm256_storeu_pd(&u_next[i], next);
    }

    for (int i = 1 + 4 * bloques; i < N - 1; i++) {
        double lap = u[i - 1] - 2.0 * u[i] + u[i + 1];
        u_next[i] = 2.0 * u[i] - u_prev[i] + cfl2 * lap;
    }
}

static void paso_openmp_avx(const double *u_prev,
                            const double *u,
                            double *u_next,
                            int N,
                            double cfl2) {
    u_next[0] = 0.0;
    u_next[N - 1] = 0.0;

    __m256d dos = _mm256_set1_pd(2.0);
    __m256d a = _mm256_set1_pd(cfl2);

    int interior = N - 2;
    int bloques = interior / 4;

    #pragma omp parallel for schedule(static)
    for (int b = 0; b < bloques; b++) {
        int i = 1 + 4 * b;

        __m256d um = _mm256_loadu_pd(&u[i - 1]);
        __m256d uc = _mm256_loadu_pd(&u[i]);
        __m256d up = _mm256_loadu_pd(&u[i + 1]);
        __m256d old = _mm256_loadu_pd(&u_prev[i]);

        __m256d lap = _mm256_add_pd(um, up);
        lap = _mm256_sub_pd(lap, _mm256_mul_pd(dos, uc));

        __m256d next = _mm256_sub_pd(_mm256_mul_pd(dos, uc), old);
        next = _mm256_add_pd(next, _mm256_mul_pd(a, lap));

        _mm256_storeu_pd(&u_next[i], next);
    }

    #pragma omp parallel for schedule(static)
    for (int i = 1 + 4 * bloques; i < N - 1; i++) {
        double lap = u[i - 1] - 2.0 * u[i] + u[i + 1];
        u_next[i] = 2.0 * u[i] - u_prev[i] + cfl2 * lap;
    }
}

static double checksum(const double *u, int N) {
    double s = 0.0;

    #pragma omp parallel for reduction(+:s)
    for (int i = 0; i < N; i++) {
        s += fabs(u[i]);
    }

    return s;
}

static void guardar_snapshot(FILE *file, const double *u, int N, int step) {
    for (int i = 0; i < N; i += 4) {
        double x = (double)i / (double)(N - 1);
        fprintf(file, "%d,%d,%.12f,%.12e\n", step, i, x, u[i]);
    }
}

static double ejecutar_modo(const char *modo,
                            int N,
                            int steps,
                            double cfl,
                            int guardar,
                            double *chk) {
    double *u_prev = malloc((size_t)N * sizeof(double));
    double *u = malloc((size_t)N * sizeof(double));
    double *u_next = malloc((size_t)N * sizeof(double));

    if (u_prev == NULL || u == NULL || u_next == NULL) {
        printf("Error de memoria en 1D.\n");
        exit(1);
    }

    inicializar(u_prev, u, N);

    double cfl2 = cfl * cfl;

    FILE *snap = NULL;

    if (guardar) {
        snap = fopen("resultados/wave1d_snapshots.csv", "w");
        fprintf(snap, "step,i,x,u\n");
        guardar_snapshot(snap, u, N, 0);
    }

    double t0 = omp_get_wtime();

    for (int n = 1; n <= steps; n++) {
        if (strcmp(modo, "scalar") == 0) {
            paso_scalar(u_prev, u, u_next, N, cfl2);
        } else if (strcmp(modo, "openmp") == 0) {
            paso_openmp(u_prev, u, u_next, N, cfl2);
        } else if (strcmp(modo, "avx") == 0) {
            paso_avx(u_prev, u, u_next, N, cfl2);
        } else {
            paso_openmp_avx(u_prev, u, u_next, N, cfl2);
        }

        double *tmp = u_prev;
        u_prev = u;
        u = u_next;
        u_next = tmp;

        if (guardar && n % 6 == 0) {
            guardar_snapshot(snap, u, N, n);
        }
    }

    double t1 = omp_get_wtime();

    if (snap != NULL) {
        fclose(snap);
    }

    *chk = checksum(u, N);

    free(u_prev);
    free(u);
    free(u_next);

    return t1 - t0;
}

int main(void) {
    int N = 4096;
    int steps = 720;
    double cfl = 0.5;

    FILE *file = fopen("resultados/wave1d_benchmark.csv", "w");

    if (file == NULL) {
        printf("No se pudo abrir CSV 1D.\n");
        return 1;
    }

    fprintf(file, "mode,N,steps,cfl,threads,time_seconds,checksum\n");

    const char *modos[] = {"scalar", "openmp", "avx", "openmp_avx"};

    printf("Onda 1D por diferencias finitas\n");
    printf("N = %d, steps = %d, CFL = %.3f\n", N, steps, cfl);
    printf("Threads = %d\n\n", omp_get_max_threads());

    for (int m = 0; m < 4; m++) {
        double chk = 0.0;
        int guardar = strcmp(modos[m], "scalar") == 0;

        double tiempo = ejecutar_modo(modos[m], N, steps, cfl, guardar, &chk);

        fprintf(file,
                "%s,%d,%d,%.6f,%d,%.10f,%.12e\n",
                modos[m], N, steps, cfl,
                omp_get_max_threads(), tiempo, chk);

        printf("%-12s tiempo = %.6f s | checksum = %.8e\n",
               modos[m], tiempo, chk);
    }

    fclose(file);

    printf("\nArchivos 1D generados.\n");

    return 0;
}
