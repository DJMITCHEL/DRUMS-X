#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

// Motor de analisis: detecta golpes, los clasifica, agrupa los que suenan igual
// y aisla un one-shot de cada grupo. No depende de la interfaz ni del plugin.
namespace dkc
{
    enum Family { Kick = 0, Snare, Clap, Hat, OpenHat, Tom, Perc, Cymbal, numFamilies };

    inline const char* familyLabel (int f)
    {
        static const char* n[] = { "KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "TOM", "PERC", "CYMBAL" };
        return n[juce::jlimit (0, 7, f)];
    }

    struct Hit
    {
        int start = 0, nextStart = 0, family = Perc, cluster = -1;
        float strength = 0.0f;
        std::vector<float> feat;
    };

    struct PadSound
    {
        int family = 0, pad = 0, hitCount = 0;
        juce::String name;
        juce::AudioBuffer<float> clean;   // mediana de todos los golpes del grupo
        juce::AudioBuffer<float> raw;     // el golpe real mas representativo
    };

    struct AnalysisResult
    {
        double sampleRate = 44100.0;
        int numSamples = 0;
        juce::String name;
        std::vector<Hit> hits;
        int familyCount[numFamilies] = {};
        std::vector<PadSound> sounds;
        std::vector<float> peaks;         // para dibujar la onda
    };

    //==========================================================================
    namespace detail
    {
        constexpr int fftOrder = 11;
        constexpr int fftSize = 1 << fftOrder;   // 2048
        constexpr int hop = 512;
        constexpr int numBands = 20;

        struct Features
        {
            float fLow = 0, fMid = 0, fHi = 0, centroid = 0, lowCentroid = 0, flatness = 0, decay = 0, strength = 0;
            std::vector<float> vec;
        };

        inline std::vector<float> makeHann()
        {
            std::vector<float> w ((size_t) fftSize);
            for (int i = 0; i < fftSize; ++i)
                w[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi * i / (fftSize - 1));
            return w;
        }

        // Ventana plana al principio (para no matar el ataque) y que cae al final
        inline std::vector<float> makeHitWindow()
        {
            std::vector<float> w ((size_t) fftSize);
            const int flat = (int) (0.6 * fftSize);
            for (int i = 0; i < fftSize; ++i)
                w[(size_t) i] = i < flat ? 1.0f
                    : (float) (0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * (i - flat) / (double) (fftSize - flat))));
            return w;
        }

        inline void computeSpectrum (juce::dsp::FFT& fft, const std::vector<float>& win, const float* x, int avail,
                                     std::vector<float>& buf, std::vector<float>& power)
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            const int m = std::min (avail, fftSize);
            for (int i = 0; i < m; ++i)
                buf[(size_t) i] = x[i] * win[(size_t) i];

            fft.performRealOnlyForwardTransform (buf.data(), true);

