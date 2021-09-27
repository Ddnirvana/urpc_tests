# URPC Tests

A simple case to run and evaluate URPC.

The URPC implementation is now ISA-agnostic (please update the CACHE_LINE size
according to your platform).

However, the code currently uses rdtsc to read cycles (therefore x64-specific).
You should change the rdtsc if you want to run the tests in other platforms.

## Quick Start

To build:

	make urpc

To run:

	./urpc


That's all.
You can see the results printed on the screen.

## Infos

The implementation is inspired by URPC, Barrelfish, and LXDs.
