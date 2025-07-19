#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <set>
#include <algorithm>
#include <cmath>

#include "sndfile.h" // For load_audio_for_example
#include "beat_this_api.h"

// Function to load audio from file (simplified for example)
// In a real application, this would use platform-specific audio loading.
// For this example, we'll assume a simple WAV file loading.
// This function is for demonstration purposes and not part of the core API.
bool load_audio_for_example(const std::string& path, std::vector<float>& audio_buffer, int& samplerate, int& channels) {
    SF_INFO sfinfo;
    SNDFILE* infile = sf_open(path.c_str(), SFM_READ, &sfinfo);
    if (!infile) {
        std::cerr << "Error: could not open audio file: " << path << std::endl;
        return false;
    }

    samplerate = sfinfo.samplerate;
    channels = sfinfo.channels;
    audio_buffer.resize(sfinfo.frames * sfinfo.channels);
    sf_read_float(infile, audio_buffer.data(), audio_buffer.size());

    sf_close(infile);
    return true;
}

// Function to save beat information to a .beats file
bool save_beats_to_file(const BeatThis::BeatResult& result, const std::string& output_filepath) {
    std::ofstream outfile(output_filepath);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_filepath << std::endl;
        return false;
    }

    outfile << std::fixed << std::setprecision(3);
    for (size_t i = 0; i < result.beats.size(); ++i) {
        outfile << result.beats[i] << "\t" << result.beat_counts[i] << "\n";
    }

    return true;
}

// Audio generation parameters
const int SAMPLE_RATE = 44100; // Hz
const double DURATION_PER_BEAT = 0.1; // seconds
const double ATTACK_TIME = 0.01; // seconds
const double DECAY_TIME = 0.05; // seconds

// Function to generate a sine wave with ADSR-like envelope
std::vector<float> generate_sine_wave(
    double frequency, 
    double duration, 
    double attack_time, 
    double decay_time,
    int sample_rate
) {
    int num_samples = static_cast<int>(duration * sample_rate);
    std::vector<float> waveform(num_samples);

    int attack_samples = static_cast<int>(attack_time * sample_rate);
    int decay_samples = static_cast<int>(decay_time * sample_rate);

    for (int i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        double amplitude = 1.0;

        // Attack phase
        if (i < attack_samples) {
            amplitude = static_cast<double>(i) / attack_samples;
        }
        // Decay phase
        else if (i > num_samples - decay_samples) {
            amplitude = static_cast<double>(num_samples - i) / decay_samples;
        }

        waveform[i] = static_cast<float>(amplitude * std::sin(2.0 * M_PI * frequency * t));
    }
    return waveform;
}

// Function to write WAV file
bool write_wav_file(const std::string& filepath, const std::vector<float>& audio_data, int sample_rate) {
    SF_INFO sfinfo;
    sfinfo.samplerate = sample_rate;
    sfinfo.channels = 1; // Mono
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT; // WAV, float format

    SNDFILE* outfile = sf_open(filepath.c_str(), SFM_WRITE, &sfinfo);
    if (!outfile) {
        std::cerr << "Error: Could not open WAV file for writing: " << filepath << std::endl;
        std::cerr << sf_strerror(NULL) << std::endl; // Print libsndfile error
        return false;
    }

    sf_write_float(outfile, audio_data.data(), audio_data.size());
    sf_close(outfile);
    return true;
}

// Function to generate audio from beats
bool generate_beats_audio(const BeatThis::BeatResult& result, const std::string& output_wav_file) {
    if (result.beats.empty()) {
        std::cerr << "Error: No beats to generate audio from." << std::endl;
        return false;
    }

    // Determine total duration of the audio to be generated
    double total_duration = result.beats.back() + DURATION_PER_BEAT + DECAY_TIME;
    int total_samples = static_cast<int>(total_duration * SAMPLE_RATE);
    std::vector<float> final_audio(total_samples, 0.0f);

    for (size_t i = 0; i < result.beats.size(); ++i) {
        double frequency;
        if (result.beat_counts[i] == 1) { // Downbeat
            frequency = 880.0; // Hz
        } else {
            frequency = 440.0; // Hz
        }

        std::vector<float> beat_waveform = generate_sine_wave(
            frequency, DURATION_PER_BEAT, ATTACK_TIME, DECAY_TIME, SAMPLE_RATE
        );

        // Mix the generated waveform into the final audio buffer
        int start_sample = static_cast<int>(result.beats[i] * SAMPLE_RATE);
        for (size_t j = 0; j < beat_waveform.size(); ++j) {
            if (start_sample + j < total_samples) {
                final_audio[start_sample + j] += beat_waveform[j];
            }
        }
    }

    // Normalize audio to prevent clipping
    float max_val = 0.0f;
    for (float sample : final_audio) {
        max_val = std::max(max_val, std::abs(sample));
    }
    if (max_val > 1.0f) {
        for (float& sample : final_audio) {
            sample /= max_val;
        }
    }

    return write_wav_file(output_wav_file, final_audio, SAMPLE_RATE);
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <onnx_model_path> <audio_file_path> [options]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --output-beats <file>    Save beat information to .beats file" << std::endl;
    std::cerr << "  --output-audio <file>    Generate audio file with beats as click track" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --output-beats output.beats" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --output-audio output.wav" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --output-beats output.beats --output-audio output.wav" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string onnx_model_path = argv[1];
    const std::string audio_file_path = argv[2];
    std::string output_beats_file;
    std::string output_wav_file;

    // Parse command line arguments
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--output-beats" && i + 1 < argc) {
            output_beats_file = argv[++i];
        } else if (arg == "--output-audio" && i + 1 < argc) {
            output_wav_file = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Check if at least one output is specified
    if (output_beats_file.empty() && output_wav_file.empty()) {
        std::cerr << "Error: At least one output option must be specified (--output-beats or --output-audio)" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    try {
        // Initialize the BeatThis API
        BeatThis::BeatThis beat_analyzer(onnx_model_path);

        // Load audio for example
        std::vector<float> audio_buffer;
        int samplerate;
        int channels;
        if (!load_audio_for_example(audio_file_path, audio_buffer, samplerate, channels)) {
            return 1;
        }

        std::cout << "Loaded audio: " << audio_buffer.size() << " samples, " << samplerate << " Hz, " << channels << " channels" << std::endl;

        // Process audio
        auto result = beat_analyzer.process_audio(audio_buffer, samplerate, channels);

        std::cout << "Found " << result.beats.size() << " beats and " << result.downbeats.size() << " downbeats" << std::endl;

        // Save results to .beats file if requested
        if (!output_beats_file.empty()) {
            if (save_beats_to_file(result, output_beats_file)) {
                std::cout << "Beats saved to: " << output_beats_file << std::endl;
            } else {
                std::cerr << "Failed to save beats to file." << std::endl;
                return 1;
            }
        }

        // Generate audio if requested
        if (!output_wav_file.empty()) {
            if (generate_beats_audio(result, output_wav_file)) {
                std::cout << "Beat audio generated: " << output_wav_file << std::endl;
            } else {
                std::cerr << "Failed to generate beat audio." << std::endl;
                return 1;
            }
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}