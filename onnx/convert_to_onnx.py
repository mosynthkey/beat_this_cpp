#!/usr/bin/env python3
"""
Convert Beat This! PyTorch checkpoint to ONNX format
"""
import sys
import os
import torch
import torch.onnx
import onnx
import numpy as np
import urllib.request

# Add the local beat_this module to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'beat_this'))

try:
    from beat_this.model.pl_module import BeatThis
    print("Successfully imported BeatThis from local beat_this module")
except ImportError as e:
    print(f"Failed to import from local beat_this module: {e}")
    print("Make sure the beat_this submodule is properly initialized")
    sys.exit(1)

# Model URL from the working guide
MODEL_URL = "https://cloud.cp.jku.at/public.php/dav/files/7ik4RrBKTS273gp/final0.ckpt"

def convert_checkpoint_to_onnx(checkpoint_path, output_path, verbose=False):
    """
    Convert Beat This! PyTorch checkpoint to ONNX format
    
    Args:
        checkpoint_path: Path to PyTorch checkpoint
        output_path: Path for output ONNX model
        verbose: Enable verbose logging
    """
    
    if verbose:
        print(f"🔧 Configuration:")
        print(f"  - Checkpoint: {checkpoint_path}")
        print(f"  - Output: {output_path}")
        print(f"  - Verbose: {verbose}")
        print()
    
    print(f"Loading checkpoint from: {checkpoint_path}")
    
    # Load the checkpoint
    try:
        checkpoint = torch.load(checkpoint_path, map_location='cpu')
        if verbose:
            print(f"✅ Checkpoint loaded successfully")
            print(f"  - Checkpoint keys: {list(checkpoint.keys())}")
            if 'hyper_parameters' in checkpoint:
                print(f"  - Model hyperparameters: {checkpoint['hyper_parameters']}")
        else:
            print("Checkpoint loaded successfully")
    except Exception as e:
        print(f"❌ Error loading checkpoint: {e}")
        return False
    
    # Initialize the model and load state dict with proper key handling
    try:
        model = BeatThis()
        
        # Handle Lightning checkpoint format - remove 'model.' prefix
        state_dict = checkpoint['state_dict']
        new_state_dict = {}
        for k, v in state_dict.items():
            if k.startswith('model.'):
                new_state_dict[k[6:]] = v  # Remove 'model.' prefix
            else:
                new_state_dict[k] = v
        
        model.load_state_dict(new_state_dict)
        model.eval()
        
        if verbose:
            print(f"✅ Model loaded and set to evaluation mode")
            print(f"  - Model type: {type(model)}")
            print(f"  - Model device: {next(model.parameters()).device}")
            print(f"  - Model parameters: {sum(p.numel() for p in model.parameters()):,}")
            print("✅ Successfully handled Lightning checkpoint format")
        else:
            print("Model loaded and set to evaluation mode")
        
    except Exception as e:
        print(f"❌ Error loading model: {e}")
        if verbose:
            import traceback
            print(f"Full traceback: {traceback.format_exc()}")
        return False
    
    # Create dummy input (mel spectrogram: batch_size=1, time_frames=300, freq_bins=128)
    # Model expects (batch, time, frequency) format
    dummy_input = torch.randn(1, 300, 128)
    
    if verbose:
        print(f"🔧 Created dummy input with shape: {dummy_input.shape}")
        print(f"  - Batch size: {dummy_input.shape[0]}")
        print(f"  - Time frames: {dummy_input.shape[1]}")
        print(f"  - Frequency bins: {dummy_input.shape[2]}")
        print(f"  - Input dtype: {dummy_input.dtype}")
    else:
        print(f"Created dummy input with shape: {dummy_input.shape}")
    
    # Test the model with dummy input to verify it works
    try:
        with torch.no_grad():
            test_output = model(dummy_input)
        
        if verbose:
            print(f"✅ Model test successful")
            print(f"  - Output keys: {list(test_output.keys())}")
            print(f"  - Beat output shape: {test_output['beat'].shape}")
            print(f"  - Beat output dtype: {test_output['beat'].dtype}")
            print(f"  - Beat output range: [{test_output['beat'].min():.3f}, {test_output['beat'].max():.3f}]")
            print(f"  - Downbeat output shape: {test_output['downbeat'].shape}")
            print(f"  - Downbeat output dtype: {test_output['downbeat'].dtype}")
            print(f"  - Downbeat output range: [{test_output['downbeat'].min():.3f}, {test_output['downbeat'].max():.3f}]")
        else:
            print(f"Model test successful. Output keys: {test_output.keys()}")
            print(f"Beat output shape: {test_output['beat'].shape}")
            print(f"Downbeat output shape: {test_output['downbeat'].shape}")
    except Exception as e:
        print(f"❌ Error during model test: {e}")
        if verbose:
            import traceback
            print(f"Full traceback: {traceback.format_exc()}")
        return False
    
    # Export to ONNX
    try:
        if verbose:
            print("🚀 Starting ONNX export...")
            print(f"  - Export parameters: True")
            print(f"  - ONNX opset version: 14")
            print(f"  - Input names: ['input_spectrogram']")
            print(f"  - Output names: ['beat', 'downbeat']")
            print(f"  - Dynamic axes: time dimension")
        else:
            print("Starting ONNX export...")
            
        torch.onnx.export(
            model,
            dummy_input,
            output_path,
            input_names=['input_spectrogram'],
            output_names=['beat', 'downbeat'],
            opset_version=14,  # Required for scaled_dot_product_attention
            dynamic_axes={
                'input_spectrogram': {1: 'time'},  # Variable time dimension
                'beat': {1: 'time'},
                'downbeat': {1: 'time'}
            },
            verbose=False  # Control verbose output separately
        )
        
        if verbose:
            print("✅ ONNX export completed")
            # Get file size
            file_size = os.path.getsize(output_path)
            print(f"  - Output file size: {file_size / (1024*1024):.1f} MB")
        else:
            print("ONNX export completed")
    except Exception as e:
        print(f"❌ Error during ONNX export: {e}")
        if verbose:
            import traceback
            print(f"Full traceback: {traceback.format_exc()}")
        return False
    
    # Verify the ONNX model
    try:
        if verbose:
            print("🔍 Verifying ONNX model...")
        else:
            print("Verifying ONNX model...")
            
        onnx_model = onnx.load(output_path)
        onnx.checker.check_model(onnx_model)
        
        if verbose:
            print("✅ ONNX model verification successful")
            print(f"  - Graph nodes: {len(onnx_model.graph.node)}")
            print(f"  - Graph inputs: {len(onnx_model.graph.input)}")
            print(f"  - Graph outputs: {len(onnx_model.graph.output)}")
            print(f"  - Initializers: {len(onnx_model.graph.initializer)}")
            
            # Print dynamic axes information
            for input_info in onnx_model.graph.input:
                print(f"  - Input '{input_info.name}':")
                for i, dim in enumerate(input_info.type.tensor_type.shape.dim):
                    if dim.dim_value > 0:
                        print(f"    - Dimension {i}: {dim.dim_value} (fixed)")
                    else:
                        print(f"    - Dimension {i}: {dim.dim_param} (dynamic)")
        else:
            print("ONNX model verification successful")
    except Exception as e:
        print(f"❌ Error during ONNX verification: {e}")
        if verbose:
            import traceback
            print(f"Full traceback: {traceback.format_exc()}")
        return False
    
    print(f"✅ Conversion completed successfully!")
    print(f"📁 ONNX model saved to: {output_path}")
    
    # Print model info
    if verbose:
        print("\nModel Information:")
        print(f"- Input: mel_spectrogram {list(dummy_input.shape)}")
        print(f"- Output 1: beat predictions")
        print(f"- Output 2: downbeat predictions")
        print(f"- ONNX Opset Version: 14")
        file_size = os.path.getsize(output_path)
        print(f"- File size: {file_size / (1024*1024):.1f} MB")
    
    return True

