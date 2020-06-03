#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <clocale>

typedef unsigned char uchar;
typedef unsigned long ulong;
using namespace std;
//----------------------------------------------------------------------------
//��������� ������ � ������

typedef struct bfile
{
	FILE *file;
	uchar mask;
	int rack;
	int pacifier_counter;
}
BFILE;

#define PACIFIER_COUNT 2047

BFILE *OpenInputBFile(char *name);
BFILE *OpenOutputBFile(char *name);
void  WriteBit(BFILE *bfile, int bit);
void  WriteBits(BFILE *bfile, ulong code, int count);
int   ReadBit(BFILE *bfile);
ulong ReadBits(BFILE *bfile, int bit_count);
void  CloseInputBFile(BFILE *bfile);
void  CloseOutputBFile(BFILE *bfile);

//----------------------------------------------------------------------------
// ������� �������� ������

void CompressFile(FILE *input, BFILE *output);
void ExpandFile(BFILE *input, FILE *output);
void usage_exit(char *prog_name);
void print_ratios(char *input, char *output);
long file_size(char *name);

//----------------------------------------------------------------------------
// ������� ������ � ������� ������ ��� ��������� LZSS

void InitTree(int r);
void ContractNode(int old_node, int new_node);
void ReplaceNode(int old_node, int new_node);
int  FindNextNode(int node);
void DeleteString(int p);
int  AddString(int new_node, int *match_position);

//----------------------------------------------------------------------------
// ���������, ������������ ��� ������ LZSS

// ���������� ����� � ����, ���������� �������� ����������
#define INDEX_BITS  12
// ���������� ����� � ����, ���������� ����� ����������
#define LENGTH_BITS  4
// ����� ����������� ���� � ������
#define WINDOW_SIZE  ( 1 << INDEX_BITS)  //4096 ��� INDEX_BITS = 12
// ������������ ����� ����������
#define RAW_LOOK_AHEAD_SIZE  ( 1 << LENGTH_BITS)
// ���������, ������������ ������ �����������
#define BREAK_EVEN  (( 1 + INDEX_BITS + LENGTH_BITS) / 9)
// ������������ ����� ���������� � ������ ����, ��� �����
// 0 � 1 �� ������������
#define LOOK_AHEAD_SIZE  ( RAW_LOOK_AHEAD_SIZE + BREAK_EVEN)
// ������������ ���� - ������ ��������� ������
#define TREE_ROOT  WINDOW_SIZE
// ����������� ��������� ����� �����
#define END_OF_STREAM  0
// ����������� ����� ����
#define UNUSED  0
// �����, ����������� �������������� ������� ��� ������ �
// ��������� �������
#define MODULO(a)  ( (a) & (WINDOW_SIZE - 1) )

char *CompressionName = "����� LZSS";
char *Usage = "�������_���� ��������_����\n\n";

//----------------------------------------------------------------------------
// ��������� ��������� ������ ��� ������ ���������

void fatal_error(char *str, ...)
{
	printf("������: %s\n", str);
	exit(1);
}

//----------------------------------------------------------------------------
// �������� ����� ��� ��������� ������

BFILE *OpenOutputBFILE(char *name)
{
	BFILE *bfile;

	bfile = (BFILE *)calloc(1, sizeof (BFILE));
	bfile->file = fopen(name, "wb");
	bfile->rack = 0;
	bfile->mask = 0x80;
	bfile->pacifier_counter = 0;
	return bfile;
}

//----------------------------------------------------------------------------
// �������� ����� ��� ���������� ������

BFILE *OpenInputBFile(char *name)
{
	BFILE *bfile;

	bfile = (BFILE *)calloc(1, sizeof (BFILE));
	bfile->file = fopen(name, "rb");
	bfile->rack = 0;
	bfile->mask = 0x80;
	bfile->pacifier_counter = 0;
	return bfile;
}

//----------------------------------------------------------------------------
// �������� ����� ��� ��������� ������

void CloseOutputBFile(BFILE *bfile)
{
	if (bfile->mask != 0x80)
		putc(bfile->rack, bfile->file);
	fclose(bfile->file);
	free((char *)bfile);
}

//----------------------------------------------------------------------------
// �������� ����� ��� ���������� ������

void CloseInputBFile(BFILE *bfile)
{
	fclose(bfile->file);
	free((char *)bfile);
}

//----------------------------------------------------------------------------
// ����� ������ ����

