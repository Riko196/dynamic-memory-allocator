#include "wrapper.h"
#include <stdio.h>
#define MEMORY_BLOCK_POINTER_SIZE 5
#define INTEGER_SIZE 4

struct byte_int
{
	uint8_t byte[INTEGER_SIZE];
};

struct memory_block_pointer
{
	struct byte_int next;
	uint8_t allocated;
};

struct byte_int int_to_byte_int(unsigned int number)
{
	struct byte_int byte_number;
	for (int i = 0; i < INTEGER_SIZE; i++)
		byte_number.byte[i] = (number >> (8 * (INTEGER_SIZE - 1 - i))) & 0xFF;
	return byte_number;
}

uint32_t byte_int_to_int(struct byte_int number)
{
	uint32_t int_number = 0;
	for (int i = 0; i < INTEGER_SIZE; i++)
		int_number |= ((uint32_t)number.byte[i] << (8 * (INTEGER_SIZE - 1 - i)));
	return int_number;
}

void write_int_to_memory(unsigned int addr, struct byte_int number)
{
	for (int i = 0; i < INTEGER_SIZE; i++)
		mwrite(addr + i, number.byte[i]);
}

struct byte_int read_int_from_memory(unsigned int addr)
{
	struct byte_int number;
	for (int i = 0; i < INTEGER_SIZE; i++)
		number.byte[i] = mread(addr + i);
	return number;
}

void write_memory_block_pointer_to_memory(unsigned int addr, struct memory_block_pointer block)
{
	write_int_to_memory(addr, block.next);
	mwrite(addr + INTEGER_SIZE, block.allocated);
}

struct memory_block_pointer read_memory_block_pointer_from_memory(unsigned int addr)
{
	struct memory_block_pointer block;
	block.next = read_int_from_memory(addr);
	block.allocated = mread(addr + INTEGER_SIZE);
	return block;
}

void my_init(void)
{
	if (msize() < MEMORY_BLOCK_POINTER_SIZE)
		return;

	struct memory_block_pointer first_block;
	first_block.next = int_to_byte_int(msize());
	first_block.allocated = 0;
	write_memory_block_pointer_to_memory(0, first_block);
}
//it unions all continuous blocks which have allocation 0
void defragmentation(struct memory_block_pointer current_block, uint32_t current_index)
{
	while (1)
	{
		uint32_t next_index = byte_int_to_int(read_int_from_memory(current_index));

		if (next_index >= msize())
			return;

		struct memory_block_pointer next_block = read_memory_block_pointer_from_memory(next_index);

		if ((current_block.allocated == 0) && (next_block.allocated == 0))
		{
			current_block.next = next_block.next;
			write_memory_block_pointer_to_memory(current_index, current_block);
		}
		else
			return;
	}
}

int my_alloc(unsigned int size)
{
	if (msize() <= size || msize() < MEMORY_BLOCK_POINTER_SIZE)
		return FAIL;

	uint32_t current_index = 0;
	uint32_t next_index = byte_int_to_int(read_int_from_memory(current_index));

	struct memory_block_pointer current_block = read_memory_block_pointer_from_memory(current_index);

	while (1)
	{
		if (current_block.allocated == 0)
		{
			defragmentation(current_block, current_index);
			next_index = byte_int_to_int(read_int_from_memory(current_index));
		}

		int free_memory_in_this_block = next_index - (current_index + 2 * MEMORY_BLOCK_POINTER_SIZE);
		if ((current_block.allocated == 0) && (free_memory_in_this_block >= 0) && (free_memory_in_this_block >= size))
			break;

		if (next_index >= msize())
			return FAIL;

		current_block = read_memory_block_pointer_from_memory(next_index);
		current_index = next_index;
		next_index = byte_int_to_int(read_int_from_memory(current_index));
	}

	uint32_t new_block_index = next_index - size - MEMORY_BLOCK_POINTER_SIZE;
	write_int_to_memory(new_block_index, int_to_byte_int(next_index));
	mwrite(new_block_index + INTEGER_SIZE, 1);

	current_block.next = int_to_byte_int(new_block_index);
	write_memory_block_pointer_to_memory(current_index, current_block);
	return new_block_index + MEMORY_BLOCK_POINTER_SIZE;
}

int my_free(unsigned int addr)
{
	if (msize() < MEMORY_BLOCK_POINTER_SIZE || addr >= msize() || addr <= MEMORY_BLOCK_POINTER_SIZE)
		return FAIL;

	uint32_t current_index = 0;
	struct memory_block_pointer current_block = read_memory_block_pointer_from_memory(current_index);

	while (1)
	{
		uint32_t first_byte = current_index + MEMORY_BLOCK_POINTER_SIZE;
		if (first_byte > addr)
			return FAIL;

		if (first_byte == addr)
		{
			current_block.allocated = 0;
			write_memory_block_pointer_to_memory(current_index, current_block);
			return OK;
		}

		uint32_t next_index = byte_int_to_int(read_int_from_memory(current_index));

		if (next_index >= msize())
			return FAIL;

		current_block = read_memory_block_pointer_from_memory(next_index);
		current_index = next_index;
	}
}
