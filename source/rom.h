// SPDX-License-Identifier: GPL-3.0-only
#ifndef BB_ROM_H
#define BB_ROM_H

enum {
    ROM_IMPORT_OK = 1,
    ROM_IMPORT_MISSING = 0,
    ROM_IMPORT_UNSUPPORTED = -1,
    ROM_IMPORT_IO = -2,
    ROM_IMPORT_INVALID = -3,
    ROM_IMPORT_NOMEM = -4
};

/* Search beside pack_path for a supported .nds file and prepare the cache. */
int rom_import(const char *pack_path);
int rom_extract(const char *rom_path, const char *pack_path);

#endif
