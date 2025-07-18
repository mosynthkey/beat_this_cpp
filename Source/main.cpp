#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>

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

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <onnx_model_path> <audio_file_path> <output_beats_file>" << std::endl;
        return 1;
    }
    const char* onnx_model_path = argv[1];
    const std::string audio_file_path = argv[2];
    const char* output_beats_file = argv[3];

    // Initialize the BeatThis API
    void* api_context = BeatThis_init(onnx_model_path);
    if (!api_context) {
        std::cerr << "Failed to initialize BeatThis API." << std::endl;
        return 1;
    }

    // Load audio for example
    std::vector<float> audio_buffer;
    int samplerate;
    int channels;
    if (!load_audio_for_example(audio_file_path, audio_buffer, samplerate, channels)) {
        BeatThis_cleanup(api_context);
        return 1;
    }

    std::cout << "Loaded audio: " << audio_buffer.size() << " samples, " << samplerate << " Hz, " << channels << " channels" << std::endl;

    // Process audio
    BeatThisResult result = BeatThis_process_audio(
        api_context,
        audio_buffer.data(),
        audio_buffer.size(),
        samplerate,
        channels // Pass original channels
    );

    // Save results to .beats file
    if (BeatThis_save_beats_to_file(api_context, result, output_beats_file)) {
        std::cout << "\nBeats saved to: " << output_beats_file << std::endl;
    } else {
        std::cerr << "Failed to save beats to file." << std::endl;
    }

    // Clean up
    BeatThis_free_result(result);

    BeatThis_cleanup(api_context);

    return 0;
}