"""Compile the actual parser and RX handler against host-side DMA stubs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'Src/main.c').read_text()
usart = (root / 'Src/usart.c').read_text()
def function(source, signature):
    start = source.index(signature + '\n{')
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

code = r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define __DMB() ((void)0)
typedef struct { uint8_t small_letter, capital_letter; } letter_count_;
'''
code += main[main.index('#define MESSAGE_LIMIT'):main.index('int main(void)')]
code += function(main, 'void proccesDmaData(uint8_t sign)')
code += r'''
#define DMA_USART2_BUFFER_SIZE 128
#define DMA1 0
#define USART2 0
#define LL_DMA_CHANNEL_6 6
static uint16_t mock_remaining = 128;
static int idle, enabled = 1, restarts;
uint8_t bufferUSART2dma[128];
static uint8_t received[4096];
static unsigned received_count;
static void collect(uint8_t value) { received[received_count++] = value; }
static void (*USART2_ProcessData)(uint8_t) = collect;
#define LL_DMA_GetDataLength(a,b) mock_remaining
#define LL_USART_IsActiveFlag_IDLE(a) idle
#define LL_DMA_DisableChannel(a,b) (enabled = 0)
#define LL_DMA_ClearFlag_GI6(a) ((void)0)
#define LL_DMA_SetMemoryAddress(a,b,c) ((void)0)
#define LL_DMA_SetDataLength(a,b,c) (mock_remaining = (c))
#define LL_DMA_EnableChannel(a,b) (enabled = 1, restarts++)
'''
code += function(usart, 'void USART2_CheckDmaReception(void)')
code += r'''
static void feed(const char *s) { while (*s) proccesDmaData((uint8_t)*s++); }
static void expect(const char *s, unsigned lower, unsigned upper) {
    assert(message_read != message_write);
    received_message *m = &messages[message_read];
    assert(strcmp(m->text, s) == 0);
    assert(m->count.small_letter == lower && m->count.capital_letter == upper);
    message_read = (message_read + 1U) % MESSAGE_QUEUE_SIZE;
}
static void burst(unsigned count, unsigned *sequence) {
    for (unsigned i = 0; i < count; ++i) {
        assert(enabled && mock_remaining);
        bufferUSART2dma[128 - mock_remaining--] = (uint8_t)(*sequence)++;
        idle = 0;
        if (mock_remaining == 64 || mock_remaining == 0) USART2_CheckDmaReception();
    }
    idle = 1;
    USART2_CheckDmaReception();
    USART2_CheckDmaReception(); /* Repeated IRQ must not duplicate bytes. */
}
int main(void) {
    feed("ignored$"); assert(message_read == message_write);
    feed("#Ab"); assert(message_read == message_write);
    feed("c12Z$"); expect("Abc12Z", 2, 2);
    feed("#a$#B$#$"); expect("a",1,0); expect("B",0,1); expect("",0,0);
    feed("#old#New$"); expect("New",2,1);
    feed("#"); for (int i=0;i<34;i++) feed("a"); feed("$");
    expect("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",34,0);
    feed("#"); for (int i=0;i<35;i++) feed("a"); feed("$");
    assert(message_read == message_write);
    feed("#Ok$"); expect("Ok",1,1);
    for (unsigned i=0;i<MESSAGE_QUEUE_SIZE;i++) feed("#a$");
    assert(dropped_messages == 1);
    for (unsigned i=0;i<MESSAGE_QUEUE_SIZE-1;i++) expect("a",1,0);
    unsigned sequence=0;
    for (unsigned i=0;i<80;i++) burst(1+i%20,&sequence);
    assert(received_count == sequence && restarts > 0);
    for (unsigned i=0;i<sequence;i++) assert(received[i] == (uint8_t)i);
    /* Exercise full-buffer fallback as well as IDLE-based early rearming. */
    idle=0;
    while (mock_remaining) {
        bufferUSART2dma[128-mock_remaining--]=(uint8_t)sequence++;
    }
    USART2_CheckDmaReception();
    assert(mock_remaining==128 && received_count==sequence);
    for (unsigned i=0;i<sequence;i++) assert(received[i] == (uint8_t)i);
    puts("PASS: parser boundaries, fragmented/multiple messages, queue full, RX ordering and rearming");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Wno-unused-variable',
                     str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