void WriteBit(BFILE *bfile, int bit)
{
	if (bit)
		bfile->rack |= bfile->mask;
	bfile->mask >>= 1;
	if (bfile->mask == 0)
	{
		putc(bfile->rack, bfile->file);
		if ((bfile->pacifier_counter++ & PACIFIER_COUNT) == 0)
			putc('.', stdout);
		bfile->rack = 0;
		bfile->mask = 0x80;
	}
}

//----------------------------------------------------------------------------
// ����� ���������� �����

void WriteBits(BFILE *bfile, ulong code, int count)
{
	ulong mask;

	mask = 1L << (count - 1);
	while (mask != 0)
	{
		if (mask & code)
			bfile->rack |= bfile->mask;
		bfile->mask >>= 1;
		if (bfile->mask == 0)
		{
			putc(bfile->rack, bfile->file);
			if ((bfile->pacifier_counter++ & PACIFIER_COUNT) == 0)
				putc('.', stdout);
			bfile->rack = 0;
			bfile->mask = 0x80;
		}
		mask >>= 1;
	}
}

//----------------------------------------------------------------------------
// ���� ������ ����

int ReadBit(BFILE *bfile)
{
	int value;

	if (bfile->mask == 0x80)
	{
		bfile->rack = getc(bfile->file);
		if (bfile->rack == EOF)
			fatal_error("������ � ��������� ReadBit!\n");
		if ((bfile->pacifier_counter++ & PACIFIER_COUNT) == 0)
			putc('.', stdout);
	}
	value = bfile->rack & bfile->mask;
	bfile->mask >>= 1;
	if (bfile->mask == 0)
		bfile->mask = 0x80;
	return (value ? 1 : 0);
}	

//----------------------------------------------------------------------------
// ���� ���������� �����

ulong ReadBits(BFILE *bfile, int bit_count)
{
	ulong mask;
	ulong return_value;
			
	mask = 1L << (bit_count - 1);
	return_value = 0;
	while (mask != 0)
	{
		if (bfile->mask == 0x80)
		{
			bfile->rack = getc(bfile->file);
			if (bfile->rack == EOF)
				fatal_error("������ � ��������� ReadBits!\n");
			if ((bfile->pacifier_counter++ & PACIFIER_COUNT) == 0)
				putc('.', stdout);
		}
		if (bfile->rack & bfile->mask)
			return_value |= mask;
		mask >>= 1;
		bfile->mask >>= 1;
		if (bfile->mask == 0)
			bfile->mask = 0x80;
	}
	return return_value;
}

void MainCompressFunc()
{

	char InputPath[1000];
	char OutputPath[1000];
	int add = 0;
	BFILE *output;
	FILE  *input;
	printf("������� ����� �����:\n");
	fflush(stdin);
	cin >> InputPath;
	printf("������� ����� ������\n");
	fflush(stdin);
	cin >> OutputPath;
	
	input = fopen(InputPath, "rb");
	if (input == NULL)
		fatal_error("������ ��� �������� ����� ��� �����\n");

	output = OpenOutputBFILE(OutputPath);
	if (output == NULL)
		fatal_error("������ ��� �������� ����� ��� ������\n");

	printf("\n������ %s � %s\n", InputPath, OutputPath);
	printf("��������� %s\n", CompressionName);
	CompressFile(input, output);
	CloseOutputBFile(output);
	fclose(input);
	print_ratios(InputPath, OutputPath);
}

void MainExpandFunc()
{
	
	char InputPath[1000];
	char OutputPath[1000];
	int add = 0;
	FILE *output;
	BFILE  *input;
	printf("������� ����� ������\n");
	fflush(stdin);
	cin >> InputPath;	
	printf("������� ����� ��������� �����\n");
	cin >> OutputPath;
	input = OpenInputBFile(InputPath);

	if (input == NULL)
		fatal_error("������ ��� �������� ����� ��� �����\n");
	output = fopen(OutputPath, "wb");
	if (output == NULL)
		fatal_error("������ ��� �������� ����� ��� ������\n");
	printf("\n������������ %s � %s\n", InputPath, OutputPath);
	printf("��������� %s\n", CompressionName);

	ExpandFile(input, output);
	CloseOutputBFile(input);
	fclose(output);
}

