#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace dkc
{
    struct PitchResult
    {
        bool valid = false;
        float midiNote = 0.0f;     // nota MIDI con decimales (60 = Do central)
        float confidence = 0.0f;   // 0..1
    };

    // Detecta el tono de un sample (kick 808, bajo, tom, etc.) con el algoritmo YIN.
    // Para sonidos sin tono claro (hats, snares ruidosos) devuelve valid = false.
    inline PitchResult detectPitch (const juce::AudioBuffer<float>& buf, double sr)
    {
        PitchResult res;
        const int n = buf.getNumSamples();
        const int nch = buf.getNumChannels();
        if (n < 2048 || nch < 1 || sr <= 0.0)
            return res;

        // Mezcla a mono
        std::vector<float> mono ((size_t) n, 0.0f);
        for (int c = 0; c < nch; ++c)
        {
            const float* d = buf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                mono[(size_t) i] += d[i] / (float) nch;
        }

        // Punto mas fuerte del sample
        int peak = 0;
        float pv = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (mono[(size_t) i]);
            if (a > pv) { pv = a; peak = i; }
        }
        if (pv < 1.0e-4f)
            return res;

        // Decimar x4 (promedio) para ir mas rapido: rango de busqueda 30 Hz - 1 kHz
        const int dec = 4;
        const double sd = sr / dec;
        const int tauMin = std::max (2, (int) (sd / 1000.0));
        const int tauMax = (int) (sd / 30.0);
        int W = 1024;

        int s0 = peak + (int) (0.02 * sr);       // saltamos el "click" inicial
        int needDec = W + tauMax;
        if (s0 + needDec * dec > n)
            s0 = n - needDec * dec;
        if (s0 < 0)
        {
            s0 = 0;
            W = n / dec - tauMax;
            if (W < 256) return res;
            needDec = W + tauMax;
        }

        std::vector<float> x ((size_t) needDec);
        for (int i = 0; i < needDec; ++i)
        {
            float sum = 0.0f;
            for (int k = 0; k < dec; ++k)
                sum += mono[(size_t) (s0 + i * dec + k)];
            x[(size_t) i] = sum / (float) dec;
        }

        // YIN: funcion de diferencia + normalizacion acumulada
        std::vector<float> d ((size_t) tauMax + 1, 0.0f), cmnd ((size_t) tauMax + 1, 1.0f);
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < W; ++j)
            {
                const float diff = x[(size_t) j] - x[(size_t) (j + tau)];
                sum += (double) diff * diff;
            }
            d[(size_t) tau] = (float) sum;
        }

        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            float m = 1.0e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (cmnd[(size_t) tau] < m) { m = cmnd[(size_t) tau]; best = tau; }
            if (best < 0 || m > 0.30f)
                return res;
        }

        // Interpolacion parabolica para afinar el periodo
        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float a = cmnd[(size_t) (best - 1)], b = cmnd[(size_t) best], c = cmnd[(size_t) (best + 1)];
            const float den = a + c - 2.0f * b;
            if (std::abs (den) > 1.0e-9f)
                tauF = (float) best + 0.5f * (a - c) / den;
        }

        const double freq = sd / (double) tauF;
        res.midiNote = (float) (69.0 + 12.0 * std::log2 (freq / 440.0));
        res.confidence = 1.0f - cmnd[(size_t) best];
        res.valid = true;
        return res;
    }
}
