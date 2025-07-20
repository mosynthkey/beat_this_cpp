# ONNX Model Setup

This directory contains the ONNX model file required for beat detection. The model is converted from the original PyTorch checkpoint provided by the Beat This! team.

## Quick Setup

The pre-converted ONNX model is already included in this repository as `beat_this.onnx`. You can start using the Beat This! C++ implementation immediately without any additional setup.

## Converting from PyTorch Checkpoint

To convert the original PyTorch model checkpoint to ONNX format:

### Prerequisites

1. **Python Environment**: Python 3.8+ with the following packages:
   ```bash
   pip install torch numpy einops rotary-embedding-torch
   pip install onnx
   ```

2. **Beat This! Module**: The conversion script uses a local beat_this module (included as a submodule).

### Step 1: Download the Model Checkpoint

Download the pre-trained model checkpoint from the original Beat This! project:
```bash
# Download the checkpoint file (approximately 400MB)
wget https://cloud.cp.jku.at/public.php/dav/files/7ik4RrBKTS273gp/final0.ckpt -O final0.ckpt
```

Or manually download from: https://cloud.cp.jku.at/public.php/dav/files/7ik4RrBKTS273gp/final0.ckpt

This checkpoint contains the trained transformer-based Beat This! model weights published with the original research paper.

### Step 2: Use the Conversion Script

The conversion script `convert_to_onnx.py` is provided in this directory. It includes:

- **Automatic module detection**: Works with the local beat_this submodule
- **Proper input/output naming**: Compatible with the C++ implementation
- **State dict key handling**: Resolves model prefix issues from Lightning checkpoints
- **Error handling**: Comprehensive validation and debugging output
- **Model verification**: Automatic ONNX model validation

Key features of the conversion script:
```python
# Load model from Lightning checkpoint with proper key handling
model = BeatThis()
checkpoint = torch.load(checkpoint_path, map_location='cpu')
state_dict = checkpoint['state_dict']
new_state_dict = {}
for k, v in state_dict.items():
    if k.startswith('model.'):
        new_state_dict[k[6:]] = v  # Remove 'model.' prefix
    else:
        new_state_dict[k] = v
model.load_state_dict(new_state_dict)

# Input format: (batch_size, time_frames, freq_bins)
dummy_input = torch.randn(1, 300, 128)

# Export with correct names and dynamic axes for C++ compatibility
torch.onnx.export(
    model,
    dummy_input,
    output_path,
    input_names=['input_spectrogram'],  # Expected by C++ implementation
    output_names=['beat', 'downbeat'],  # Expected by C++ implementation
    opset_version=14,  # Required for scaled_dot_product_attention
    dynamic_axes={
        'input_spectrogram': {1: 'time'},  # Variable time dimension
        'beat': {1: 'time'},
        'downbeat': {1: 'time'}
    }
)
```

**Important**: The time dimension (dimension 1) must be dynamic for proper compatibility with the C++ implementation. The script properly handles PyTorch Lightning checkpoint format and removes the 'model.' prefix from state dict keys.

### Step 3: Run the Conversion

```bash
# Install required dependencies
pip install torch numpy einops rotary-embedding-torch
pip install onnx

# Run the conversion script
python convert_to_onnx.py final0.ckpt beat_this.onnx

# Run with verbose logging for detailed information
python convert_to_onnx.py final0.ckpt beat_this.onnx --verbose
```

The script will:
1. Load the PyTorch Lightning checkpoint
2. Handle state dict key prefixes from Lightning wrapper
3. Load the Beat This! model
4. Test the model with dummy input
5. Export to ONNX format with proper input/output names
6. Verify the exported model

**Basic output:**
```
Loading checkpoint from: final0.ckpt
Checkpoint loaded successfully
Model loaded and set to evaluation mode
Created dummy input with shape: torch.Size([1, 300, 128])
Model test successful. Output keys: dict_keys(['beat', 'downbeat'])
Beat output shape: torch.Size([1, 300])
Downbeat output shape: torch.Size([1, 300])
Starting ONNX export...
ONNX export completed
ONNX model verification successful
✅ Conversion completed successfully!
📁 ONNX model saved to: beat_this.onnx
```

**Verbose output includes additional details:**
- Model hyperparameters and architecture information
- Parameter counts and device information
- Detailed output tensor ranges and types
- ONNX graph structure (nodes, inputs, outputs)
- Dynamic axis configuration verification
- File size and optimization settings

## Model Architecture Details

The Beat This! model has the following input/output specifications:

- **Input**: Mel spectrogram with shape `[batch_size, time_frames, 128]`
- **Output**: Two tensors:
  - Beat predictions: `[batch_size, time_frames]`
  - Downbeat predictions: `[batch_size, time_frames]`

## Troubleshooting

### Missing Dependencies
If you encounter import errors, ensure you have installed all required packages:
```bash
pip install torch numpy einops rotary-embedding-torch onnx
```

### Model Loading Issues
The conversion script handles PyTorch Lightning checkpoint format automatically by:
1. Loading the checkpoint and extracting the state_dict
2. Removing the 'model.' prefix from Lightning wrapper keys
3. Loading the cleaned state_dict into the BeatThis model

### ONNX Version Compatibility
The script uses ONNX opset version 14, which is required for the `scaled_dot_product_attention` operator used in the transformer model. If you encounter compatibility issues:
```python
torch.onnx.export(..., opset_version=14, ...)  # Required for transformer operations
```

### Input Dimension Issues
If you encounter errors related to input dimensions, ensure:
- Input shape is `(batch_size, time_frames, 128)` (3D, not 4D)
- Time dimension (dimension 1) is set as dynamic
- Model expects mel spectrograms, not raw audio

### C++ Integration Issues
For successful C++ integration, the ONNX model must have:
- Input name: `input_spectrogram`
- Output names: `beat`, `downbeat`
- Dynamic time dimension for variable-length audio processing

## Model Information

- **Original Model**: Beat This! transformer-based beat tracking
- **Input Format**: 128-dimensional Mel spectrograms
- **Sample Rate**: 22050 Hz (internal processing)
- **Frame Rate**: Typically ~87 Hz (depends on hop length)
- **Model Size**: Approximately 10-50 MB (varies by architecture)

## Verification

After conversion, you can verify the model works with the C++ implementation:
```bash
cd /path/to/beat_this_cpp
./build/beat_this_cpp onnx/beat_this.onnx test_audio.wav --output-beats test.beats
```

Expected output for successful verification:
```
Loaded audio: 32970042 samples, 44100 Hz, 2 channels
Found 1183 beats and 363 downbeats
Beats saved to: test.beats
```

If you see this output, the ONNX model conversion was successful and the model is working correctly with the C++ implementation.