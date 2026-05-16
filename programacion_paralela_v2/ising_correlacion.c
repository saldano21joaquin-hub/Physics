#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <omp.h>

typedef struct {
    uint64_t state;
} RNG;

static uint64_t rng_next(RNG *rng) {
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * 2685821657736338717ULL;
}

static double rng_uniform(RNG *rng) {
    return (rng_next(rng) >> 11) * (1.0 / 9007199254740992.0);
}

static int rng_int(RNG *rng, int n) {
    return (int)(rng_uniform(rng) * n);
}

int index_2d(int i, int j, int L) {
    return i * L + j;
}

void initialize_random_spins(int *spins, int L, uint64_t seed) {
    int N = L * L;
    RNG rng;
    rng.state = seed;

    for (int k = 0; k < N; k++) {
        if (rng_uniform(&rng) < 0.5) {
            spins[k] = -1;
        } else {
            spins[k] = 1;
        }
    }
}

long long magnetization(const int *spins, int N) {
    long long M = 0;

    #pragma omp parallel for reduction(+:M)
    for (int k = 0; k < N; k++) {
        M += spins[k];
    }

    return M;
}

double delta_energy_flip(const int *spins,
                         int L,
                         int i,
                         int j,
                         double J,
                         double B) {
    int s = spins[index_2d(i, j, L)];

    int up    = spins[index_2d((i - 1 + L) % L, j, L)];
    int down  = spins[index_2d((i + 1) % L, j, L)];
    int left  = spins[index_2d(i, (j - 1 + L) % L, L)];
    int right = spins[index_2d(i, (j + 1) % L, L)];

    int neighbor_sum = up + down + left + right;

    return 2.0 * s * (J * neighbor_sum + B);
}

int metropolis_sweep(int *spins,
                     int L,
                     double T,
                     double J,
                     double B,
                     RNG *rng) {
    int N = L * L;
    int accepted = 0;

    double beta = 1.0 / T;

    for (int step = 0; step < N; step++) {
        int i = rng_int(rng, L);
        int j = rng_int(rng, L);

        double dE = delta_energy_flip(spins, L, i, j, J, B);

        int accept = 0;

        if (dE <= 0.0) {
            accept = 1;
        } else {
            double probability = exp(-beta * dE);
            double r = rng_uniform(rng);

            if (r < probability) {
                accept = 1;
            }
        }

        if (accept) {
            int k = index_2d(i, j, L);
            spins[k] = -spins[k];
            accepted++;
        }
    }

    return accepted;
}

void connected_correlation(const int *spins, int L, int max_R, double *C) {
    int N = L * L;

    long long M = magnetization(spins, N);
    double m = (double)M / (double)N;

    for (int R = 1; R <= max_R; R++) {
        double pair_sum = 0.0;

        #pragma omp parallel for collapse(2) reduction(+:pair_sum)
        for (int i = 0; i < L; i++) {
            for (int j = 0; j < L; j++) {
                int s = spins[index_2d(i, j, L)];

                int s_right = spins[index_2d(i, (j + R) % L, L)];
                int s_down  = spins[index_2d((i + R) % L, j, L)];

                pair_sum += (double)(s * s_right);
                pair_sum += (double)(s * s_down);
            }
        }

        double pair_average = pair_sum / (double)(2 * N);

        C[R] = pair_average - m * m;
    }
}

double estimate_xi(const double *C, int max_fit_R) {
    double x[32];
    double y[32];
    int n = 0;

    double previous = C[1];

    for (int R = 1; R <= max_fit_R; R++) {
        double value = C[R];

        if (value <= 1.0e-8) {
            continue;
        }

        if (R > 1 && value > 1.25 * previous) {
            continue;
        }

        x[n] = (double)R;
        y[n] = log(value);
        previous = value;
        n++;
    }

    if (n < 3) {
        return 0.0;
    }

    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;

    for (int i = 0; i < n; i++) {
        sx += x[i];
        sy += y[i];
        sxx += x[i] * x[i];
        sxy += x[i] * y[i];
    }

    double denominator = (double)n * sxx - sx * sx;

    if (fabs(denominator) < 1.0e-12) {
        return 0.0;
    }

    double slope = ((double)n * sxy - sx * sy) / denominator;

    if (slope >= -1.0e-8) {
        return 0.0;
    }

    double xi = -1.0 / slope;

    if (xi > 4.0) {
        xi = 4.0;
    }

    return xi;
}

