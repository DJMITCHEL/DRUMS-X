#pragma once

#include <algorithm>
#include <cmath>

namespace dkc
{
    // Compresor de un solo mando (COMP). Es una curva estatica, asi que la interfaz
    // puede dibujar EXACTAMENTE como queda la onda despues de comprimir.
    struct CompParams
    {
        float thr = 1.0f, ratio = 1.0f, makeup = 1.0f;
    };

    inline CompParams makeComp (float amount)
    {
        CompParams c;
        amount = std::min (1.0f, std::max (0.0f, amount));
        if (amount < 0.001f) return c;

        const float thrDb = -3.0f - 27.0f * amount;
        c.thr = std::pow (10.0f, thrDb / 20.0f);
        c.ratio = 1.0f + 7.0f * amount;
        c.makeup = 1.0f / (c.thr + (1.0f - c.thr) / c.ratio);   // el pico sigue en 0 dB
        return c;
    }

    inline float compShape (float x, const CompParams& c)
    {
        const float a = std::abs (x);
        const float out = a > c.thr ? c.thr + (a - c.thr) / c.ratio : a;
        return std::copysign (out * c.makeup, x);
    }
}
