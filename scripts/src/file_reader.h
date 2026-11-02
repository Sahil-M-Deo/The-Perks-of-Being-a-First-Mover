#pragma once
#include "common.h"

/*
   Reads a .pgn or .pgn.zst file line by line. A month of Lichess games is tens of GB
   compressed, so instead of unzipping it to disk we unzip it in memory, 1 MB at a time.

   How the zstd library works:
     - ZSTD_DCtx is the decompressor. It remembers where it is between calls, so we
       can feed it the file in pieces.
     - ZSTD_inBuffer holds compressed bytes from the file, ZSTD_outBuffer is where
       the unzipped text goes. zstd moves their .pos forward as it reads and writes.
     - ZSTD_decompressStream() unzips as much as it can from input to output.
*/
class FileReader {
    FILE *file;
    bool compressed;
    ZSTD_DCtx *zstd = nullptr;
    vector<char> inBuffer, outBuffer;
    ZSTD_inBuffer input;
    string buffer;
    int pos = 0;
    bool finished = false;

    bool readPlainChunk(string &chunk) {
        int n = fread(outBuffer.data(), 1, outBuffer.size(), file);
        if (n<=0) return false;
        chunk.assign(outBuffer.data(), n);
        return true;
    }

    bool readZstdChunk(string &chunk) {
        while (true) {
            if (input.pos==input.size) {
                int n = fread(inBuffer.data(), 1, inBuffer.size(), file);
                if (n<=0) return drainZstd(chunk);
                input.src = inBuffer.data();
                input.size = n;
                input.pos = 0;
            }
            ZSTD_outBuffer output = {outBuffer.data(), outBuffer.size(), 0};
            ZSTD_decompressStream(zstd, &output, &input);
            // it sometimes needs more input before it can write anything
            if (output.pos>0) {
                chunk.assign(outBuffer.data(), output.pos);
                return true;
            }
        }
    }

    // at the end of the file zstd may still hold some unzipped text, so ask for it
    bool drainZstd(string &chunk) {
        ZSTD_outBuffer output = {outBuffer.data(), outBuffer.size(), 0};
        ZSTD_inBuffer empty = {nullptr, 0, 0};
        ZSTD_decompressStream(zstd, &output, &empty);
        if (output.pos==0) return false;
        chunk.assign(outBuffer.data(), output.pos);
        return true;
    }

public:
    FileReader(string path) {
        file = fopen(path.c_str(), "rb");
        compressed = path.size()>4 && path.substr(path.size() - 4)==".zst";
        outBuffer.resize(1 << 20);
        if (compressed) {
            zstd = ZSTD_createDCtx();
            // Lichess compresses with a bigger "window" than zstd allows by default
            ZSTD_DCtx_setParameter(zstd, ZSTD_d_windowLogMax, 31);
            inBuffer.resize(1 << 20);
            input = {inBuffer.data(), 0, 0};
        }
    }

    ~FileReader() {
        if (zstd) ZSTD_freeDCtx(zstd);
        if (file) fclose(file);
    }

    bool readLine(string &line) {
        while (true) {
            int newline = buffer.find('\n', pos);
            if (newline!=string::npos) {
                line = buffer.substr(pos, newline - pos);
                pos = newline + 1;
                return true;
            }
            if (finished) {
                if (pos==(int)buffer.size()) return false;
                line = buffer.substr(pos);
                pos = buffer.size();
                return true;
            }
            // keep the half line that's left and read more after it
            buffer = buffer.substr(pos);
            pos = 0;
            string chunk;
            bool got = compressed ? readZstdChunk(chunk) : readPlainChunk(chunk);
            if (got) buffer += chunk;
            else finished = true;
        }
    }
};
