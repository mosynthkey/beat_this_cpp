#ifndef BEAT_THIS_API_H
#define BEAT_THIS_API_H

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

// Define a struct to hold the results
typedef struct {
    float* beats;
    int num_beats;
    float* downbeats;
    int num_downbeats;
} BeatThisResult;

// Function to initialize the BeatThis API
// Returns a pointer to an internal context, or NULL on failure.
void* BeatThis_init(const char* onnx_model_path);

// Function to process audio and get beat/downbeat times
// context: Pointer returned by BeatThis_init
// audio_data: Pointer to float array of audio samples
// num_samples: Number of audio samples
// samplerate: Sample rate of the audio data
// Returns a BeatThisResult struct. The caller is responsible for freeing the beats and downbeats arrays using BeatThis_free_result.
BeatThisResult BeatThis_process_audio(
    void* context,
    const float* audio_data,
    int num_samples,
    int samplerate,
    int original_channels
);

// Function to free the memory allocated for BeatThisResult
void BeatThis_free_result(BeatThisResult result);

// Function to save beat information to a .beats file
// context: Pointer returned by BeatThis_init
// result: BeatThisResult struct containing beat and downbeat times
// output_filepath: Path to the output .beats file
// Returns true on success, false on failure.
bool BeatThis_save_beats_to_file(
    void* context,
    BeatThisResult result,
    const char* output_filepath
);

// Function to clean up the BeatThis API context
void BeatThis_cleanup(void* context);


#ifdef __cplusplus
}
#endif

#endif // BEAT_THIS_API_H