def download_model_if_needed(checkpoint_path):
    """Download model checkpoint if it doesn't exist"""
    if not os.path.exists(checkpoint_path):
        print(f"Downloading model from {MODEL_URL}...")
        urllib.request.urlretrieve(MODEL_URL, checkpoint_path)
        print(f"Model saved to {checkpoint_path}")
    else:
        print(f"Model checkpoint {checkpoint_path} already exists.")

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Convert Beat This! PyTorch checkpoint to ONNX format')
    parser.add_argument('checkpoint_path', nargs='?', default='final0.ckpt',
                       help='Path to the PyTorch checkpoint (.ckpt file) - will download if not found')
    parser.add_argument('output_path', nargs='?', default='beat_this.onnx',
                       help='Output path for the ONNX model (.onnx file)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Enable verbose logging')
    parser.add_argument('--download', action='store_true', 
                       help='Force download the model checkpoint')
    
    args = parser.parse_args()
    
    checkpoint_path = args.checkpoint_path
    output_path = args.output_path
    verbose = args.verbose
    
    # Download model if needed or requested
    if args.download or not os.path.exists(checkpoint_path):
        download_model_if_needed(checkpoint_path)
    
    if not os.path.exists(checkpoint_path):
        print(f"Error: Checkpoint file not found: {checkpoint_path}")
        sys.exit(1)
    
    success = convert_checkpoint_to_onnx(checkpoint_path, output_path, verbose)
    if not success:
        print(f"\n❌ Conversion failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()