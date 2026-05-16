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

double energy(const int *spins, int L, double J, double B) {
    double E = 0.0;

    #pragma omp parallel for collapse(2) reduction(+:E)
    for (int i = 0; i < L; i++) {
        for (int j = 0; j < L; j++) {
            int s = spins[index_2d(i, j, L)];

            int right = spins[index_2d(i, (j + 1) % L, L)];
            int down  = spins[index_2d((i + 1) % L, j, L)];

            E += -J * s * (right + down);
            E += -B * s;
        }
    }

    return E;
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

int main(void) {
    int L = 16;
    int N = L * L;

    double J = 1.0;
    double B = 0.0;
    double T = 2.0;

    int total_sweeps = 800;

    int *spins = malloc((size_t)N * sizeof(int));

    if (spins == NULL) {
        printf("Error reservando memoria.\n");
        return 1;
    }

    initialize_random_spins(spins, L, 12345ULL);

    RNG rng;
    rng.state = 98765ULL;

    FILE *file = fopen("resultados/magnetizacion_iteraciones_L16_T2.csv", "w");

    if (file == NULL) {
        printf("No se pudo abrir el archivo de salida.\n");
        free(spins);
        return 1;
    }

    fprintf(file,
            "sweep,iteration,magnetization,magnetization_per_site,"
            "abs_magnetization_per_site,energy_per_site,acceptance_rate\n");

    printf("Grafico 1: magnetizacion por sitio vs iteracion\n");
    printf("L = %d, T = %.2f, J = %.2f, B = %.2f\n", L, T, J, B);
    printf("Iteraciones totales = %d\n", total_sweeps * N);
    printf("Threads OpenMP = %d\n\n", omp_get_max_threads());

    double t0 = omp_get_wtime();

    {
        long long M = magnetization(spins, N);
        double E = energy(spins, L, J, B);

        fprintf(file,
                "%d,%d,%lld,%.10f,%.10f,%.10f,%.10f\n",
                0,
                0,
                M,
                (double)M / (double)N,
                fabs((double)M) / (double)N,
                E / (double)N,
                0.0);
    }

    for (int sweep = 1; sweep <= total_sweeps; sweep++) {
        int accepted = metropolis_sweep(spins, L, T, J, B, &rng);

        int iteration = sweep * N;

        long long M = magnetization(spins, N);
        double E = energy(spins, L, J, B);
        double acceptance_rate = (double)accepted / (double)N;

        fprintf(file,
                "%d,%d,%lld,%.10f,%.10f,%.10f,%.10f\n",
                sweep,
                iteration,
                M,
                (double)M / (double)N,
                fabs((double)M) / (double)N,
                E / (double)N,
                acceptance_rate);
    }

    double t1 = omp_get_wtime();

    fclose(file);
    free(spins);

    printf("Archivo generado: resultados/magnetizacion_iteraciones_L16_T2.csv\n");
    printf("Tiempo total = %.6f s\n", t1 - t0);

    return 0;
}
