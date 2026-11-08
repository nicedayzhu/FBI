#!/bin/bash

read -p "3DS IP: " -e input
python servefiles.py "$input" .
