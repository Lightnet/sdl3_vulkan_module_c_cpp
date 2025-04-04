#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL_log.h>
#include "vsdl_utils.h"

char* readFile(const char* filename, size_t* outSize) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open shader file: %s", filename);
        *outSize = 0;
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(fileSize);
    if (!buffer) {
        fclose(file);
        *outSize = 0;
        return NULL;
    }
    fread(buffer, 1, fileSize, file);
    fclose(file);
    *outSize = fileSize;
    return buffer;
}