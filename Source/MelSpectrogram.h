#ifndef MELSPECTROGRAM_H
#define MELSPECTROGRAM_H

#include <vector>
#include <complex>
#include <cmath>
#include <numeric>
#include <algorithm>
#include "pocketfft_hdronly.h"

/**
 * @class MelSpectrogram
 * @brief Converts audio signals to Mel-scale spectrograms
 * 
 * This class implements Mel-scale spectrogram computation compatible with
 * Python's torchaudio library. It performs STFT (Short-Time Fourier Transform)
 * and applies Mel filterbank to convert frequency domain to perceptual scale.
 */
class MelSpectrogram {
public:
    MelSpectrogram();
    ~MelSpectrogram();
    
    /**
     * @brief Computes Mel-scale spectrogram from audio signal
     * @param audio Input audio signal (mono, 22050 Hz)
     * @return 2D vector representing Mel spectrogram [frames][mel_bins]
     */
    std::vector<std::vector<float>> compute(const std::vector<float>& audio);

    // Debugging accessors - remove in production
    const std::vector<std::vector<double>>& get_mel_filterbank() const { return mel_filterbank; }
    const std::vector<std::vector<double>>& get_last_power_spectrum() const { return last_power_spectrum; }
    const std::vector<std::vector<double>>& get_log1p_input() const { return log1p_input_cpp; }

private:
    // Audio processing parameters (must match Python implementation)
    const int sample_rate = 22050;     // Target sample rate
    const int n_fft = 1024;            // FFT size
    const int win_length = 1024;       // Window length
    const int hop_length = 441;        // Hop length between frames
    const int f_min = 30;              // Minimum frequency
    const int f_max = 11000;           // Maximum frequency
    const int n_mels = 128;            // Number of Mel bands
    const float power = 1.0f;          // Power spectrum exponent
    const float log_multiplier = 1000.0f; // Log scaling factor
    const double amin = 1e-10;         // Minimum amplitude clipping

    std::vector<std::vector<double>> mel_filterbank; // (n_freqs, n_mels)
    std::vector<std::vector<double>> last_power_spectrum;
    std::vector<std::vector<double>> log1p_input_cpp;

    // Helper functions
    void create_mel_filterbank();
    float hz_to_mel(float hz);
    float mel_to_hz(float mel);
    std::vector<float> create_hann_window(int size);

};

#endif // MELSPECTROGRAM_H