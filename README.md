# MemDiff

## Overview

Compares the differences in memory using a traditional memory check, which is to generate a checksum for each memory page (or an entire region) and then record it. After the checksums are generated they are compared with checksums that are consistently generated throughout the process's lifespan - at least that is a common implementation. However, this contrasts in the sense that it generates a checksum to record, but it also takes a snapshot of all the pages and records and then parses the data into code to be used as macros. 

## Implementation

The implementation is that it scans only the checksums, when a checksum mismatch occurs, the page of the mismatch is flagged and then a deep scan is invoked on the page which then extracts where the data was changed and also compares it against the page on record, which shows what the data should be as well. When both bits of data are extracted, it outputs the changes and then generates a C-compatible function "macro" which reflects the changes as well as a macro that undos the changes. Here is an example of this memory differentiation and code generation:

![alt_text](https://i.imgur.com/LKUYCqd.png)

In the above screenshot, it generates the code behind a game modification software without reverse engineering it. It does this by first pressing the button on that software to enable flying in the game. MemDiff then captures that change and reflects it in both a list and generated code. It builds the code that enabled and disabled flying. It generates macros "on the fly".

## TODO:

- Format code generation as hexadecimal instead of decimal
- Use image_base + offset style addresses instead of a pure VA
- Make use of PageEval flag
- Etc...
