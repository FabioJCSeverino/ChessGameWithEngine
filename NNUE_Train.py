import csv
import random

import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader


DATASET_FILE = "dataset.csv"
MAX_POSITIONS = 500_000
TRAIN_RATIO = 0.9
BATCH_SIZE = 1024
EPOCHS = 50
LEARNING_RATE = 0.001
SEED = 42

random.seed(SEED)
torch.manual_seed(SEED)



# Chess features
PIECE_INDEX = {
    'P': 0,
    'N': 1,
    'B': 2,
    'R': 3,
    'Q': 4,
    'K': 5,

    'p': 6,
    'n': 7,
    'b': 8,
    'r': 9,
    'q': 10,
    'k': 11,
}


def fen_to_features(fen):

    parts = fen.split()

    board = parts[0]
    side_to_move = parts[1]

    features = torch.zeros(769, dtype=torch.float32)

    rank = 7
    file = 0

    for c in board:

        if c == '/':
            rank -= 1
            file = 0

        elif c.isdigit():
            file += int(c)

        else:

            piece_index = PIECE_INDEX[c]
            square = rank * 8 + file

            feature_index = (piece_index * 64 + square)

            features[feature_index] = 1.0

            file += 1

    # Side to move
    if side_to_move == 'w':
        features[768] = 1.0

    return features


# Read CSV

def load_dataset(filename, max_positions):

    positions = []

    with open(filename, "r", encoding="utf-8") as file:

        reader = csv.DictReader(file)

        for row in reader: 

            fen = row["fen"]

            score = float(row["evaluation"])

            positions.append((fen, score))

            if len(positions) >= max_positions:
                break

    print(f"Loaded {len(positions):,} positions")

    return positions


# Dataset

class ChessDataset(Dataset):

    def __init__(self, positions):
        self.positions = positions

    def __len__(self):
        return len(self.positions)

    def __getitem__(self, index):

        fen, score = self.positions[index]

        features = fen_to_features(fen)

        target = torch.tensor([score], dtype=torch.float32)

        return features, target


# NNUE

def clipReLU(x):
    return torch.clamp(x, 0.0, 1.0)


class NeuralNet(nn.Module):

    def __init__(self):

        super().__init__()

        self.fc1 = nn.Linear(769, 32)
        self.fc2 = nn.Linear(32, 32)
        self.fc3 = nn.Linear(32, 8)
        self.fc4 = nn.Linear(8, 1)

    def forward(self, x):

        x = clipReLU(self.fc1(x))
        x = clipReLU(self.fc2(x))
        x = clipReLU(self.fc3(x))

        x = self.fc4(x)

        return x


# Training

def train(model, train_loader, val_loader, device):

    criterion = nn.MSELoss()

    optimizer = torch.optim.Adam(model.parameters(), lr=LEARNING_RATE)

    for epoch in range(EPOCHS):
        # Training

        model.train()

        train_loss = 0.0

        for features, targets in train_loader:

            features = features.to(device)
            targets = targets.to(device)

            optimizer.zero_grad()
            predictions = model(features)
            loss = criterion(predictions, targets)

            loss.backward()
            optimizer.step()
            train_loss += loss.item()

        train_loss /= len(train_loader)

        # Validation

        model.eval()

        val_loss = 0.0

        with torch.no_grad():

            for features, targets in val_loader:

                features = features.to(device)
                targets = targets.to(device)

                predictions = model(features)

                loss = criterion(predictions, targets)
                val_loss += loss.item()

        val_loss /= len(val_loader)

        print(
            f"Epoch {epoch + 1}/{EPOCHS} "
            f"Train Loss: {train_loss:.6f} "
            f"Val Loss: {val_loss:.6f}"
        )

# Main

def main():

    device = torch.device(
        "cuda"
        if torch.cuda.is_available()
        else "cpu"
    )

    print("Device:", device)

    # Load CSV
    positions = load_dataset(DATASET_FILE, MAX_POSITIONS)

    # Shuffle
    random.shuffle(positions)

    # Train / validation split
    split = int(len(positions) * TRAIN_RATIO)

    train_positions = positions[:split]

    val_positions = positions[split:]

    print(
        f"Training positions: "
        f"{len(train_positions):,}"
    )

    print(
        f"Validation positions: "
        f"{len(val_positions):,}"
    )

    # Datasets
    train_dataset = ChessDataset(train_positions)

    val_dataset = ChessDataset(val_positions)

    # DataLoaders
    train_loader = DataLoader(
        train_dataset,
        batch_size=BATCH_SIZE,
        shuffle=True,
        num_workers=0
    )

    val_loader = DataLoader(
        val_dataset,
        batch_size=BATCH_SIZE,
        shuffle=False,
        num_workers=0
    )

    #Model
    model = NeuralNet()

    # Load Model for retraining
    #model.load_state_dict(torch.load("modelo.pt", weights_only=True))
    #model.eval()
    

    # Train
    train(model, train_loader, val_loader, device)

    exemplo = torch.rand(1, 769, dtype=torch.float32)

    traced = torch.jit.trace(model, exemplo)
    traced.save("modelo.pt")   

    print("Saved model to modelo.pt")


if __name__ == "__main__":
    main()