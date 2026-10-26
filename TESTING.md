# CV4 verification

The implementation uses USART2 at 115200 baud, 8 data bits, no parity,
one stop bit, no flow control. RX is DMA1 channel 6 in normal mode with
128 bytes; TX is DMA1 channel 7. HT, TC and IDLE events process newly
received bytes. At IDLE, fewer than 20 remaining bytes triggers rearming;
TC also rearms a full buffer. Send no more than 20 characters per burst,
as required by the assignment, with an idle gap between bursts.

## Local checks

Run `python3 tests/test_logic.py` from the project directory. This compiles
the actual parser and reception function with simulated DMA registers and
checks fragmented frames, consecutive frames, ignored input, the length
boundary, queue overflow, byte ordering and buffer rearming.
These checks do not validate physical interrupt timing or the USB connection.

## Board test

1. Refresh the project in CubeIDE (F5), build, and run it on the Nucleo.
2. Connect through the ST-LINK USB connector and identify `/dev/ttyACM*`.
3. Open Minicom using the detected port, e.g. `minicom -D /dev/ttyACM0 -b 115200`.
   Set 8N1 and disable hardware and software flow control (Ctrl+A, O,
   Serial port setup). Close any other program using the same serial port.
4. Every two seconds, expect a line like:
   `Buffer capacity: 128 bytes, occupied memory: 0 bytes, load [in %]: 0.0%`
5. Send `#Abc12Z$`. Expect:
   `Valid string: Abc12Z, lower-case: 2, upper-case: 2`
6. Send ordinary text without `#`: no valid-string result should appear.
7. Send `#Ab`, pause, then `c$`: expect `Abc`, lower-case 2, upper-case 1.
8. Send `#a$#B$`: expect two separate result lines.
9. Send `#`, then 34 letters in bursts of at most 20, then `$`: accepted.
   Repeat with 35 letters before `$`: discarded. `#Ok$` must work afterwards.
10. Repeat short messages to cross the half-buffer point and the rearm
    threshold; verify replies are neither duplicated nor missing.

A new `#` restarts an incomplete frame. The `$` terminator must appear
within the first 35 characters after `#`, allowing up to 34 payload bytes.
ASCII a-z and A-Z are counted; digits and punctuation are retained but not
counted. The example count in the starter comment is inaccurate:
`Platn15uborZnakov` has 12 lowercase and 2 uppercase letters.

Replies use a 31-message usable queue and a persistent TX buffer. Continuous
input can produce more reply bytes than the serial link can transmit; if
that queue fills, `dropped_messages` increments (inspect in the debugger).
For the terminal test, wait for replies between bursts. Buffer occupancy
reports the DMA write position, including bytes already processed, until
DMA is rearmed. Percentage formatting uses integer arithmetic and one
decimal place, so float printf support is unnecessary.