            const int bins = fftSize / 2 + 1;
            power.resize ((size_t) bins);
            for (int k = 0; k < bins; ++k)
            {
                const float re = buf[(size_t) (2 * k)], im = buf[(size_t) (2 * k + 1)];
                power[(size_t) k] = re * re + im * im;
            }
        }

        inline bool extractFeatures (juce::dsp::FFT& fft, const std::vector<float>& win, std::vector<float>& buf,
                                     const std::vector<float>& mono, int start, int sliceLen, double sr, Features& out)
        {
            const int n = (int) mono.size();
            if (start < 0 || start >= n) return false;
            sliceLen = std::min (sliceLen, n - start);
            if (sliceLen < 512) return false;

            std::vector<float> power;
            computeSpectrum (fft, win, mono.data() + start, std::min (sliceLen, fftSize), buf, power);

            const int bins = fftSize / 2 + 1;
            const double binHz = sr / (double) fftSize;

            double b[6] = { 0, 0, 0, 0, 0, 0 };
            double total = 0, cNum = 0, cDen = 0, arith = 0, lNum = 0, lDen = 0;
            int cnt = 0;
            for (int k = 1; k < bins; ++k)
            {
                const double f = k * binHz, p = power[(size_t) k];
                if (f < 20.0) continue;
                if (f >= 16000.0) break;
                total += p;
                cNum += f * p;
                cDen += p;
                if (f < 500.0) { lNum += f * p; lDen += p; }
                const int band = f < 100 ? 0 : f < 300 ? 1 : f < 1000 ? 2 : f < 3000 ? 3 : f < 8000 ? 4 : 5;
                b[band] += p;
                if (f >= 100.0) { arith += p; ++cnt; }
            }
            if (total <= 1.0e-12 || cnt == 0) return false;

            out.fLow = (float) ((b[0] + b[1]) / total);
            out.fMid = (float) ((b[2] + b[3]) / total);
            out.fHi  = (float) ((b[4] + b[5]) / total);
            out.centroid = (float) (cNum / std::max (cDen, 1.0e-20));
            out.lowCentroid = (float) (lNum / std::max (lDen, 1.0e-20));

            arith /= cnt;
            double lg = 0.0;
            for (int k = 1; k < bins; ++k)
            {
                const double f = k * binHz;
                if (f < 100.0) continue;
                if (f >= 16000.0) break;
                lg += std::log ((double) power[(size_t) k] + 1.0e-6 * arith + 1.0e-20);
            }
            lg /= cnt;
            out.flatness = (float) (std::exp (lg) / (arith + 1.0e-20));

            // Espectro en dB para el vector de caracteristicas (agrupacion)
            std::vector<float> db ((size_t) bins);
            for (int k = 0; k < bins; ++k)
                db[(size_t) k] = 10.0f * std::log10 (power[(size_t) k] + 1.0e-12f);

            auto dbAt = [&] (double f)
            {
                const double pos = juce::jlimit (0.0, (double) (bins - 2), f / binHz);
                const int i0 = (int) pos;
                const double fr = pos - i0;
                return (float) (db[(size_t) i0] + (db[(size_t) i0 + 1] - db[(size_t) i0]) * fr);
            };

            out.vec.clear();
            const double fMin = 40.0, fMax = std::min (16000.0, 0.45 * sr);
            float meanDb = 0.0f;
            for (int i = 0; i < numBands; ++i)
            {
                const double e0 = fMin * std::pow (fMax / fMin, (double) i / numBands);
                const double e1 = fMin * std::pow (fMax / fMin, (double) (i + 1) / numBands);
                const float v = (dbAt (e0) + dbAt (std::sqrt (e0 * e1)) + dbAt (e1)) / 3.0f;
                out.vec.push_back (v);
                meanDb += v;
            }
            meanDb /= (float) numBands;
            for (auto& v : out.vec) v -= meanDb;

            // Envolvente en frames de 5 ms
            const int fr = std::max (1, (int) (0.005 * sr));
            const int F = std::min (sliceLen, (int) (1.5 * sr)) / fr;
            if (F < 4) return false;

            std::vector<float> rms ((size_t) F);
            for (int j = 0; j < F; ++j)
            {
                double s = 0.0;
                for (int i = 0; i < fr; ++i)
                {
                    const double v = mono[(size_t) (start + j * fr + i)];
                    s += v * v;
                }
                rms[(size_t) j] = (float) std::sqrt (s / fr);
            }

            int pi = 0;
            float peak = 0.0f;
            for (int j = 0; j < std::min (F, 8); ++j)
                if (rms[(size_t) j] > peak) { peak = rms[(size_t) j]; pi = j; }
            if (peak <= 1.0e-7f) return false;

            int jEnd = -1;
            for (int j = pi + 1; j < F; ++j)
                if (rms[(size_t) j] < 0.1f * peak) { jEnd = j; break; }
            out.decay = (float) ((jEnd >= 0 ? jEnd - pi : F - pi) * (double) fr / sr);
            out.strength = peak;

            static const int envIdx[4] = { 2, 6, 12, 24 };   // 10, 30, 60 y 120 ms
            for (int e : envIdx)
            {
                const float v = e < F ? juce::jlimit (-60.0f, 0.0f, 20.0f * std::log10 (rms[(size_t) e] / peak + 1.0e-6f))
                                      : -60.0f;
                out.vec.push_back (v);
            }
            return true;
        }

        // Reglas de la primera version (mas adelante se puede cambiar por un clasificador entrenado)
        inline int classify (const Features& f)
        {
            if (f.fHi >= 0.55f)
            {
                if (f.decay > 0.9f) return Cymbal;
                if (f.decay > 0.2f) return OpenHat;
                return Hat;
            }
            if (f.fLow >= 0.45f)
            {
                // Donde esta el grave: kick < 150 Hz, tom por encima (aunque haya un hat encima)
                return f.lowCentroid < 150.0f ? Kick : Tom;
            }
            if (f.fLow < 0.18f && f.fMid >= 0.45f && f.flatness >= 0.10f) return Clap;
            if (f.fMid + f.fHi >= 0.5f && f.flatness >= 0.08f) return Snare;
            if (f.fLow >= 0.30f) return Tom;
            return Perc;
        }

        inline float featDist (const std::vector<float>& a, const std::vector<float>& b)
        {
            if (a.size() != b.size() || a.empty()) return 1.0e9f;
            double s = 0.0;
            for (size_t i = 0; i < a.size(); ++i)
            {
                const double d = (double) a[i] - (double) b[i];
                s += d * d;
            }
            return (float) std::sqrt (s / (double) a.size());
        }

        inline float medianOf (std::vector<float>& v)
        {
            const size_t k = v.size();
            if (k == 1) return v[0];
            std::sort (v.begin(), v.end());
            return (k % 2) ? v[k / 2] : 0.5f * (v[k / 2 - 1] + v[k / 2]);
        }

        //======================================================================
        // Paso 1: encontrar los golpes (inicio exacto en muestras)
        inline std::vector<int> detectOnsets (const std::vector<float>& mono, double sr, float sens,
                                              std::atomic<float>* progress, const std::function<bool()>& shouldExit)
        {
            std::vector<int> onsets;
            const int n = (int) mono.size();
            if (n < fftSize * 2) return onsets;

            const int numFrames = (n - fftSize) / hop + 1;
            const int bins = fftSize / 2 + 1;
            const int kLo = 2;
            const int kHi = std::min (bins - 2, (int) (16000.0 * fftSize / sr));

            juce::dsp::FFT fft (fftOrder);
            const auto win = makeHann();
            std::vector<float> buf ((size_t) (2 * fftSize));
            std::vector<std::vector<float>> hist (3, std::vector<float> ((size_t) bins, 0.0f));
            std::vector<float> env ((size_t) numFrames, 0.0f);

            // Flujo espectral tipo "SuperFlux": ignora notas sostenidas y vibratos
            for (int f = 0; f < numFrames; ++f)
            {
                if ((f & 255) == 0)
                {
                    if (shouldExit && shouldExit()) return {};
                    if (progress) progress->store (0.40f * (float) f / (float) numFrames);
                }

                for (int i = 0; i < fftSize; ++i)
                    buf[(size_t) i] = mono[(size_t) (f * hop + i)] * win[(size_t) i];
                std::fill (buf.begin() + fftSize, buf.end(), 0.0f);
                fft.performRealOnlyForwardTransform (buf.data(), true);

                auto& cur = hist[(size_t) (f % 3)];
                for (int k = 0; k < bins; ++k)
                {
                    const float re = buf[(size_t) (2 * k)], im = buf[(size_t) (2 * k + 1)];
                    cur[(size_t) k] = std::log1p (std::sqrt (re * re + im * im) * 10.0f);
                }

                if (f >= 2)
                {
                    const auto& ref = hist[(size_t) ((f + 1) % 3)];   // frame f-2
                    float flux = 0.0f;
                    for (int k = kLo; k <= kHi; ++k)
                    {
                        const float rm = std::max ({ ref[(size_t) k - 1], ref[(size_t) k], ref[(size_t) k + 1] });
                        const float d = cur[(size_t) k] - rm;
                        if (d > 0.0f) flux += d;
                    }
                    env[(size_t) f] = flux / (float) (kHi - kLo + 1);
                }
            }

            std::vector<float> sorted (env);
            const size_t i95 = (size_t) (0.95 * (double) (numFrames - 1));
            std::nth_element (sorted.begin(), sorted.begin() + (long) i95, sorted.end());
            const float ref95 = sorted[i95];
            if (ref95 <= 1.0e-6f) return onsets;

            std::vector<double> pre ((size_t) numFrames + 1, 0.0);
            for (int i = 0; i < numFrames; ++i)
                pre[(size_t) i + 1] = pre[(size_t) i] + env[(size_t) i];

            const float s = juce::jlimit (0.0f, 1.0f, sens);
            const float delta = ref95 * (0.06f + 0.5f * (1.0f - s) * (1.0f - s));
            const int W = 20, sep = 3;

            std::vector<int> peakFrames;
            for (int f = 2; f < numFrames - 2; ++f)
            {
                const int lo = std::max (0, f - W), hi = std::min (numFrames, f + W + 1);
                const float mean = (float) ((pre[(size_t) hi] - pre[(size_t) lo]) / (double) (hi - lo));
                if (env[(size_t) f] < mean + delta) continue;

                bool isMax = true;
                for (int j = f - sep; j <= f + sep && isMax; ++j)
                {
                    if (j < 0 || j >= numFrames || j == f) continue;
                    if (j < f ? env[(size_t) j] >= env[(size_t) f] : env[(size_t) j] > env[(size_t) f])
                        isMax = false;
                }
                if (isMax) peakFrames.push_back (f);
            }

            // Afinar cada golpe al muestreo en el dominio del tiempo
            const int Wt = std::max (16, (int) (0.012 * sr));
            const float gain = 1.3f + 1.5f * (1.0f - s);
            const int minGap = (int) (0.02 * sr);

            for (int pf : peakFrames)
            {
                const int centre = pf * hop + fftSize / 2;
                const int r0 = std::max (0, centre - 1536);
                const int r1 = std::min (n - 1, centre + 768);
                if (r1 - r0 < 64) continue;

                const int a0 = std::max (0, r0 - 32), a1 = std::min (n, r1 + 32);
                std::vector<double> P ((size_t) (a1 - a0) + 1, 0.0);
                for (int i = 0; i < a1 - a0; ++i)
                    P[(size_t) i + 1] = P[(size_t) i] + std::abs ((double) mono[(size_t) (a0 + i)]);

                auto ea = [&] (int i)
                {
                    const int lo = std::max (a0, i - 32), hi = std::min (a1, i + 32);
                    return (P[(size_t) (hi - a0)] - P[(size_t) (lo - a0)]) / (double) (hi - lo);
                };

                int imax = r0;
                double emax = 0.0;
                for (int i = r0; i < r1; ++i)
                {
                    const double e = ea (i);
                    if (e > emax) { emax = e; imax = i; }
                }
                if (emax < 1.0e-5) continue;

                int on = imax;
                while (on > r0 && ea (on - 1) > 0.15 * emax) --on;
                on = std::max (1, on - 16);
                for (int i = on; i > on - 200 && i > 0; --i)       // ajustar al cruce por cero
                    if ((mono[(size_t) (i - 1)] <= 0.0f) != (mono[(size_t) i] <= 0.0f)) { on = i; break; }

                if (! onsets.empty() && on - onsets.back() < minGap) continue;

                // Debe ser un golpe de verdad: sube la energia de golpe
                double sp = 0.0, sq = 0.0;
                const int p0 = std::max (0, on - Wt), p1 = std::min (n, on + Wt);
                for (int i = p0; i < on; ++i) sp += (double) mono[(size_t) i] * mono[(size_t) i];
                for (int i = on; i < p1; ++i) sq += (double) mono[(size_t) i] * mono[(size_t) i];
                const float preR = (float) std::sqrt (sp / std::max (1, on - p0));
                const float postR = (float) std::sqrt (sq / std::max (1, p1 - on));
                if (postR < 2.0e-3f || postR < preR * gain) continue;

                onsets.push_back (on);
            }
            return onsets;
        }

        //======================================================================
        struct Cluster
        {
            std::vector<int> members;
            std::vector<float> centroid;
        };

        // Paso 4: apila los golpes del grupo (alineados) y se queda con lo que se repite
        inline PadSound buildSound (const juce::AudioBuffer<float>& src, const std::vector<float>& mono, double sr,
                                    const std::vector<Hit>& hits, const Cluster& cl, int family)
        {
            PadSound snd;
            snd.family = family;
            snd.hitCount = (int) cl.members.size();

            const int n = src.getNumSamples();
            const int nch = std::min (2, src.getNumChannels());
            static const double lenSec[numFamilies] = { 0.45, 0.35, 0.30, 0.15, 0.50, 0.45, 0.30, 1.20 };
            const int L = std::max (256, std::min ((int) (lenSec[family] * sr), n));

            std::vector<std::pair<float, int>> order;
            for (int m : cl.members)
                order.push_back ({ featDist (hits[(size_t) m].feat, cl.centroid), m });
            std::sort (order.begin(), order.end());

            const int K = std::min ((int) order.size(), 24);
            const int ref = order[0].second;
            const int refStart = hits[(size_t) ref].start;
            const int corrLen = std::max (32, (int) (0.012 * sr));
            const int maxLag = std::max (4, (int) (0.002 * sr));

            auto corrAt = [&] (int s, int lag)
            {
                double num = 0, ea = 0, eb = 0;
                for (int i = 0; i < corrLen; ++i)
                {
                    const int ia = refStart + i, ib = s + lag + i;
                    if (ia >= n || ib < 0 || ib >= n) break;
                    const double a = mono[(size_t) ia], b = mono[(size_t) ib];
                    num += a * b; ea += a * a; eb += b * b;
                }
                return num / std::sqrt (ea * eb + 1.0e-12);
            };

            std::vector<int> pos ((size_t) K);
            for (int j = 0; j < K; ++j)
            {
                const int s = hits[(size_t) order[(size_t) j].second].start;
                int bestLag = 0;
                if (j > 0)
                {
                    double bestC = -2.0;
                    for (int lag = -maxLag; lag <= maxLag; ++lag)
                    {
                        if (s + lag < 0) continue;
                        const double c = corrAt (s, lag);
                        if (c > bestC) { bestC = c; bestLag = lag; }
                    }
                }
                pos[(size_t) j] = std::max (0, s + bestLag);
            }

            snd.clean.setSize (nch, L);
            snd.raw.setSize (nch, L);
            snd.raw.clear();

            std::vector<float> col ((size_t) K);
            for (int ch = 0; ch < nch; ++ch)
            {
                const float* d = src.getReadPointer (ch);
                float* out = snd.clean.getWritePointer (ch);
                for (int i = 0; i < L; ++i)
                {
                    for (int j = 0; j < K; ++j)
                    {
                        const int idx = pos[(size_t) j] + i;
                        col[(size_t) j] = idx < n ? d[idx] : 0.0f;
                    }
                    out[i] = medianOf (col);
                }
            }

            // Golpe real (recortado antes del siguiente golpe)
            const int rawLen = std::max ((int) (0.05 * sr),
                                         std::min ({ L, hits[(size_t) ref].nextStart - refStart, n - refStart }));
            for (int ch = 0; ch < nch; ++ch)
            {
                const float* d = src.getReadPointer (ch);
                float* out = snd.raw.getWritePointer (ch);
                for (int i = 0; i < rawLen && refStart + i < n; ++i)
                    out[i] = d[refStart + i];
            }

            auto fadeAndNormalise = [&] (juce::AudioBuffer<float>& b, int usedLen, double fadeSec)
            {
                const int fade = std::max (1, std::min (usedLen, (int) (fadeSec * sr)));
                float peak = 0.0f;
                for (int ch = 0; ch < b.getNumChannels(); ++ch)
                {
                    float* d = b.getWritePointer (ch);
                    for (int i = 0; i < fade; ++i)
                        d[usedLen - 1 - i] *= (float) i / (float) fade;
                    for (int i = 0; i < b.getNumSamples(); ++i)
                        peak = std::max (peak, std::abs (d[i]));
                }
                if (peak > 1.0e-6f)
                    b.applyGain (0.9f / peak);
            };

            fadeAndNormalise (snd.clean, L, 0.015);
            fadeAndNormalise (snd.raw, std::min (rawLen, n - refStart), 0.010);
            return snd;
        }
    }   // namespace detail

    //==========================================================================
    inline AnalysisResult analyse (const juce::AudioBuffer<float>& src, double sr, float sensitivity,
                                   std::atomic<float>* progress, const std::function<bool()>& shouldExit)
    {
        using namespace detail;

        AnalysisResult res;
        res.sampleRate = sr;
        const int n = src.getNumSamples();
        const int nch = src.getNumChannels();
        res.numSamples = n;
        if (n < fftSize * 2 || nch < 1 || sr <= 0.0) return res;

        auto exiting = [&] { return shouldExit && shouldExit(); };
        auto report = [&] (float v) { if (progress) progress->store (v); };

        std::vector<float> mono ((size_t) n, 0.0f);
        for (int ch = 0; ch < nch; ++ch)
        {
            const float* d = src.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                mono[(size_t) i] += d[i] / (float) nch;
        }

        // Onda para dibujar
        const int pw = 1600;
        res.peaks.assign ((size_t) pw, 0.0f);
        for (int x = 0; x < pw; ++x)
        {
            const int s0 = (int) ((juce::int64) x * n / pw);
            const int s1 = std::max (s0 + 1, (int) ((juce::int64) (x + 1) * n / pw));
            const int stride = std::max (1, (s1 - s0) / 64);
            float pk = 0.0f;
            for (int i = s0; i < s1 && i < n; i += stride)
                pk = std::max (pk, std::abs (mono[(size_t) i]));
            res.peaks[(size_t) x] = std::min (1.0f, pk);
        }

        // 1) Detectar
        const auto onsets = detectOnsets (mono, sr, sensitivity, progress, shouldExit);
        if (exiting()) return res;

        // 2) Clasificar
        juce::dsp::FFT fft (fftOrder);
        const auto hitWin = makeHitWindow();
        std::vector<float> buf ((size_t) (2 * fftSize));

        for (size_t i = 0; i < onsets.size(); ++i)
        {
            if ((i & 31) == 0)
            {
                if (exiting()) return res;
                report (0.40f + 0.25f * (float) i / (float) onsets.size());
            }

            const int start = onsets[i];
            const int next = i + 1 < onsets.size() ? onsets[i + 1] : n;
            const int sliceLen = std::min ({ next - start, (int) (1.5 * sr), n - start });

            Features f;
            if (! extractFeatures (fft, hitWin, buf, mono, start, sliceLen, sr, f))
                continue;

            Hit h;
            h.start = start;
            h.nextStart = next;
            h.family = classify (f);
            h.strength = f.strength;
            h.feat = f.vec;
            res.hits.push_back (std::move (h));
        }

        for (const auto& h : res.hits)
            ++res.familyCount[h.family];

        // 3) Agrupar los que suenan igual (dentro de cada familia)
        const float tau = 6.0f;
        std::vector<std::pair<int, std::vector<Cluster>>> kept;

        for (int fam = 0; fam < numFamilies; ++fam)
        {
            std::vector<int> idx;
            for (size_t i = 0; i < res.hits.size(); ++i)
                if (res.hits[i].family == fam) idx.push_back ((int) i);
            if (idx.empty()) continue;

            std::sort (idx.begin(), idx.end(), [&] (int a, int b) { return res.hits[(size_t) a].strength > res.hits[(size_t) b].strength; });

            std::vector<Cluster> cl;
            for (int h : idx)
            {
                const auto& ft = res.hits[(size_t) h].feat;
                int best = -1;
                float bd = 1.0e9f;
                for (size_t c = 0; c < cl.size(); ++c)
                {
                    const float d = featDist (ft, cl[c].centroid);
                    if (d < bd) { bd = d; best = (int) c; }
                }

                if (best >= 0 && bd < tau)
                {
                    auto& c = cl[(size_t) best];
                    c.members.push_back (h);
                    const float w = 1.0f / (float) c.members.size();
                    for (size_t k = 0; k < ft.size(); ++k)
                        c.centroid[k] += (ft[k] - c.centroid[k]) * w;
                }
                else
                {
                    Cluster c;
                    c.members.push_back (h);
                    c.centroid = ft;
                    cl.push_back (std::move (c));
                }
            }

            // Una pasada de reasignacion con los centroides ya estables
            for (auto& c : cl) c.members.clear();
            for (int h : idx)
            {
                const auto& ft = res.hits[(size_t) h].feat;
                size_t best = 0;
                float bd = 1.0e9f;
                for (size_t c = 0; c < cl.size(); ++c)
                {
                    const float d = featDist (ft, cl[c].centroid);
                    if (d < bd) { bd = d; best = c; }
                }
                cl[best].members.push_back (h);
            }
            cl.erase (std::remove_if (cl.begin(), cl.end(), [] (const Cluster& c) { return c.members.empty(); }), cl.end());

            for (auto& c : cl)
            {
                std::fill (c.centroid.begin(), c.centroid.end(), 0.0f);
                for (int h : c.members)
                    for (size_t k = 0; k < c.centroid.size(); ++k)
                        c.centroid[k] += res.hits[(size_t) h].feat[k] / (float) c.members.size();
            }

            std::sort (cl.begin(), cl.end(), [] (const Cluster& a, const Cluster& b) { return a.members.size() > b.members.size(); });

            const int minCount = std::max (2, (int) (0.04 * (double) idx.size()));
            std::vector<Cluster> keep;
            for (size_t c = 0; c < cl.size() && keep.size() < 8; ++c)
                if (c == 0 || (int) cl[c].members.size() >= minCount)
                    keep.push_back (cl[c]);

            for (size_t c = 0; c < keep.size(); ++c)
                for (int h : keep[c].members)
                    res.hits[(size_t) h].cluster = (int) c;

            kept.push_back ({ fam, std::move (keep) });
        }

        // 4) Aislar y colocar en los pads (kick en C1, snare en D1, hat en F#1, open hat en A#1...)
        static const int homes[numFamilies] = { 0, 2, 3, 6, 10, 5, 9, 13 };
        bool used[64] = {};
        int total = 0;
        for (auto& k : kept) total += (int) k.second.size();
        int done = 0;

        for (auto& k : kept)
        {
            for (size_t c = 0; c < k.second.size(); ++c)
            {
                if (exiting()) return res;
                report (0.65f + 0.35f * (float) done / (float) std::max (1, total));
                ++done;

                int pad = -1;
                if (c == 0 && ! used[homes[k.first]]) pad = homes[k.first];
                for (int p = 0; pad < 0 && p < 64; ++p)
                    if (! used[p]) pad = p;
                if (pad < 0) continue;
                used[pad] = true;

                auto snd = buildSound (src, mono, sr, res.hits, k.second[c], k.first);
                snd.pad = pad;
                snd.name = juce::String (familyLabel (k.first)) + (c > 0 ? " " + juce::String ((int) c + 1) : juce::String());
                res.sounds.push_back (std::move (snd));
            }
        }

        report (1.0f);
        return res;
    }
}
