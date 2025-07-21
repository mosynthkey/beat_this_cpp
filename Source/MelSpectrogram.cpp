#include "MelSpectrogram.h"
#include <vector>
#include <complex>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <numbers>
#include "pocketfft_hdronly.h"

// Constructor
MelSpectrogram::MelSpectrogram() {
    create_mel_filterbank();
}

// Destructor
MelSpectrogram::~MelSpectrogram() {
    // Clean up FFTW plans if any were created dynamically
}

// Helper function: Convert Hz to Mel scale (Slaney)
float MelSpectrogram::hz_to_mel(float hz) {
    // Slaney scale
    double f_min = 0.0;
    double f_sp = 200.0 / 3.0;

    double mels = (hz - f_min) / f_sp;

    double min_log_hz = 1000.0;
    double min_log_mel = (min_log_hz - f_min) / f_sp;
    double logstep = std::log(6.4) / 27.0;

    if (hz >= min_log_hz) {
        mels = min_log_mel + std::log(hz / min_log_hz) / logstep;
    }
    return static_cast<float>(mels);
}

// Helper function: Convert Mel to Hz scale (Slaney)
float MelSpectrogram::mel_to_hz(float mel) {
    // Slaney scale
    double f_min = 0.0;
    double f_sp = 200.0 / 3.0;
    double freqs = f_min + f_sp * mel;

    double min_log_hz = 1000.0;
    double min_log_mel = (min_log_hz - f_min) / f_sp;
    double logstep = std::log(6.4) / 27.0;

    if (mel >= min_log_mel) {
        freqs = min_log_hz * std::exp(logstep * (mel - min_log_mel));
    }
    return static_cast<float>(freqs);
}

// Helper function: Create a Hann window
std::vector<float> MelSpectrogram::create_hann_window(int size) {
    std::vector<float> window(size);
    for (int i = 0; i < size; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi * i / size));
    }
    return window;
}

// Helper function: Create Mel filterbank
void MelSpectrogram::create_mel_filterbank() {
    // Calculate Mel frequencies for f_min, f_max
    double mel_min = hz_to_mel(f_min);
    double mel_max = hz_to_mel(f_max);

    // Create n_mels + 2 evenly spaced Mel points
    std::vector<double> mel_points(n_mels + 2);
    for (int i = 0; i < n_mels + 2; ++i) {
        mel_points[i] = mel_min + (mel_max - mel_min) * i / (n_mels + 1);
    }

    // Convert Mel points back to Hz
    std::vector<double> hz_points(n_mels + 2);
    for (int i = 0; i < n_mels + 2; ++i) {
        hz_points[i] = mel_to_hz(mel_points[i]);
    }

    // Convert Hz points to FFT bin numbers
    std::vector<double> freqs(n_fft / 2 + 1);
    for (int i = 0; i < n_fft / 2 + 1; ++i) {
        freqs[i] = static_cast<double>(i) * sample_rate / n_fft;
    }

    std::vector<int> bin_points(n_mels + 2);
    for (int i = 0; i < n_mels + 2; ++i) {
        auto it = std::lower_bound(freqs.begin(), freqs.end(), hz_points[i]);
        bin_points[i] = std::distance(freqs.begin(), it);
    }

    // Resize mel_filterbank to (n_fft / 2 + 1, n_mels) to match Python's (freq_bins, mel_bins)
    mel_filterbank.resize(n_fft / 2 + 1, std::vector<double>(n_mels, 0.0));

    // Outer loop for frequency bins (rows)
    for (int i = 0; i < n_fft / 2 + 1; ++i) {
        double hz_i = freqs[i];
        // Inner loop for mel filters (columns)
        for (int j = 0; j < n_mels; ++j) {
            double hz_left = hz_points[j];
            double hz_center = hz_points[j + 1];
            double hz_right = hz_points[j + 2];

            double left_slope_val = 0.0;
            if (hz_center - hz_left != 0) {
                left_slope_val = (hz_i - hz_left) / (hz_center - hz_left);
            }

            double right_slope_val = 0.0;
            if (hz_right - hz_center != 0) {
                right_slope_val = (hz_right - hz_i) / (hz_right - hz_center);
            }
            
            mel_filterbank[i][j] = std::max(0.0, std::min(left_slope_val, right_slope_val));
        }
    }
}

std::vector<std::vector<float>> MelSpectrogram::compute(const std::vector<float>& audio) {
    // Apply padding similar to torchaudio.stft(center=True, pad_mode="reflect")
    int pad_size = n_fft / 2;
    std::vector<float> padded_audio;
    padded_audio.reserve(audio.size() + 2 * pad_size);

    // Pre-padding (reflection)
    for (int i = pad_size; i >= 1; --i) {
        padded_audio.push_back(audio[i]);
    }

    // Original audio
    padded_audio.insert(padded_audio.end(), audio.begin(), audio.end());

    // Post-padding (reflection)
    for (int i = 1; i <= pad_size; ++i) {
        padded_audio.push_back(audio[audio.size() - 1 - i]);
    }

    std::vector<float> window = create_hann_window(win_length);

    // Calculate number of frames based on padded audio
    int num_frames = (padded_audio.size() - n_fft) / hop_length + 1;
    if (num_frames <= 0) {
        return std::vector<std::vector<float>>();
    }

    std::vector<std::vector<float>> mel_spectrogram_output(num_frames, std::vector<float>(n_mels));
    log1p_input_cpp.resize(num_frames, std::vector<double>(n_mels));
    last_power_spectrum.resize(num_frames, std::vector<double>(n_fft / 2 + 1));

    // PocketFFT setup
    std::vector<std::complex<float>> out_pocket(n_fft / 2 + 1);
    std::vector<float> in_pocket(n_fft);
    pocketfft::shape_t shape = { (size_t)n_fft };
    pocketfft::stride_t stride_in = { sizeof(float) };
    pocketfft::stride_t stride_out = { sizeof(std::complex<float>) };

    for (int i = 0; i < num_frames; ++i) {
        // Extract frame and apply window
        int start_idx = i * hop_length;
        for (int j = 0; j < n_fft; ++j) {
            in_pocket[j] = padded_audio[start_idx + j] * window[j];
        }

        // Execute FFT
        pocketfft::r2c(shape, stride_in, stride_out, 0, true, in_pocket.data(), out_pocket.data(), 1.0f);


        // Compute amplitude spectrum and apply normalization
        std::vector<double> amplitude_spectrum(n_fft / 2 + 1);
        double normalization_factor = std::sqrt(static_cast<double>(win_length));
        for (int j = 0; j < n_fft / 2 + 1; ++j) {
            double real = out_pocket[j].real();
            double imag = out_pocket[j].imag();
            amplitude_spectrum[j] = std::sqrt(real * real + imag * imag) / normalization_factor;
        }
        
        // Store the computed spectrum for this frame
        last_power_spectrum[i] = amplitude_spectrum;

        // Apply Mel filterbank and log scaling
        for (int m = 0; m < n_mels; ++m) {
            double mel_energy = 0.0;
            for (int k = 0; k < n_fft / 2 + 1; ++k) {
                mel_energy += amplitude_spectrum[k] * mel_filterbank[k][m];
            }
            log1p_input_cpp[i][m] = log_multiplier * mel_energy;
            mel_spectrogram_output[i][m] = std::log1p(log_multiplier * std::max(mel_energy, amin));
        }
    }

    // No explicit cleanup needed for PocketFFT with std::vector

    return mel_spectrogram_output;
}