void About()
{
	system("cls");
	printf("��� �������� ������ �������� ��.830501\n������ ������ �������������\n��������� ������ Windows ���������� LZSS\n");
	_getch();
	system("cls");
}
//----------------------------------------------------------------------------
// �������� ��������� (������ ������)

int main()
{
	
	char choice;
	
	setlocale(LC_ALL, "Russian");
	setbuf(stdout, NULL);
	setlocale(LC_ALL, "Russian");
	do
	{

		printf("\n1-����� ����� \n2-���������� \n3-� ��������� \n4-�����\n");
		fflush(stdin);
		choice = getchar();

		switch (choice)
		{
		case '1':
			MainCompressFunc();
			break;

		case '2':
			MainExpandFunc();
			break;

		case '3':
			About();
			break;

		case '4':
			exit(0);

		default:
			break;
		}
	} while (true);

	return 0;
}

//----------------------------------------------------------------------------
// ����� ��������� �� �������������

void usage_exit(char *prog_name)
{
	char *short_name;
	char *extension;

	short_name = strrchr(prog_name, '\\');
	if (short_name == NULL)
		short_name = strrchr(prog_name, '/');
	if (short_name == NULL)
		short_name = strrchr(prog_name, ':');
	if (short_name != NULL)
		short_name++;
	else
		short_name = prog_name;
	extension = strrchr(short_name, '.');
	if (extension != NULL)
		*extension = '\0';
	printf("\n���������� �������������: %s %s\n",
		short_name, Usage);
	exit(0);
}

//----------------------------------------------------------------------------
// ��������� ����� �����

long file_size(char *name)
{
	long eof_ftell;
	FILE *file;

	file = fopen(name, "r");
	if (file == NULL)
		return (0L);
	fseek(file, 0L, SEEK_END);
	eof_ftell = ftell(file);
	fclose(file);
	return eof_ftell;
}

//----------------------------------------------------------------------------
// ����� ��������� � ����������� �������� ������

void print_ratios(char *input, char *output)
{
	long input_size;
	long output_size;
	int ratio;

	input_size = file_size(input);
	if (input_size == 0)
		input_size = 1;
	output_size = file_size(output);
	ratio = (int)(output_size * 100L / input_size);
	printf("\n������ �������� ����� (����): %ld\n",
		input_size);
	printf("������ ��������� ����� (����): %ld\n",
		output_size);
	if (output_size == 0)
		output_size = 1;
	printf("������� ������: %d%%\n", ratio);
}

//----------------------------------------------------------------------------
// ����� �������� ����� ���������� ��������� LZSS

// ���������� ����
uchar window[WINDOW_SIZE];

// ������� ������ ������ ����������
struct
{
	int parent;
	int smaller_child;
	int larger_child;
}
tree[WINDOW_SIZE + 1];

//----------------------------------------------------------------------------
// ������������� ��������� ������ ������

void InitTree(int r)
{
	tree[TREE_ROOT].larger_child = r;
	tree[r].parent = TREE_ROOT;
	tree[r].larger_child = UNUSED;
	tree[r].smaller_child = UNUSED;
}

//----------------------------------------------------------------------------
// ��������� �������� ����

void ContractNode(int old_node, int new_node)
{
	tree[new_node].parent = tree[old_node].parent;
	if (tree[tree[old_node].parent].larger_child == old_node)
		tree[tree[old_node].parent].larger_child = new_node;
	else
		tree[tree[old_node].parent].smaller_child = new_node;
	tree[old_node].parent = UNUSED;
}

//----------------------------------------------------------------------------
// ��������� �������� ����

void ReplaceNode(int old_node, int new_node)
{
	int parent;

	parent = tree[old_node].parent;
	if (tree[parent].smaller_child == old_node)
		tree[parent].smaller_child = new_node;
	else
		tree[parent].larger_child = new_node;
	tree[new_node] = tree[old_node];
	tree[tree[new_node].smaller_child].parent = new_node;
	tree[tree[new_node].larger_child].parent = new_node;
	tree[old_node].parent = UNUSED;
}

//----------------------------------------------------------------------------
// ����� ���������� ��������, ��� ���������, ����

int FindNextNode(int node)
{
	int next;

	next = tree[node].smaller_child;
	while (tree[next].larger_child != UNUSED)
		next = tree[next].larger_child;
	return next;
}

//----------------------------------------------------------------------------
// �������� ������ �� ��������� ������ ������

