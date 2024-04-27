#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>

class PerlinNoise {
private:
    std::vector<int> perm;

    double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    double lerp(double t, double a, double b) { return a + t * (b - a); }
    double grad(int hash, double x, double y) {
        int h = hash & 15;
        double u = h < 8 ? x : y;
        double v = h < 4 ? y : h == 12 || h == 14 ? x : 0;
        return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
    }

public:
    PerlinNoise(uint64_t seed) {
        perm.resize(256);
        std::iota(perm.begin(), perm.end(), 0);
        std::default_random_engine engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        perm.insert(perm.end(), perm.begin(), perm.end());
    }

    double noise(double x, double y) {
        int xi = (int) std::floor(x) & 255;
        int yi = (int) std::floor(y) & 255;

        double xf = x - std::floor(x);
        double yf = y - std::floor(y);

        double u = fade(xf);
        double v = fade(yf);

        int aa = perm[perm[xi] + yi];
        int ab = perm[perm[xi] + yi + 1];
        int ba = perm[perm[xi + 1] + yi];
        int bb = perm[perm[xi + 1] + yi + 1];

        double x1 = lerp(u, grad(aa, xf, yf), grad(ba, xf - 1, yf));
        double x2 = lerp(u, grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1));

        return (lerp(v, x1, x2) + 1) / 2;
    }

    int generateValue(double x, double y) {
        double n = noise(x, y);
        return static_cast<int>(n * 255);
    }
};
