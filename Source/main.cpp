#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <set>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <filesystem> // For absolute path conversion

#include "miniaudio.h"


#include "beat_this_api.h"

// Function to load audio from file
bool load_audio_for_example(const std::string& path, std::vector<float>& audio_buffer, int& samplerate, int& channels) {
    ma_result result;
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 0);

    result = ma_decoder_init_file(path.c_str(), &config, &decoder);
    if (result != MA_SUCCESS) {
        std::cerr << "Error: could not open audio file '" << path << "': " << ma_result_description(result) << std::endl;
        return false;
    }

    samplerate = decoder.outputSampleRate;
    channels = decoder.outputChannels;

    // Read all audio data
    ma_uint64 total_frames = 0;
    result = ma_decoder_get_length_in_pcm_frames(&decoder, &total_frames);
    if (result != MA_SUCCESS) {
        std::cerr << "Error: could not get audio length: " << ma_result_description(result) << std::endl;
        ma_decoder_uninit(&decoder);
        return false;
    }

    audio_buffer.resize(total_frames * channels);
    ma_uint64 frames_read = 0;
    result = ma_decoder_read_pcm_frames(&decoder, audio_buffer.data(), total_frames, &frames_read);
    if (result != MA_SUCCESS || frames_read != total_frames) {
        std::cerr << "Error: could not read all audio frames: " << ma_result_description(result) << std::endl;
        ma_decoder_uninit(&decoder);
        return false;
    }

    ma_decoder_uninit(&decoder);
    return true;
}

// Function to save beat information to .beats file
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

        waveform[i] = static_cast<float>(amplitude * std::sin(2.0 * std::numbers::pi * frequency * t));
    }
    return waveform;
}

// Function to write WAV file
bool write_wav_file(const std::string& filepath, const std::vector<float>& audio_data, int sample_rate, int channels = 1) {
    ma_result result;
    ma_encoder encoder;
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, (ma_uint32)channels, (ma_uint32)sample_rate);

    result = ma_encoder_init_file(filepath.c_str(), &config, &encoder);
    if (result != MA_SUCCESS) {
        std::cerr << "Error: Could not open WAV file for writing: " << ma_result_description(result) << std::endl;
        return false;
    }

    ma_uint64 frames_written = 0;
    result = ma_encoder_write_pcm_frames(&encoder, audio_data.data(), audio_data.size() / channels, &frames_written);
    if (result != MA_SUCCESS || frames_written != audio_data.size() / channels) {
        std::cerr << "Error: Could not write all audio frames: " << ma_result_description(result) << std::endl;
        ma_encoder_uninit(&encoder);
        return false;
    }

    ma_encoder_uninit(&encoder);
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

