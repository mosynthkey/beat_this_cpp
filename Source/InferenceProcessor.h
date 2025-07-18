#ifndef INFERENCE_PROCESSOR_H
#define INFERENCE_PROCESSOR_H

#include <vector>
#include <string>
#include "onnxruntime_cxx_api.h"

/**
 * @class InferenceProcessor
 * @brief Handles neural network inference for beat detection
 * 
 * This class processes Mel spectrograms through a trained ONNX model to detect
 * beats and downbeats. It handles chunking of long audio sequences and
 * aggregates predictions from overlapping chunks.
 */
class InferenceProcessor {
public:
    InferenceProcessor(Ort::Session& session, Ort::Env& env);

    /**
     * @brief Process a full spectrogram and return beat/downbeat logits
     * @param spectrogram Input Mel spectrogram [frames][mel_bins]
     * @return Pair of vectors containing beat and downbeat logits
     */
    std::pair<std::vector<float>, std::vector<float>> process_spectrogram(
        const std::vector<std::vector<float>>& spectrogram
    );

private:
    Ort::Session& session_;
    Ort::Env& env_;

    // Chunking parameters (must match Python implementation)
    const int chunk_size = 1500;      // Size of each chunk in frames
    const int border_size = 6;        // Border size for overlap handling
    // overlap_mode is "keep_first" in Python, processed in reverse order

    // Helper functions for chunking and aggregation
    std::vector<std::vector<std::vector<float>>> split_piece(
        const std::vector<std::vector<float>>& spect,
        int chunk_size,
        int border_size
    );

    std::pair<std::vector<float>, std::vector<float>> aggregate_prediction(
        const std::vector<std::pair<std::vector<float>, std::vector<float>>>& pred_chunks,
        const std::vector<int>& starts,
        int full_size,
        int chunk_size,
        int border_size
    );

    // Helper to run ONNX inference on a single chunk
    std::pair<std::vector<float>, std::vector<float>> run_onnx_inference(
        const std::vector<std::vector<float>>& chunk_spect
    );
};

#endif // INFERENCE_PROCESSOR_H