void DeleteString(int p)
{
	int replacement;

	if (tree[p].parent == UNUSED)
		return;
	if (tree[p].larger_child == UNUSED)
		ContractNode(p, tree[p].smaller_child);
	else
	if (tree[p].smaller_child == UNUSED)
		ContractNode(p, tree[p].larger_child);
	else
	{
		replacement = FindNextNode(p);
		DeleteString(replacement);
		ReplaceNode(p, replacement);
	}
}

//----------------------------------------------------------------------------
// ���������� ������ � �������� ������ ������

int AddString(int new_node, int *match_pos)
{
	int i;
	int test_node;
	int delta;
	int match_len;
	int *child;

	if (new_node == END_OF_STREAM)
		return (0);

	test_node = tree[TREE_ROOT].larger_child;
	match_len = 0;

	for (;;)
	{
		for (i = 0; i < LOOK_AHEAD_SIZE; i++)
		{
			delta = window[MODULO(new_node + i)] -
				window[MODULO(test_node + i)];
			if (delta != 0)
				break;
		}

		if (i >= match_len)
		{
			match_len = i;
			*match_pos = test_node;
			if (match_len >= LOOK_AHEAD_SIZE)
			{
				ReplaceNode(test_node, new_node);
				return match_len;
			}
		}

		if (delta >= 0)	
			child = &tree[test_node].larger_child;
		else
			child = &tree[test_node].smaller_child;

		if (*child == UNUSED)
		{
			*child = new_node;	
			tree[new_node].parent = test_node;
			tree[new_node].larger_child = UNUSED;
			tree[new_node].smaller_child = UNUSED;
			return match_len;
		}
		test_node = *child;
	}
}

//----------------------------------------------------------------------------
// ��������� ������ �����

void CompressFile(FILE *input, BFILE *output)
{
	int i;
	int c;
	int look_ahead_bytes;
	int current_pos;
	int replace_count;
	int match_len;
	int match_pos;

	current_pos = 1;

	for (i = 0; i < LOOK_AHEAD_SIZE; i++)
	{
		if ((c = getc(input)) == EOF)
			break;
		window[current_pos + i] = (uchar)c;
	}

	look_ahead_bytes = i;
	InitTree(current_pos);
	match_len = 0;
	match_pos = 0;

	while (look_ahead_bytes > 0)
	{
		if (match_len > look_ahead_bytes)
			match_len = look_ahead_bytes;
		if (match_len <= BREAK_EVEN)
		{
			replace_count = 1;
			WriteBit(output, 1);
			WriteBits(output, (ulong)window[current_pos], 8);
		}
		else
		{
			WriteBit(output, 0);
			WriteBits(output, (ulong)match_pos, INDEX_BITS);
			WriteBits(output, (ulong)(match_len - (BREAK_EVEN + 1)),
				LENGTH_BITS);
			replace_count = match_len;
		}

		for (i = 0; i < replace_count; i++)
		{
			DeleteString(MODULO(current_pos + LOOK_AHEAD_SIZE));
			if ((c = getc(input)) == EOF)
				look_ahead_bytes--;
			else
				window[MODULO(current_pos + LOOK_AHEAD_SIZE)] = (uchar)c;
			current_pos = MODULO(current_pos + 1);
			if (look_ahead_bytes)
				match_len = AddString(current_pos, &match_pos);
		}
	}

	WriteBit(output, 0);
	WriteBits(output, (ulong)END_OF_STREAM, INDEX_BITS);
}

//----------------------------------------------------------------------------
// ��������� ������������� ������� �����

void ExpandFile(BFILE *input, FILE *output)
{
	int i;
	int current_pos;
	int c;
	int match_len;
	int match_pos;

	current_pos = 1;

	for (;;)
	{
		if (ReadBit(input))
		{
			c = (int)ReadBits(input, 8);
			putc(c, output);
			window[current_pos] = (uchar)c;
			current_pos = MODULO(current_pos + 1);
		}
		else
		{
			match_pos = (int)ReadBits(input, INDEX_BITS);
			if (match_pos == END_OF_STREAM)
				break;
			match_len = (int)ReadBits(input, LENGTH_BITS);
			match_len += BREAK_EVEN;
			for (i = 0; i <= match_len; i++)	
			{
				c = window[MODULO(match_pos + i)];
				putc(c, output);
				window[current_pos] = (uchar)c;
				current_pos = MODULO(current_pos + 1);
			}
		}
	}
}
