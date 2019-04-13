#pragma once

#include "stdafx.h"

// State structure
struct keccakState
{
	uint64_t *A;
	unsigned int blockLen;
	uint8_t *buffer;
	unsigned int bufferLen;
	int length;
	unsigned int d;
};

void keccakProcessBuffer(struct keccakState *state);
void keccakAddPadding(keccakState *state);
void keccakf(keccakState *state);

struct keccakState *keccakCreate(int length);
void keccakUpdate(const uint8_t *input, int off, unsigned int len, keccakState *state);
unsigned char *keccakDigest(keccakState *state);