// Function to generate mixed audio (original + beats)
bool generate_mixed_audio(const BeatThis::BeatResult& result, 
                         const std::vector<float>& original_audio, 
                         int original_samplerate,
                         int original_channels,
                         const std::string& output_wav_file) {
    if (result.beats.empty()) {
        std::cerr << "Error: No beats to generate audio from." << std::endl;
        return false;
    }

    // Preserve original audio characteristics
    int output_samplerate = original_samplerate;
    int output_channels = original_channels;
    
    // Calculate total duration
    double last_beat_time = result.beats.back() + DURATION_PER_BEAT + DECAY_TIME;
    double original_duration = static_cast<double>(original_audio.size()) / (output_samplerate * output_channels);
    double total_duration = std::max(last_beat_time, original_duration);
    
    int total_samples = static_cast<int>(total_duration * output_samplerate * output_channels);
    std::vector<float> final_audio(total_samples, 0.0f);

    // Copy original audio with reduced volume
    size_t copied_samples = std::min(original_audio.size(), static_cast<size_t>(total_samples));
    for (size_t i = 0; i < copied_samples; ++i) {
        final_audio[i] = original_audio[i] * 0.7f;
    }

    // Add click track
    for (size_t i = 0; i < result.beats.size(); ++i) {
        double frequency = (result.beat_counts[i] == 1) ? 880.0 : 440.0; // Downbeat : Other beats
        
        std::vector<float> beat_waveform = generate_sine_wave(
            frequency, DURATION_PER_BEAT, ATTACK_TIME, DECAY_TIME, output_samplerate
        );

        int start_frame = static_cast<int>(result.beats[i] * output_samplerate);
        
        for (size_t j = 0; j < beat_waveform.size(); ++j) {
            int frame_pos = start_frame + j;
            if (output_channels == 2) {
                // Stereo: add click to both channels
                int left_sample = frame_pos * 2;
                int right_sample = frame_pos * 2 + 1;
                if (left_sample < total_samples && right_sample < total_samples) {
                    final_audio[left_sample] += beat_waveform[j] * 0.3f;
                    final_audio[right_sample] += beat_waveform[j] * 0.3f;
                }
            } else {
                // Mono: add click directly
                if (frame_pos < total_samples) {
                    final_audio[frame_pos] += beat_waveform[j] * 0.3f;
                }
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

    return write_wav_file(output_wav_file, final_audio, output_samplerate, output_channels);
}

// Function to calculate BPM from beat timestamps
double calculate_bpm(const BeatThis::BeatResult& result) {
    if (result.beats.size() < 2) {
        return 0.0; // Need at least 2 beats to calculate intervals
    }

    std::vector<double> intervals;
    intervals.reserve(result.beats.size() - 1);
    
    // Calculate inter-beat intervals
    for (size_t i = 1; i < result.beats.size(); ++i) {
        double interval = result.beats[i] - result.beats[i-1];
        if (interval > 0.1 && interval < 3.0) { // Filter out unrealistic intervals (less than 20 BPM or more than 600 BPM)
            intervals.push_back(interval);
        }
    }
    
    if (intervals.empty()) {
        return 0.0;
    }
    
    // Use median interval to avoid outliers affecting the result
    std::sort(intervals.begin(), intervals.end());
    double median_interval;
    size_t n = intervals.size();
    if (n % 2 == 0) {
        median_interval = (intervals[n/2 - 1] + intervals[n/2]) / 2.0;
    } else {
        median_interval = intervals[n/2];
    }
    
    // Convert interval to BPM: 60 seconds per minute / interval in seconds
    return 60.0 / median_interval;
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <onnx_model_path> <audio_file_path> [options]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  --output-beats <file>    Save beat information to .beats file" << std::endl;
    std::cerr << "  --output-audio <file>    Generate audio file with beats as click track" << std::endl;
    std::cerr << "  --output-mixed <file>    Generate audio file with original music + click track" << std::endl;
    std::cerr << "  --calc-bpm               Calculate and display BPM from detected beats" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --output-beats output.beats" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --output-audio output.wav" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --output-mixed mixed.wav" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --calc-bpm" << std::endl;
    std::cerr << "  " << program_name << " model.onnx input.wav --output-beats output.beats --calc-bpm" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string onnx_model_path = argv[1];
    const std::string audio_file_path = argv[2];
    
    // Convert to absolute paths
    std::filesystem::path onnx_path = std::filesystem::absolute(onnx_model_path);
    std::filesystem::path audio_path = std::filesystem::absolute(audio_file_path);
    
    std::string output_beats_file;
    std::string output_wav_file;
    std::string output_mixed_file;
    bool calc_bpm = false;

    // Parse command line arguments
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--output-beats" && i + 1 < argc) {
            output_beats_file = argv[++i];
        } else if (arg == "--output-audio" && i + 1 < argc) {
            output_wav_file = argv[++i];
        } else if (arg == "--output-mixed" && i + 1 < argc) {
            output_mixed_file = argv[++i];
        } else if (arg == "--calc-bpm") {
            calc_bpm = true;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Check if at least one output is specified
    if (output_beats_file.empty() && output_wav_file.empty() && output_mixed_file.empty() && !calc_bpm) {
        std::cerr << "Error: At least one output option must be specified (--output-beats, --output-audio, --output-mixed, or --calc-bpm)" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    try {
        // Initialize the BeatThis API
        BeatThis::BeatThis beat_analyzer(onnx_path.string());

        // Load audio for example
        std::vector<float> audio_buffer;
        int samplerate;
        int channels;
        if (!load_audio_for_example(audio_path.string(), audio_buffer, samplerate, channels)) {
            return 1;
        }

        std::cout << "Loaded audio: " << audio_buffer.size() << " samples, " << samplerate << " Hz, " << channels << " channels" << std::endl;

        // Process audio
        auto result = beat_analyzer.process_audio(audio_buffer, samplerate, channels);

        std::cout << "Found " << result.beats.size() << " beats and " << result.downbeats.size() << " downbeats" << std::endl;

        // Calculate and display BPM if requested
        if (calc_bpm) {
            double bpm = calculate_bpm(result);
            if (bpm > 0.0) {
                std::cout << "Estimated BPM: " << std::fixed << std::setprecision(1) << bpm << std::endl;
            } else {
                std::cout << "Could not calculate BPM (insufficient or invalid beat data)" << std::endl;
            }
        }

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

        // Generate mixed audio if requested
        if (!output_mixed_file.empty()) {
            if (generate_mixed_audio(result, audio_buffer, samplerate, channels, output_mixed_file)) {
                std::cout << "Mixed audio generated: " << output_mixed_file << std::endl;
            } else {
                std::cerr << "Failed to generate mixed audio." << std::endl;
                return 1;
            }
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}