#!/usr/bin/env python3
import os

Import("env")

os.chdir("src")
env.Execute("nanopb_generator.py thermalcam.proto")