int main(void) {
    int L = 32;
    int N = L * L;

    double J = 1.0;
    double B = 0.0;

    double T_min = 1.5;
    double T_max = 3.0;
    double dT = 0.05;

    int equilibration_sweeps = 3000;
    int measurement_sweeps = 6000;
    int measure_every = 20;

    int max_R = L / 2;
    int max_fit_R = 5;

    FILE *file = fopen("resultados/longitud_correlacion_L32.csv", "w");

    if (file == NULL) {
        printf("No se pudo abrir el archivo de salida.\n");
        return 1;
    }

    fprintf(file, "T,xi");
    for (int R = 1; R <= max_R; R++) {
        fprintf(file, ",C_R%d", R);
    }
    fprintf(file, "\n");

    printf("Grafico 2: longitud de correlacion vs temperatura\n");
    printf("L = %d, J = %.2f, B = %.2f\n", L, J, B);
    printf("Temperaturas: %.2f a %.2f con dT = %.2f\n", T_min, T_max, dT);
    printf("equilibration_sweeps = %d\n", equilibration_sweeps);
    printf("measurement_sweeps = %d\n", measurement_sweeps);
    printf("measure_every = %d\n", measure_every);
    printf("max_fit_R = %d\n", max_fit_R);
    printf("Threads OpenMP = %d\n\n", omp_get_max_threads());

    double t0 = omp_get_wtime();

    int number_T = (int)round((T_max - T_min) / dT) + 1;

    for (int nT = 0; nT < number_T; nT++) {
        double T = T_min + (double)nT * dT;

        int *spins = malloc((size_t)N * sizeof(int));
        double *C_sum = calloc((size_t)(max_R + 1), sizeof(double));
        double *C = calloc((size_t)(max_R + 1), sizeof(double));

        if (spins == NULL || C_sum == NULL || C == NULL) {
            printf("Error reservando memoria.\n");
            free(spins);
            free(C_sum);
            free(C);
            fclose(file);
            return 1;
        }

        uint64_t seed_spins = 54321ULL + (uint64_t)(1000 * nT);
        uint64_t seed_metropolis = 98765ULL + (uint64_t)(1000 * nT);

        initialize_random_spins(spins, L, seed_spins);

        RNG rng;
        rng.state = seed_metropolis;

        for (int sweep = 0; sweep < equilibration_sweeps; sweep++) {
            metropolis_sweep(spins, L, T, J, B, &rng);
        }

        int measurements = 0;

        for (int sweep = 1; sweep <= measurement_sweeps; sweep++) {
            metropolis_sweep(spins, L, T, J, B, &rng);

            if (sweep % measure_every == 0) {
                connected_correlation(spins, L, max_R, C);

                for (int R = 1; R <= max_R; R++) {
                    C_sum[R] += C[R];
                }

                measurements++;
            }
        }

        for (int R = 1; R <= max_R; R++) {
            C[R] = C_sum[R] / (double)measurements;
        }

        double xi = estimate_xi(C, max_fit_R);

        fprintf(file, "%.8f,%.8f", T, xi);
        for (int R = 1; R <= max_R; R++) {
            fprintf(file, ",%.10f", C[R]);
        }
        fprintf(file, "\n");

        printf("T = %.3f | xi = %.3f\n", T, xi);

        free(spins);
        free(C_sum);
        free(C);
    }

    double t1 = omp_get_wtime();

    fclose(file);

    printf("\nArchivo generado: resultados/longitud_correlacion_L32.csv\n");
    printf("Tiempo total = %.6f s\n", t1 - t0);

    return 0;
}
