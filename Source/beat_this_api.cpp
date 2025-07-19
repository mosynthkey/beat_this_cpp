#include "beat_this_api.h"
#include "MelSpectrogram.h"
#include "InferenceProcessor.h"
#include "Postprocessor.h"
#include "sndfile.h"
#include "soxr.h"

#include <iostream>
#include <memory>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <set>
#include <stdexcept>

namespace BeatThis {

// Pimpl implementation
class BeatThis::Impl {
public:
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;

    explicit Impl(const std::string& onnx_model_path) 
        : env(ORT_LOGGING_LEVEL_WARNING, "beat_this_cpp_api") {
        Ort::SessionOptions session_options;
        session = std::make_unique<Ort::Session>(env, onnx_model_path.c_str(), session_options);
    }
};

BeatThis::BeatThis(const std::string& onnx_model_path) 
    : pImpl(std::make_unique<Impl>(onnx_model_path)) {
}

BeatThis::~BeatThis() = default;

namespace {
    // Helper function to calculate beat counts
    std::vector<int> calculate_beat_counts(const std::vector<float>& beats, 
                                          const std::vector<float>& downbeats) {
        std::vector<int> beat_counts(beats.size());
        
        if (beats.empty() || downbeats.empty()) {
            return beat_counts;
        }

        // Check if all downbeats are beats
        std::set<float> beats_set(beats.begin(), beats.end());
        for (float db_time : downbeats) {
            if (beats_set.find(db_time) == beats_set.end()) {
                std::cerr << "Error: Not all downbeats are beats. Cannot calculate beat counts." << std::endl;
                return beat_counts;
            }
        }

        // Handle pickup measure and beat counting
        int start_counter = 1;
        if (downbeats.size() >= 2) {
            auto it_first_downbeat = std::lower_bound(beats.begin(), beats.end(), downbeats[0]);
            auto it_second_downbeat = std::lower_bound(beats.begin(), beats.end(), downbeats[1]);

            int first_downbeat_idx = std::distance(beats.begin(), it_first_downbeat);
            int second_downbeat_idx = std::distance(beats.begin(), it_second_downbeat);

            int beats_in_first_measure = second_downbeat_idx - first_downbeat_idx;
            int pickup_beats = first_downbeat_idx;

            if (pickup_beats < beats_in_first_measure) {
                start_counter = beats_in_first_measure - pickup_beats;
            } else {
                std::cerr << "WARNING: There are more beats in the pickup measure than in the first measure. "
                         << "The beat count will start from 2 without trying to estimate the length of the pickup measure." << std::endl;
                start_counter = 1;
            }
        } else {
            std::cerr << "WARNING: There are less than two downbeats in the predictions. Something may be wrong. "
                     << "The beat count will start from 2 without trying to estimate the length of the pickup measure." << std::endl;
            start_counter = 1;
        }

        // Calculate beat counts
        int counter = start_counter;
        size_t downbeat_idx = 0;
        float next_downbeat = (downbeat_idx < downbeats.size()) ? downbeats[downbeat_idx] : -1.0f;

        for (size_t i = 0; i < beats.size(); ++i) {
            float beat_time = beats[i];
            
            if (downbeat_idx < downbeats.size() && std::abs(beat_time - next_downbeat) < 1e-6f) {
                counter = 1;
                downbeat_idx++;
                next_downbeat = (downbeat_idx < downbeats.size()) ? downbeats[downbeat_idx] : -1.0f;
            } else {
                counter++;
            }
            beat_counts[i] = counter;
        }

        return beat_counts;
    }

    // Helper function to resample audio
    bool resample_audio(const std::vector<float>& in_buffer, int in_rate, 
                       std::vector<float>& out_buffer, int out_rate) {
        size_t in_len = in_buffer.size();
        size_t out_len = static_cast<size_t>(in_len * static_cast<double>(out_rate) / in_rate);
        out_buffer.resize(out_len);

        soxr_error_t error;
        soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);
        soxr_t soxr = soxr_create(in_rate, out_rate, 1, &error, nullptr, &q_spec, nullptr);
        if (error) {
            std::cerr << "Error: soxr_create failed: " << soxr_strerror(error) << std::endl;
            return false;
        }

        size_t odone;
        error = soxr_process(soxr, in_buffer.data(), in_len, nullptr, 
                           out_buffer.data(), out_len, &odone);
        if (error) {
            std::cerr << "Error: soxr_process failed: " << soxr_strerror(error) << std::endl;
            soxr_delete(soxr);
            return false;
        }

        out_buffer.resize(odone);
        soxr_delete(soxr);
        return true;
    }

    // Helper function to convert to mono
    std::vector<float> convert_to_mono(const std::vector<float>& audio_data, int channels) {
        if (channels == 1) {
            return audio_data;
        }

        size_t num_frames = audio_data.size() / channels;
        std::vector<float> mono_buffer(num_frames);
        
        for (size_t i = 0; i < num_frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                sum += audio_data[i * channels + ch];
            }
            mono_buffer[i] = sum / channels;
        }
        
        return mono_buffer;
    }
}

BeatResult BeatThis::process_audio(const std::vector<float>& audio_data, 
                                  int samplerate, int channels) {
    try {
        // Convert to mono
        auto mono_audio = convert_to_mono(audio_data, channels);
        
        // Resample if necessary
        std::vector<float> resampled_buffer;
        constexpr int target_samplerate = 22050;
        
        if (samplerate != target_samplerate) {
            if (!resample_audio(mono_audio, samplerate, resampled_buffer, target_samplerate)) {
                throw std::runtime_error("Failed to resample audio");
            }
        } else {
            resampled_buffer = std::move(mono_audio);
        }

        // Compute Mel Spectrogram
        MelSpectrogram spect_computer;
        auto spectrogram = spect_computer.compute(resampled_buffer);

        // Run Inference
        InferenceProcessor processor(*(pImpl->session), pImpl->env);
        auto beat_downbeat_logits = processor.process_spectrogram(spectrogram);

        // Post-process to get beat and downbeat times
        Postprocessor postprocessor;
        auto beat_downbeat_times = postprocessor.process(
            beat_downbeat_logits.first, beat_downbeat_logits.second);

        // Calculate beat counts
        std::vector<int> beat_counts = calculate_beat_counts(
            beat_downbeat_times.first, beat_downbeat_times.second);

        return BeatResult{
            std::move(beat_downbeat_times.first),
            std::move(beat_downbeat_times.second),
            std::move(beat_counts)
        };

    } catch (const Ort::Exception& e) {
        throw std::runtime_error("ONNX Runtime error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("Processing error: " + std::string(e.what()));
    }
}

BeatResult BeatThis::process_audio(const float* audio_data, size_t num_samples, 
                                  int samplerate, int channels) {
    std::vector<float> audio_vector(audio_data, audio_data + num_samples);
    return process_audio(audio_vector, samplerate, channels);
}


} // namespace BeatThis