#!/usr/bin/env python3
# main_train.py
import torch
import torch.optim as optim
from torch.utils.data import DataLoader, random_split
import argparse
import os
from pathlib import Path
from model import End2EndModel
from dataset import CarlaDataset
from config import *

def main():
    parser = argparse.ArgumentParser(description='Train end-to-end model')
    parser.add_argument('--data_dir', required=True, help='Directory containing collected data')
    parser.add_argument('--epochs', type=int, default=EPOCHS, help='Number of epochs')
    parser.add_argument('--batch_size', type=int, default=BATCH_SIZE, help='Batch size')
    parser.add_argument('--lr', type=float, default=LEARNING_RATE, help='Learning rate')
    parser.add_argument('--save_path', default=MODEL_SAVE_PATH, help='Path to save model')
    args = parser.parse_args()

    # 设备
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f'Using device: {device}')

    # 数据集
    full_dataset = CarlaDataset(args.data_dir, train_split=TRAIN_VAL_SPLIT)
    train_size = int(len(full_dataset) * TRAIN_VAL_SPLIT)
    val_size = len(full_dataset) - train_size
    train_dataset, val_dataset = random_split(full_dataset, [train_size, val_size])
    train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=args.batch_size, shuffle=False)

    # 模型
    model = End2EndModel(input_shape=INPUT_SHAPE, output_dim=OUTPUT_DIM)
    model.to(device)

    # 优化器和损失
    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    criterion = torch.nn.MSELoss()

    # 训练循环
    best_val_loss = float('inf')
    for epoch in range(1, args.epochs + 1):
        model.train()
        train_loss = 0.0
        for imgs, actions in train_loader:
            imgs, actions = imgs.to(device), actions.to(device)
            optimizer.zero_grad()
            outputs = model(imgs)
            loss = criterion(outputs, actions)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * imgs.size(0)

        train_loss /= len(train_loader.dataset)

        # 验证
        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for imgs, actions in val_loader:
                imgs, actions = imgs.to(device), actions.to(device)
                outputs = model(imgs)
                loss = criterion(outputs, actions)
                val_loss += loss.item() * imgs.size(0)
        val_loss /= len(val_loader.dataset)

        print(f'Epoch {epoch}/{args.epochs} - Train Loss: {train_loss:.4f}, Val Loss: {val_loss:.4f}')

        # 保存最佳模型
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            os.makedirs(os.path.dirname(args.save_path), exist_ok=True)
            torch.save(model.state_dict(), args.save_path)
            print(f'Model saved to {args.save_path}')

    print('Training finished.')

if __name__ == '__main__':
    main()