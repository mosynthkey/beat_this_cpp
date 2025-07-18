#include "beat_this_api.h"
#include "MelSpectrogram.h"
#include "InferenceProcessor.h"
#include "Postprocessor.h"
#include "sndfile.h"
#include "soxr.h"

#include <iostream>
#include <vector>
#include <memory>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <set>

// Internal context structure
struct BeatThisContext {
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;

    BeatThisContext(const std::string& onnx_model_path) 
        : env(ORT_LOGGING_LEVEL_WARNING, "beat_this_cpp_api") {
        Ort::SessionOptions session_options;
        session = std::make_unique<Ort::Session>(env, onnx_model_path.c_str(), session_options);

    }
};

// Function to load audio using libsndfile (re-used from main.cpp)
bool load_audio_internal(const std::string& path, std::vector<float>& audio_buffer, int& samplerate) {
    SF_INFO sfinfo;
    SNDFILE* infile = sf_open(path.c_str(), SFM_READ, &sfinfo);
    if (!infile) {
        std::cerr << "Error: could not open audio file: " << path << std::endl;
        return false;
    }

    samplerate = sfinfo.samplerate;
    audio_buffer.resize(sfinfo.frames * sfinfo.channels);
    sf_read_float(infile, audio_buffer.data(), audio_buffer.size());

    // Convert to mono if necessary
    if (sfinfo.channels > 1) {
        std::vector<float> mono_buffer(sfinfo.frames);
        for (size_t i = 0; i < sfinfo.frames; ++i) {
            float sum = 0.0f;
            for (size_t j = 0; j < sfinfo.channels; ++j) {
                sum += audio_buffer[i * sfinfo.channels + j];
            }
            mono_buffer[i] = sum / sfinfo.channels;
        }
        audio_buffer = mono_buffer;
    }

    sf_close(infile);
    return true;
}

// Function to resample audio using libsoxr (re-used from main.cpp)
bool resample_audio_internal(const std::vector<float>& in_buffer, int in_rate, std::vector<float>& out_buffer, int out_rate) {
    size_t in_len = in_buffer.size();
    size_t out_len = static_cast<size_t>(in_len * static_cast<double>(out_rate) / in_rate);
    out_buffer.resize(out_len);


    soxr_error_t error;
    soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0); // High Quality - match Python soxr.HQ
    soxr_t soxr = soxr_create(in_rate, out_rate, 1, &error, NULL, &q_spec, NULL);
    if (error) {
        std::cerr << "Error: soxr_create failed: " << soxr_strerror(error) << std::endl;
        return false;
    }

    size_t odone;
    error = soxr_process(soxr, in_buffer.data(), in_len, NULL, out_buffer.data(), out_len, &odone);
    if (error) {
        std::cerr << "Error: soxr_process failed: " << soxr_strerror(error) << std::endl;
        soxr_delete(soxr);
        return false;
    }

    out_buffer.resize(odone);
    soxr_delete(soxr);
    return true;
}


void* BeatThis_init(const char* onnx_model_path) {
    try {
        return new BeatThisContext(onnx_model_path);
    } catch (const Ort::Exception& e) {
        std::cerr << "Error initializing ONNX Runtime session: " << e.what() << std::endl;
        return nullptr;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing BeatThis API: " << e.what() << std::endl;
        return nullptr;
    }
}

