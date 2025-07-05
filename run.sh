#!/bin/bash

# Script for running PQC Wallet application

echo "Starting PQC Wallet..."

# Check if application is compiled
if [ ! -f "build/PQCWallet" ]; then
    echo "Application not compiled. Compiling now..."
    cd build
    cmake ..
    make
    cd ..
fi

# Run the application
echo "Running application..."
./build/PQCWallet

echo "Application closed."
