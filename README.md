# Carla End-to-End Autonomous Driving

This project implements an end-to-end autonomous driving system using Carla simulator. It includes data collection, model training, and online testing modules. Each module can be run independently via `main_*.py` scripts.

## Project Structure
carla_end2end/
├── README.md
├── requirements.txt
├── config.py
├── model.py
├── dataset.py
├── utils.py
├── main_collect_data.py
├── main_train.py
├── main_test.py
├── pretrained/ # Saved models (created automatically)
└── data/ # Collected data (created automatically)

## Environment Setup

### Prerequisites
- **Operating System**: Ubuntu 20.04 (recommended), Windows 10, or macOS
- **Carla Simulator**: Version 0.9.13 ([Download](https://github.com/carla-simulator/carla/releases/tag/0.9.13))
- **Python**: 3.8 or higher
- **CUDA**: 11.3 (optional, for GPU training)

### Installation Steps
1. **Clone the repository**:
   ```bash
   git clone https://github.com/ovo-ovo-ovo-ovo/carla_end2end.git
   cd carla_end2end

2. **Install Python dependencies**:
    ```bash
    pip install -r requirements.txt
   
3. **Start Carla server**:
- **Navigate to your Carla installation directory.**
- **Run the server (adjust graphics quality for performance)**:
  ```bash
  ./CarlaUE4.sh -quality-level=Low -benchmark -fps=10
   
### Usage
1. **Data Collection**:
    Collect training data by driving manually or using Carla's autopilot.
   ```bash
   python main_collect_data.py --save_dir ./data/run_001 --frames 2000 --autopilot
   
- **--save_dir**: Directory to save images and actions.
- **--frames**: Number of frames to collect.
- **--autopilot**: Use Carla autopilot (default: manual driving; you need to control the vehicle yourself).
Note: If not using autopilot, you must manually drive the vehicle using the keyboard (WASD) in the Carla window.

2. **Training**:
Train the neural network using the collected data.
    ```bash
    python main_train.py --data_dir ./data/run_001 --epochs 30
- **--data_dir**: Path to the collected data directory.
- **--epochs**: Number of training epochs.
- **The best model will be saved to pretrained/model.pth by default.**

3. **Testing**:
Test the trained model in Carla.
  ```bash
  python main_test.py --model_path pretrained/model.pth