BeatThisResult BeatThis_process_audio(
    void* context,
    const float* audio_data,
    int num_samples,
    int samplerate,
    int original_channels
) {
    BeatThisResult result = {nullptr, 0, nullptr, 0};
    if (!context) {
        std::cerr << "Error: BeatThis context is null." << std::endl;
        return result;
    }

    BeatThisContext* ctx = static_cast<BeatThisContext*>(context);

    try {
        // 1. Convert to mono if stereo
        std::vector<float> mono_audio;
        if (original_channels == 2) {
            int mono_samples = num_samples / 2;
            mono_audio.resize(mono_samples);
            for (int i = 0; i < mono_samples; ++i) {
                mono_audio[i] = (audio_data[i * 2] + audio_data[i * 2 + 1]) / 2.0f;
            }
        } else {
            mono_audio = std::vector<float>(audio_data, audio_data + num_samples);
        }
        
        // 2. Resample mono audio
        std::vector<float> resampled_buffer;
        const int target_samplerate = 22050;
        if (samplerate != target_samplerate) {
            if (!resample_audio_internal(mono_audio, samplerate, resampled_buffer, target_samplerate)) {
                return result;
            }
        } else {
            resampled_buffer = mono_audio;
        }


        // 3. Compute Mel Spectrogram
        MelSpectrogram spect_computer;
        std::vector<std::vector<float>> spectrogram = spect_computer.compute(resampled_buffer);

        // 3. Run Inference using InferenceProcessor
        InferenceProcessor processor(*(ctx->session), ctx->env);
        std::pair<std::vector<float>, std::vector<float>> beat_downbeat_logits = processor.process_spectrogram(spectrogram);

        // 4. Post-process to get beat and downbeat times
        Postprocessor postprocessor;
        std::pair<std::vector<float>, std::vector<float>> beat_downbeat_times = 
            postprocessor.process(beat_downbeat_logits.first, beat_downbeat_logits.second);

        // Copy results to C-style arrays
        result.num_beats = beat_downbeat_times.first.size();
        result.beats = new float[result.num_beats];
        std::copy(beat_downbeat_times.first.begin(), beat_downbeat_times.first.end(), result.beats);

        result.num_downbeats = beat_downbeat_times.second.size();
        result.downbeats = new float[result.num_downbeats];
        std::copy(beat_downbeat_times.second.begin(), beat_downbeat_times.second.end(), result.downbeats);

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error during processing: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during processing: " << e.what() << std::endl;
    }

    return result;
}

bool BeatThis_save_beats_to_file(
    void* context, // Context is not strictly needed here, but kept for API consistency
    BeatThisResult result,
    const char* output_filepath
) {
    std::ofstream outfile(output_filepath);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_filepath << std::endl;
        return false;
    }

    // Convert C-style arrays to std::vector for easier manipulation
    std::vector<float> beats_vec(result.beats, result.beats + result.num_beats);
    std::vector<float> downbeats_vec(result.downbeats, result.downbeats + result.num_downbeats);

    // Check if all downbeats are beats (as per Python's save_beat_tsv)
    std::set<float> beats_set(beats_vec.begin(), beats_vec.end());
    for (float db_time : downbeats_vec) {
        if (beats_set.find(db_time) == beats_set.end()) {
            std::cerr << "Error: Not all downbeats are beats. Cannot save to .beats format." << std::endl;
            outfile.close();
            return false;
        }
    }

    // Handle pickup measure and beat counting
    int start_counter = 1;
    if (downbeats_vec.size() >= 2) {
        auto it_first_downbeat = std::lower_bound(beats_vec.begin(), beats_vec.end(), downbeats_vec[0]);
        auto it_second_downbeat = std::lower_bound(beats_vec.begin(), beats_vec.end(), downbeats_vec[1]);

        int first_downbeat_idx = std::distance(beats_vec.begin(), it_first_downbeat);
        int second_downbeat_idx = std::distance(beats_vec.begin(), it_second_downbeat);

        int beats_in_first_measure = second_downbeat_idx - first_downbeat_idx;
        int pickup_beats = first_downbeat_idx;

        if (pickup_beats < beats_in_first_measure) {
            start_counter = beats_in_first_measure - pickup_beats;
        } else {
            std::cerr << "WARNING: There are more beats in the pickup measure than in the first measure. The beat count will start from 2 without trying to estimate the length of the pickup measure." << std::endl;
            start_counter = 1;
        }
    } else {
        std::cerr << "WARNING: There are less than two downbeats in the predictions. Something may be wrong. The beat count will start from 2 without trying to estimate the length of the pickup measure." << std::endl;
        start_counter = 1;
    }

    // Write the beat file
    int counter = start_counter;
    size_t downbeat_idx = 0;
    float next_downbeat = (downbeat_idx < downbeats_vec.size()) ? downbeats_vec[downbeat_idx] : -1.0f; // Use -1.0f as a sentinel

    outfile << std::fixed << std::setprecision(3);

    for (float beat_time : beats_vec) {
        if (downbeat_idx < downbeats_vec.size() && std::abs(beat_time - next_downbeat) < 1e-6) { // Compare with tolerance
            counter = 1;
            downbeat_idx++;
            next_downbeat = (downbeat_idx < downbeats_vec.size()) ? downbeats_vec[downbeat_idx] : -1.0f;
        } else {
            counter++;
        }
        outfile << beat_time << "\t" << counter << "\n";
    }

    outfile.close();
    return true;
}

void BeatThis_free_result(BeatThisResult result) {
    delete[] result.beats;
    delete[] result.downbeats;
}

void BeatThis_cleanup(void* context) {
    if (context) {
        delete static_cast<BeatThisContext*>(context);
    }
}

