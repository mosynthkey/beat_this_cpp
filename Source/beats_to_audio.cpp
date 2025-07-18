#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <algorithm>

#include "sndfile.h"

// Audio parameters
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

// Function to read .beats file
std::vector<std::pair<double, int>> read_beats_file(const std::string& filepath) {
    std::vector<std::pair<double, int>> beats;
    std::ifstream infile(filepath);
    std::string line;

    if (!infile.is_open()) {
        std::cerr << "Error: Could not open .beats file: " << filepath << std::endl;
        return beats;
    }

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        double time;
        int beat_number;
        if (!(iss >> time >> beat_number)) { break; } // Error or end of file
        beats.push_back({time, beat_number});
    }
    infile.close();
    return beats;
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

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_beats_file> <output_wav_file>" << std::endl;
        return 1;
    }

    std::string input_beats_file = argv[1];
    std::string output_wav_file = argv[2];

    std::vector<std::pair<double, int>> beats = read_beats_file(input_beats_file);
    if (beats.empty()) {
        return 1;
    }

    // Determine total duration of the audio to be generated
    // Add some padding at the end to ensure the last beat has enough time to decay
    double total_duration = beats.back().first + DURATION_PER_BEAT + DECAY_TIME;
    int total_samples = static_cast<int>(total_duration * SAMPLE_RATE);
    std::vector<float> final_audio(total_samples, 0.0f);

    for (const auto& beat : beats) {
        double frequency;
        if (beat.second == 1) { // Downbeat
            frequency = 880.0; // Hz
        } else {
            frequency = 440.0; // Hz
        }

        std::vector<float> beat_waveform = generate_sine_wave(
            frequency, DURATION_PER_BEAT, ATTACK_TIME, DECAY_TIME, SAMPLE_RATE
        );

        // Mix the generated waveform into the final audio buffer
        int start_sample = static_cast<int>(beat.first * SAMPLE_RATE);
        for (size_t i = 0; i < beat_waveform.size(); ++i) {
            if (start_sample + i < total_samples) {
                final_audio[start_sample + i] += beat_waveform[i];
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

    if (write_wav_file(output_wav_file, final_audio, SAMPLE_RATE)) {
        std::cout << "Successfully generated audio: " << output_wav_file << std::endl;
        return 0;
    } else {
        std::cerr << "Failed to generate audio." << std::endl;
        return 1;
    }
}
