#include "lj_def.h"
#include "lj_arch.h"
#if LJ_64 && !LJ_GC64
#include "xr_alloc.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef long (*PNTAVM)(HANDLE handle, void **addr, ULONG zbits,
		       size_t *size, ULONG alloctype, ULONG prot);
extern PNTAVM ntavm;
/* Number of top bits of the lower 32 bits of an address that must be zero.
** Apparently 0 gives us full 64 bit addresses and 1 gives us the lower 2GB.
*/
#define NTAVM_ZEROBITS		1

#define MAX_SIZE_T		(~(size_t)0)
#define MFAIL			((void *)(MAX_SIZE_T))

// Ëóàäæèò âûäåëÿåò ïàìÿòü êóñêàìè, êðàòíûìè 128Ê
#define CHUNK_SIZE (128 * 1024)
#define CHUNK_COUNT 1024
static int inited = 0;
void* g_heap;
char g_heapMap[CHUNK_COUNT + 1];
char* g_firstFreeChunk;
char* find_free(int size);

//#define DEBUG_MEM
#ifdef DEBUG_MEM
static char buf[100];
void dump_map(void* ptr, size_t size, char c);
#endif

void XR_INIT()
{
	if (inited)
		return;
	g_heap = NULL;
	size_t size = CHUNK_SIZE * CHUNK_COUNT;
	long st = ntavm(INVALID_HANDLE_VALUE, &g_heap, NTAVM_ZEROBITS, &size,
	                MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	for (int i = 0; i < CHUNK_COUNT; i++)
		g_heapMap[i] = 'x';
	g_heapMap[CHUNK_COUNT] = '\0';
	g_firstFreeChunk = g_heapMap;

#ifdef DEBUG_MEM	
	sprintf(buf, "XR_INIT create_block %p result=%X\r\n", g_heap, st);
	OutputDebugString(buf);
#endif
	inited = 1;
}

void* XR_MMAP(size_t size)
{
#ifdef DEBUG_MEM
	sprintf(buf, "XR_MMAP(%Iu)", size);
	OutputDebugString(buf);
#endif
	int chunks = size / CHUNK_SIZE;
	char* s = find_free(chunks);
	void* ptr = MFAIL;
	if (s != NULL) {
		ptr = (char*)g_heap + CHUNK_SIZE * (s - g_heapMap);
		for (int i = 0; i < chunks; i++)
			s[i] = 'a' + chunks - 1;
		if (s == g_firstFreeChunk)
			g_firstFreeChunk = find_free(1);
	}
#ifdef DEBUG_MEM
	sprintf(buf, " ptr=%p chunks %d\r\n", ptr, chunks);
	OutputDebugString(buf);
	dump_map(ptr, size, 'U');
#endif
	return ptr;
}

// Ñóäÿ ïî êîììåíòàðèþ ê CALL_MUNMAP, ëóàäæèò ìîæåò îáúåäèíÿòü âûäåëåííûå åìó ÷àíêè
// è îñâîáîæäàòü èõ êàê îäèí. Íàäåþñü îí íå ñëåïèò âìåñòå ÷àíêè èç ðàçíûõ ïóëîâ
void XR_DESTROY(void* ptr, size_t size)
{
#ifdef DEBUG_MEM
	sprintf(buf, "XR_DESTROY(ptr=%p, size=%Iu)", ptr, size);
	OutputDebugString(buf);
#endif
	char* s = g_heapMap + ((char*)ptr - (char*)g_heap) / CHUNK_SIZE;
	int count = size / CHUNK_SIZE;
	for (int i = 0; i < count; i++)
		s[i] = 'x';
	if (s < g_firstFreeChunk)
		g_firstFreeChunk = s;
#ifdef DEBUG_MEM	
	dump_map(ptr, size, 'X');
#endif
}

// Íàõîäèò ïîäðÿä èäóùèå ñâîáîäíûå ÷àíêè êîëè÷åñòâîì size, íà÷èíàÿ ñ ïåðâîãî ñâîáîäíîãî
// Âîçâðàùàåò óêàçàòåëü íà ãðóïïó èç heapMap èëè NULL, åñëè íàéòè íå óäàëîñü
char* find_free(int size)
{
	char* p = g_firstFreeChunk;
	int count = 0;
	while (*p != '\0') {
		if (*p == 'x')
			count++;
		else
			count = 0;
		p++;
		if (count == size)
			return p - count;
	}
	return NULL;
}

static 	char temp[1025];
void dump_map(void* ptr, size_t size, char c)
{
#ifdef DEBUG_MEM
	OutputDebugString("heap:\r\n|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------\r\n");
	strcpy(temp, g_heapMap);
	char *cur = temp + ((char*)ptr - (char*)g_heap) / CHUNK_SIZE;
	for (int i = 0; i < size / CHUNK_SIZE; i++)
		cur[i] = c;

	for (int i = 0; i < 8; i++)
	{
		char a = temp[i * 128 + 128];
		temp[i * 128 + 128] = '\0';
		OutputDebugString(temp + i * 128);
		temp[i * 128 + 128] = a;
		OutputDebugString("\r\n");
	}
	OutputDebugString("--------------------------------------------------------------------------------------------------------------------------------\r\n");
#endif
} 
#endif